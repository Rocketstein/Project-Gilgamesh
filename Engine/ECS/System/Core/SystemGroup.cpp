#include "SystemGroup.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "Engine/Core/Logging/Logger.h"

namespace
{
// Restores the traversal guard when a system callback returns or throws.
class ExecutionGuard final
{
public:
	explicit ExecutionGuard(bool& executing) noexcept
		: executing_(executing)
	{
		executing_ = true;
	}

	~ExecutionGuard() { executing_ = false; }

private:
	bool& executing_; // Refers to the guarded group's traversal state.
};
} // Anonymous namespace

// Ownership is committed before hooks run, and hooks cannot mutate the group.
ISystem& SystemGroup::AddSystem(std::unique_ptr<ISystem> system)
{
	if (!CanMutate() || !system || system->GetSystemGroup() != nullptr || system->IsInitialized())
	{
		GILGAMESH_LOG(Core, Error, "Cannot register a system: the group is locked or the input is not a new system.");
		throw std::logic_error("System registration requires an unlocked group and an uninitialized system.");
	}

	ISystem& added = *system;
	systems_.push_back(std::move(system));
	added.systemGroup_ = this;
	if (IsInitialized())
	{
		ExecutionGuard guard(executing_);
		try
		{
			added.InitializeOwned();
		}
		catch (...)
		{
			added.systemGroup_ = nullptr;
			systems_.pop_back();
			throw;
		}
	}
	return added;
}

// Removal deactivates and destroys the member before releasing its exclusive ownership.
bool SystemGroup::RemoveSystem(ISystem& system, World& world)
{
	if (!CanMutate())
	{
		GILGAMESH_LOG(Core, Warning, "Cannot remove systems during execution or lifecycle transitions.");
		return false;
	}

	const auto found = std::find_if(systems_.begin(), systems_.end(),
		[&system](const auto& owned) { return owned.get() == &system; });
	if (found == systems_.end())
		return false;

	ExecutionGuard guard(executing_);
	(*found)->ShutdownOwned(world);
	(*found)->systemGroup_ = nullptr;
	systems_.erase(found);
	return true;
}

// Registration order is the scheduling contract; disabled roots do not execute children.
void SystemGroup::Update(World& world, float deltaTime)
{
	if (!IsActive())
		return;
	if (executing_)
	{
		GILGAMESH_LOG(Core, Warning, "Cannot recursively execute the same system group.");
		return;
	}

	ExecutionGuard guard(executing_);
	for (const auto& system : systems_)
	{
		if (system->IsActive())
			system->Update(world, deltaTime);
	}
}

// A running group's own lock cannot be lifted by one of its callbacks.
void SystemGroup::SetMutationAllowed(bool allowed) noexcept
{
	if (executing_)
	{
		GILGAMESH_LOG(Core, Warning, "Cannot change a group mutation gate during execution.");
		return;
	}
	mutationAllowed_ = allowed;
}

// Child creation follows the same order that will later be used for execution.
void SystemGroup::OnCreate()
{
	ExecutionGuard guard(executing_);
	for (const auto& system : systems_)
		system->InitializeOwned();
}

// Reverse teardown preserves dependencies on systems registered earlier in the group.
void SystemGroup::OnDestroy(World& world)
{
	ExecutionGuard guard(executing_);
	while (!systems_.empty())
	{
		// Release each dependent before destroying the earlier systems it may use.
		auto system = std::move(systems_.back());
		systems_.pop_back();
		system->ShutdownOwned(world);
		system->systemGroup_ = nullptr;
	}
}

// Enabling a group restores only the children whose requested flags remain enabled.
void SystemGroup::OnEnable()
{
	ExecutionGuard guard(executing_);
	for (const auto& system : systems_)
		system->RefreshActivation();
}

// Disable dependents first without discarding their requested enablement flags.
void SystemGroup::OnDisable()
{
	ExecutionGuard guard(executing_);
	for (auto system = systems_.rbegin(); system != systems_.rend(); ++system)
		(*system)->RefreshActivation();
}

// Parent guards protect nested ownership while the world gate protects sibling phases.
bool SystemGroup::CanMutate() const noexcept
{
	if (executing_ || !mutationAllowed_)
		return false;
	const SystemGroup* parent = GetSystemGroup();
	return parent == nullptr || parent->CanMutate();
}

// Root groups must obey their own locks as well as the parent checks used by leaves.
bool SystemGroup::IsStateChangeAllowed() const noexcept
{
	return CanMutate();
}
