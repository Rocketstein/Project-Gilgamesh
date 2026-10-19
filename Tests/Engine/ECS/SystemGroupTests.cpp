#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Engine/ECS/Framework/World.h"
#include "Engine/ECS/System/Core/SystemGroup.h"
#include "Tests/TestFramework.h"

namespace
{
// Records lifecycle, execution, and destruction while allowing callback-driven checks.
class RecordingSystem final : public ISystem
{
public:
	RecordingSystem(std::vector<std::string>& events, std::string name)
		: events_(events), name_(std::move(name))
	{
	}

	~RecordingSystem() override { Record("delete"); }

	void OnCreate() override
	{
		Record("create");
		if (onCreate)
			onCreate();
	}

	void OnEnable() override { Record("enable"); }
	void OnDisable() override { Record("disable"); }

	void OnDestroy(World& world) override
	{
		Record("destroy");
		if (onDestroy)
			onDestroy();
	}

	void Update(World& world, float deltaTime) override
	{
		Record("update");
		if (onUpdate)
			onUpdate(world, deltaTime);
	}

private:
	// Keep expectations readable by storing one named event per callback.
	void Record(const char* event) { events_.push_back(name_ + "." + event); }

public:
	std::function<void()> onCreate; // Optional registration-time mutation probe.
	std::function<void()> onDestroy; // Optional failing teardown hook.
	std::function<void(World&, float)> onUpdate; // Optional execution probe.

private:
	std::vector<std::string>& events_; // Test-owned event history outlives the system.
	std::string name_; // Identifies the system in expected callback sequences.
};
} // Anonymous namespace

GILGAMESH_TEST("Engine.ECS.SystemGroup", "OwnsSystemsAndExecutesRegistrationOrder")
{
	World world;
	std::vector<std::string> events;
	SystemGroup group;

	// Enable requested children before creation; root activation remains independent.
	auto first = std::make_unique<RecordingSystem>(events, "first");
	auto second = std::make_unique<RecordingSystem>(events, "second");
	first->SetEnabled(true);
	second->SetEnabled(true);
	ISystem& firstRef = group.AddSystem(std::move(first));
	ISystem& secondRef = group.AddSystem(std::move(second));
	group.Initialize();
	group.Initialize();

	const std::vector<std::string> created{ "first.create", "second.create" };
	GILGAMESH_CHECK_MESSAGE(events == created && firstRef.IsInitialized() &&
		firstRef.GetSystemGroup() == &group && secondRef.GetSystemGroup() == &group,
		"creation must happen once in insertion order and bind each system to its owner");

	// A disabled root suppresses execution even when its children requested enablement.
	group.Update(world, 0.25f);
	GILGAMESH_CHECK_MESSAGE(events == created && !firstRef.IsActive(),
		"disabled roots must not activate or execute their children");
	group.SetEnabled(true);
	group.Update(world, 0.25f);
	group.Shutdown(world);
	group.Shutdown(world);

	const std::vector<std::string> expected{
		"first.create", "second.create",
		"first.enable", "second.enable",
		"first.update", "second.update",
		"second.disable", "first.disable",
		"second.destroy", "second.delete",
		"first.destroy", "first.delete"
	};
	GILGAMESH_CHECK_MESSAGE(events == expected && !group.IsInitialized() && !group.IsActive(),
		"updates must follow insertion order and shutdown must disable and release in reverse order");
}

GILGAMESH_TEST("Engine.ECS.SystemGroup", "NestedActivationPreservesRequestedFlags")
{
	World world;
	std::vector<std::string> events;
	SystemGroup root;
	auto nested = std::make_unique<SystemGroup>();
	SystemGroup& nestedRef = *nested;
	auto first = std::make_unique<RecordingSystem>(events, "first");
	auto second = std::make_unique<RecordingSystem>(events, "second");
	ISystem& firstRef = *first;
	ISystem& secondRef = *second;
	first->SetEnabled(true);
	nested->SetEnabled(true);
	nested->AddSystem(std::move(first));
	nested->AddSystem(std::move(second));
	root.AddSystem(std::move(nested));
	root.Initialize();
	root.SetEnabled(true);

	// Parent deactivation keeps the child's requested state for the next activation.
	nestedRef.SetEnabled(false);
	GILGAMESH_CHECK_MESSAGE(firstRef.IsEnabled() && !firstRef.IsActive() &&
		!secondRef.IsEnabled() && !secondRef.IsActive(),
		"parent deactivation must preserve each child's requested enablement");
	nestedRef.SetEnabled(true);
	root.SetEnabled(false);
	root.SetEnabled(true);
	secondRef.SetEnabled(true);
	events.clear();
	root.Update(world, 0.5f);

	const std::vector<std::string> expected{ "first.update", "second.update" };
	GILGAMESH_CHECK_MESSAGE(events == expected && nestedRef.GetSystemGroup() == &root &&
		firstRef.GetSystemGroup() == &nestedRef && firstRef.IsActive() && secondRef.IsActive(),
		"nested groups must propagate root activation and keep explicit child execution order");
	root.Shutdown(world);
}

GILGAMESH_TEST("Engine.ECS.SystemGroup", "InitializesAdditionsAndTearsDownRemoval")
{
	World world;
	std::vector<std::string> events;
	SystemGroup group;
	group.Initialize();
	group.SetEnabled(true);

	// Registration into a live root immediately creates and activates the new member.
	auto system = std::make_unique<RecordingSystem>(events, "member");
	system->SetEnabled(true);
	ISystem& member = group.AddSystem(std::move(system));
	const std::vector<std::string> added{ "member.create", "member.enable" };
	GILGAMESH_CHECK_MESSAGE(events == added && member.IsActive(),
		"adding an enabled system to a running group must complete creation and activation");

	// A foreign object cannot be removed; a member is disabled before destruction.
	RecordingSystem foreign(events, "foreign");
	GILGAMESH_CHECK_MESSAGE(!group.RemoveSystem(foreign, world),
		"removal must reject systems owned by another source");
	GILGAMESH_CHECK_MESSAGE(group.RemoveSystem(member, world),
		"removal must release an owned member outside execution");
	const std::vector<std::string> removed{
		"member.create", "member.enable", "member.disable", "member.destroy", "member.delete"
	};
	GILGAMESH_CHECK_MESSAGE(events == removed,
		"removal must run disable and destruction hooks before releasing ownership");
	group.Shutdown(world);
}

GILGAMESH_TEST("Engine.ECS.SystemGroup", "RejectsMutationDuringExecutionAndAcrossLockedPhases")
{
	std::vector<std::string> events;
	World world;
	SystemGroup& group = world.GetSimulationSystems();
	SystemGroup& otherPhase = world.GetTransformSystems();
	auto actor = std::make_unique<RecordingSystem>(events, "actor");
	auto victim = std::make_unique<RecordingSystem>(events, "victim");
	auto crossVictim = std::make_unique<RecordingSystem>(events, "crossVictim");
	ISystem& victimRef = *victim;
	ISystem& crossVictimRef = *crossVictim;
	bool ownAddRejected = false;
	bool crossAddRejected = false;
	bool removalRejected = false;
	bool crossRemovalRejected = false;

	// Deliberately try every scheduling mutation from a running callback.
	actor->onUpdate = [&](World& runningWorld, float)
	{
		try { group.AddSystem(std::make_unique<RecordingSystem>(events, "rejected")); }
		catch (const std::logic_error&) { ownAddRejected = true; }
		try { otherPhase.AddSystem(std::make_unique<RecordingSystem>(events, "crossRejected")); }
		catch (const std::logic_error&) { crossAddRejected = true; }
		removalRejected = !group.RemoveSystem(victimRef, runningWorld);
		crossRemovalRejected = !otherPhase.RemoveSystem(crossVictimRef, runningWorld);
		victimRef.SetEnabled(false);
		group.SetEnabled(false);
		otherPhase.SetEnabled(false);
		group.Shutdown(runningWorld);
		otherPhase.Shutdown(runningWorld);
	};
	actor->SetEnabled(true);
	victim->SetEnabled(true);
	group.AddSystem(std::move(actor));
	group.AddSystem(std::move(victim));
	otherPhase.AddSystem(std::move(crossVictim));

	// Actual world execution must lock both the running root and inactive phase roots.
	world.Update(0.1f);
	GILGAMESH_CHECK_MESSAGE(ownAddRejected && crossAddRejected && removalRejected && crossRemovalRejected &&
		victimRef.IsEnabled() && group.IsActive() && otherPhase.IsActive() &&
		group.IsInitialized() && otherPhase.IsInitialized(),
		"execution and world-phase locks must reject ownership, enablement, and shutdown mutations");

	// The world releases locks after traversal; callers cannot manipulate the gates.
	GILGAMESH_CHECK_MESSAGE(group.RemoveSystem(victimRef, world) &&
		otherPhase.RemoveSystem(crossVictimRef, world),
		"mutation must be allowed again after the enclosing phase ends");
}

GILGAMESH_TEST("Engine.ECS.SystemGroup", "LifecycleHooksCannotMutateOwnership")
{
	World world;
	std::vector<std::string> events;
	SystemGroup group;
	group.Initialize();
	group.SetEnabled(true);
	auto actor = std::make_unique<RecordingSystem>(events, "actor");
	bool rejected = false;
	actor->onCreate = [&]
	{
		try { group.AddSystem(std::make_unique<RecordingSystem>(events, "rejected")); }
		catch (const std::logic_error&) { rejected = true; }
		group.SetEnabled(false);
	};
	actor->SetEnabled(true);
	ISystem& member = group.AddSystem(std::move(actor));
	GILGAMESH_CHECK_MESSAGE(rejected && group.IsActive() && member.IsActive(),
		"initialization callbacks must not modify membership or deactivate the owning group");
	group.Shutdown(world);
}

GILGAMESH_TEST("Engine.ECS.SystemGroup", "RestoresTraversalAfterFailureAndContinuesTeardown")
{
	World world;
	std::vector<std::string> events;
	SystemGroup group;
	group.Initialize();
	group.SetEnabled(true);
	auto failing = std::make_unique<RecordingSystem>(events, "failing");
	ISystem& failingRef = *failing;
	failing->onUpdate = [](World&, float) { throw std::runtime_error("update failure"); };
	failing->onDestroy = [] { throw std::runtime_error("destroy failure"); };
	failing->SetEnabled(true);
	group.AddSystem(std::move(failing));

	// Execution failures propagate while the traversal guard releases its lock.
	bool threw = false;
	try { group.Update(world, 0.1f); }
	catch (const std::runtime_error&) { threw = true; }
	GILGAMESH_CHECK_MESSAGE(threw && group.RemoveSystem(failingRef, world),
		"failed execution must release its traversal lock and failed teardown must still remove the system");

	// A later registration proves the group remains usable after both failures.
	auto survivor = std::make_unique<RecordingSystem>(events, "survivor");
	survivor->SetEnabled(true);
	group.AddSystem(std::move(survivor));
	group.Update(world, 0.1f);
	group.Shutdown(world);
	GILGAMESH_CHECK_MESSAGE(events.back() == "survivor.delete",
		"remaining systems must execute and release normally after another system fails");
}

