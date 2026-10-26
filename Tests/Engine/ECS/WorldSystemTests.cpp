#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Engine/ECS/Framework/World.h"
#include "Engine/ECS/Framework/World.h"
#include "Engine/ECS/System/Core/ISystem.h"
#include "Engine/ECS/System/Core/SystemGroup.h"
#include "Tests/TestFramework.h"

namespace
{
// Supplies a test callback without coupling assertions to concrete engine systems.
class CallbackSystem final : public ISystem
{
public:
    explicit CallbackSystem(std::function<void(World&, float)> callback)
        : callback_(std::move(callback)) {}

    void Update(World& world, float deltaTime) override
    {
        callback_(world, deltaTime);
    }

private:
    std::function<void(World&, float)> callback_; // Runs under the owning world's phase guard.
};

// Runs a callback only when the final component instance is destroyed.
class DestructorCallbackComponent final
{
public:
    explicit DestructorCallbackComponent(std::function<void()> callback)
        : callback_(std::move(callback)) {}

    // Archetype migration transfers the callback instead of invoking it in the source row.
    DestructorCallbackComponent(DestructorCallbackComponent&& other) noexcept
    {
        callback_.swap(other.callback_);
    }

    DestructorCallbackComponent(const DestructorCallbackComponent&) = delete;
    DestructorCallbackComponent& operator=(const DestructorCallbackComponent&) = delete;

    ~DestructorCallbackComponent() noexcept
    {
        if (callback_)
            callback_();
    }

private:
    std::function<void()> callback_; // Empty after migration; active only in the final row.
};

// Fails during command playback so later reservations must be discarded safely.
class PlaybackFailureComponent final
{
public:
    explicit PlaybackFailureComponent(bool shouldThrow)
    {
        if (shouldThrow)
            throw std::runtime_error("Component playback failed");
    }

    PlaybackFailureComponent(PlaybackFailureComponent&&) noexcept = default;
};

// Registers an enabled system while leaving its lifetime with the group.
void AddEnabledSystem(SystemGroup& group, std::function<void(World&, float)> callback)
{
    ISystem& system = group.AddSystem(std::make_unique<CallbackSystem>(std::move(callback)));
    system.SetEnabled(true);
}

// Compares complete scene identities, including ownership and slot generation.
bool SameScene(SceneHandle left, SceneHandle right)
{
    return left.id == right.id && left.generation == right.generation &&
        left.owningWorldID_ == right.owningWorldID_;
}

// Verifies that a prohibited operation fails through the scheduling contract.
template <typename Fn>
bool RejectsWithLogicError(Fn&& operation)
{
    try
    {
        std::forward<Fn>(operation)();
    }
    catch (const std::logic_error&)
    {
        return true;
    }
    return false;
}
} // Anonymous namespace

GILGAMESH_TEST("Engine.ECS.WorldSystems", "AppliesActiveSceneChangesAtPlaybackBoundaries")
{
    World world;
    const SceneHandle first = world.CreateScene("First");
    const SceneHandle second = world.CreateScene("Second");

    // Scene creation must not silently select a render source.
    GILGAMESH_CHECK_MESSAGE(world.GetActiveSceneHandle().IsNull() &&
        world.TryGetActiveScene() == nullptr,
        "new scenes must remain inactive until explicitly selected");
    GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(first),
        "a live scene must be accepted for deferred activation");
    GILGAMESH_CHECK_MESSAGE(world.TryGetActiveScene() == nullptr,
        "requesting activation must not change the current selection immediately");

    // Either world entry point must apply pending scene selections before systems run.
    world.PreparePresentation(0.0f);
    GILGAMESH_CHECK_MESSAGE(SameScene(world.GetActiveSceneHandle(), first) &&
        world.TryGetActiveScene() == world.TryGetScene(first),
        "presentation preparation must publish the requested scene");
    GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(second),
        "a second scene must be selectable");
    GILGAMESH_CHECK_MESSAGE(SameScene(world.GetActiveSceneHandle(), first),
        "the old selection must remain visible before playback");
    world.Update(0.0f);
    GILGAMESH_CHECK_MESSAGE(SameScene(world.GetActiveSceneHandle(), second),
        "world update must apply the pending scene selection");

    // Clearing follows the same deferred rule and leaves both scenes alive.
    world.RequestClearActiveScene();
    GILGAMESH_CHECK_MESSAGE(SameScene(world.GetActiveSceneHandle(), second),
        "clearing must wait for a playback boundary");
    world.Update(0.0f);
    GILGAMESH_CHECK_MESSAGE(world.GetActiveSceneHandle().IsNull() &&
        world.TryGetActiveScene() == nullptr && world.IsSceneHandleValid(first) &&
        world.IsSceneHandleValid(second),
        "clearing selection must preserve the scenes without choosing a fallback");
}

GILGAMESH_TEST("Engine.ECS.WorldSystems", "PreservesFIFOForSceneAndEntityRequests")
{
    World world;
    const SceneHandle first = world.CreateScene("First");
    const SceneHandle second = world.CreateScene("Second");

    // Mix scene selection, entity creation, component attachment, and scene destruction.
    GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(first),
        "the first activation must be queued");
    const Entity entity = world.CreateEntity(first);
    world.Emplace<int>(entity, 42);
    GILGAMESH_CHECK_MESSAGE(world.RequestDestroyScene(first),
        "destruction of the still-live scene must be queued");
    GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(second),
        "the following activation must be queued after destruction");
    GILGAMESH_CHECK_MESSAGE(world.IsSceneHandleValid(first) && !world.IsAlive(entity),
        "requests must not execute before the playback boundary");

    // FIFO playback must destroy entities created earlier in the same queue.
    world.Update(0.0f);
    GILGAMESH_CHECK_MESSAGE(!world.IsSceneHandleValid(first) && !world.IsAlive(entity),
        "scene destruction must include entities committed earlier during playback");
    GILGAMESH_CHECK_MESSAGE(SameScene(world.GetActiveSceneHandle(), second),
        "the later activation must determine the final active scene");

    // Removing the active scene must not promote an unrelated surviving scene.
    const SceneHandle third = world.CreateScene("Third");
    GILGAMESH_CHECK_MESSAGE(world.RequestDestroyScene(second),
        "the active scene must support deferred destruction");
    world.PreparePresentation(0.0f);
    GILGAMESH_CHECK_MESSAGE(world.GetActiveSceneHandle().IsNull() &&
        world.TryGetActiveScene() == nullptr && world.IsSceneHandleValid(third),
        "destroying the active scene must clear the selection without fallback");
}

GILGAMESH_TEST("Engine.ECS.WorldSystems", "RejectsInvalidSceneHandlesAtRequestAndPlayback")
{
    World world;
    World otherWorld;
    const SceneHandle active = world.CreateScene("Active");
    const SceneHandle foreign = otherWorld.CreateScene("Foreign");
    GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(active),
        "the valid initial selection must be accepted");
    world.Update(0.0f);

    // Foreign and null handles must not disturb the current active scene.
    GILGAMESH_CHECK_MESSAGE(!world.RequestSetActiveScene({}) &&
        !world.RequestSetActiveScene(foreign) && !world.RequestDestroyScene(foreign),
        "invalid scene requests must be rejected before recording");
    world.Update(0.0f);
    GILGAMESH_CHECK_MESSAGE(SameScene(world.GetActiveSceneHandle(), active),
        "invalid requests must preserve the previous selection");

    // A queued handle must be revalidated when its slot is reused before playback.
    const SceneHandle stale = world.CreateScene("Stale");
    GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(stale) &&
        world.RequestDestroyScene(stale),
        "requests for a currently live scene must initially be accepted");
    GILGAMESH_CHECK_MESSAGE(world.DestroyScene(stale),
        "idle immediate destruction must remain available for initialization and tests");
    const SceneHandle replacement = world.CreateScene("Replacement");
    GILGAMESH_CHECK_MESSAGE(replacement.id == stale.id &&
        replacement.generation != stale.generation,
        "scene slot reuse must change the handle generation");
    world.Update(0.0f);
    GILGAMESH_CHECK_MESSAGE(world.IsSceneHandleValid(replacement) &&
        SameScene(world.GetActiveSceneHandle(), active),
        "stale queued requests must neither select nor destroy the replacement");
    GILGAMESH_CHECK_MESSAGE(!world.RequestSetActiveScene(stale) &&
        !world.RequestDestroyScene(stale),
        "stale requests must also be rejected before recording");
}

GILGAMESH_TEST("Engine.ECS.WorldSystems", "ActiveSceneDoesNotNarrowWorldQueries")
{
    World world;
    const SceneHandle active = world.CreateScene("Active");
    const SceneHandle inactive = world.CreateScene("Inactive");
    const Entity activeEntity = world.CreateEntity(active);
    const Entity inactiveEntity = world.CreateEntity(inactive);
    const Entity worldEntity = world.CreateEntity();
    world.Emplace<int>(activeEntity, 1);
    world.Emplace<int>(inactiveEntity, 2);
    world.Emplace<int>(worldEntity, 4);
    GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(active),
        "the render source must be selectable");
    world.Update(0.0f);

    // General ECS queries keep their whole-world semantics after scene selection.
    int count = 0;
    int total = 0;
    world.ForEachComponent<int>([&](int& value)
    {
        ++count;
        total += value;
    });
    GILGAMESH_CHECK_MESSAGE(count == 3 && total == 7,
        "world queries must include inactive-scene and scene-less entities");
}

GILGAMESH_TEST("Engine.ECS.WorldSystems", "OrdersPhasesAndPublishesSimulationChanges")
{
    World world;
    std::vector<int> events;
    const Entity entity = world.CreateEntity();
    bool simulationSawCommittedEntity = false;
    bool transformSawSimulationChange = false;
    int extractedValue = 0;

    // Simulation sees start-boundary playback and records work for its end boundary.
    AddEnabledSystem(world.GetSimulationSystems(), [&](World& current, float deltaTime)
    {
        events.push_back(1);
        simulationSawCommittedEntity = current.IsAlive(entity) && deltaTime == 0.25f;
        current.Emplace<int>(entity, 7);
    });
    AddEnabledSystem(world.GetSimulationSystems(), [&](World&, float)
    {
        events.push_back(2);
    });

    // Presentation evaluates transforms before any extraction reads their final values.
    AddEnabledSystem(world.GetTransformSystems(), [&](World& current, float)
    {
        events.push_back(3);
        transformSawSimulationChange = current.Has<int>(entity);
        if (transformSawSimulationChange)
            current.Get<int>(entity) *= 3;
    });
    AddEnabledSystem(world.GetExtractionSystems(), [&](World& current, float deltaTime)
    {
        events.push_back(4);
        extractedValue = current.Get<int>(entity);
        GILGAMESH_CHECK_MESSAGE(deltaTime == 0.5f,
            "presentation must forward its own elapsed time to extraction");
    });

    world.Update(0.25f);
    GILGAMESH_CHECK_MESSAGE(events == std::vector<int>({ 1, 2 }) &&
        simulationSawCommittedEntity && world.Has<int>(entity),
        "update must run simulation in registration order between playback boundaries");
    world.PreparePresentation(0.5f);
    GILGAMESH_CHECK_MESSAGE(events == std::vector<int>({ 1, 2, 3, 4 }) &&
        transformSawSimulationChange && extractedValue == 21,
        "presentation must consume committed simulation work after transform evaluation");
}

GILGAMESH_TEST("Engine.ECS.WorldSystems", "PauseSkipsSimulationWhileKeepingPresentationLive")
{
    World world;
    int simulationCount = 0;
    int transformCount = 0;
    int extractionCount = 0;
    AddEnabledSystem(world.GetSimulationSystems(), [&](World&, float) { ++simulationCount; });
    AddEnabledSystem(world.GetTransformSystems(), [&](World&, float) { ++transformCount; });
    AddEnabledSystem(world.GetExtractionSystems(), [&](World&, float) { ++extractionCount; });

    // Pausing must still commit editor requests and run presentation systems.
    world.SetSimulationPaused(true);
    const SceneHandle scene = world.CreateScene("Paused");
    const Entity entity = world.CreateEntity(scene);
    GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(scene),
        "editing scene selection must remain available while paused");
    world.Update(0.0f);
    world.PreparePresentation(0.0f);
    GILGAMESH_CHECK_MESSAGE(world.IsSimulationPaused() && simulationCount == 0 &&
        transformCount == 1 && extractionCount == 1 && world.IsAlive(entity) &&
        SameScene(world.GetActiveSceneHandle(), scene),
        "pause must skip simulation alone while preserving playback and presentation");

    // Resuming reuses the already-registered systems and their enabled state.
    world.SetSimulationPaused(false);
    world.Update(0.1f);
    GILGAMESH_CHECK_MESSAGE(!world.IsSimulationPaused() && simulationCount == 1,
        "resuming must restore simulation execution");
}

GILGAMESH_TEST("Engine.ECS.WorldSystems", "BlocksImmediateSceneMutationAndWorldReentry")
{
    World world;
    const SceneHandle scene = world.CreateScene("Protected");
    bool creationRejected = false;
    bool destructionRejected = false;
    bool updateRejected = false;
    bool presentationRejected = false;
    AddEnabledSystem(world.GetSimulationSystems(), [&](World& current, float)
    {
        // Callback code must not mutate the scene table or recursively run phases.
        creationRejected = RejectsWithLogicError([&] { (void)current.CreateScene(); });
        destructionRejected = RejectsWithLogicError([&] { (void)current.DestroyScene(scene); });
        updateRejected = RejectsWithLogicError([&] { world.Update(0.0f); });
        presentationRejected = RejectsWithLogicError([&] { world.PreparePresentation(0.0f); });
    });
    world.Update(0.0f);
    GILGAMESH_CHECK_MESSAGE(creationRejected && destructionRejected &&
        updateRejected && presentationRejected && world.IsSceneHandleValid(scene),
        "callback guards must reject unsafe immediate mutation and phase reentry");

    // A rejected nested call must leave the outer phase able to finish normally.
    GILGAMESH_CHECK_MESSAGE(world.DestroyScene(scene),
        "the world must return to its idle mutation state after system execution");
}

GILGAMESH_TEST("Engine.ECS.WorldSystems", "PresentationRejectsStructuralRequests")
{
    World world;
    const SceneHandle scene = world.CreateScene("Presentation");
    const Entity entity = world.CreateEntity(scene);
    world.Emplace<int>(entity, 5);
    world.Update(0.0f);
    int rejected = 0;

    // Both presentation phases must keep the evaluated ECS structure stable.
    const auto attemptMutations = [&](World& current, float)
    {
        rejected += RejectsWithLogicError([&] { (void)current.RequestCreateScene(); });
        rejected += RejectsWithLogicError([&] { (void)current.CreateEntity(); });
        rejected += RejectsWithLogicError([&] { current.RemoveEntity(entity); });
        rejected += RejectsWithLogicError([&] { current.Emplace<float>(entity, 1.0f); });
        rejected += RejectsWithLogicError([&] { current.RemoveComponent<int>(entity); });
        rejected += RejectsWithLogicError([&] { (void)current.RequestSetActiveScene(scene); });
        rejected += RejectsWithLogicError([&] { current.RequestClearActiveScene(); });
        rejected += RejectsWithLogicError([&] { (void)current.RequestDestroyScene(scene); });
    };
    AddEnabledSystem(world.GetTransformSystems(), attemptMutations);
    AddEnabledSystem(world.GetExtractionSystems(), attemptMutations);
    world.PreparePresentation(0.0f);

    // Rejected requests must not leak into the next structural playback.
    world.Update(0.0f);
    GILGAMESH_CHECK_MESSAGE(rejected == 16 && world.IsAlive(entity) &&
        world.Has<int>(entity) && !world.Has<float>(entity) &&
        world.IsSceneHandleValid(scene) && world.GetActiveSceneHandle().IsNull(),
        "transform and extraction must reject every structural request without recording it");
}

GILGAMESH_TEST("Engine.ECS.WorldSystems", "RestoresIdlePhaseAfterSystemFailure")
{
    World world;
    ISystem& failing = world.GetExtractionSystems().AddSystem(
        std::make_unique<CallbackSystem>([](World&, float)
        {
            throw std::runtime_error("Extraction failed");
        }));
    failing.SetEnabled(true);

    // A system exception must propagate without permanently locking the world phase.
    bool failed = false;
    try
    {
        world.PreparePresentation(0.0f);
    }
    catch (const std::runtime_error&)
    {
        failed = true;
    }
    failing.SetEnabled(false);
    const SceneHandle scene = world.CreateScene("Recovered");
    GILGAMESH_CHECK_MESSAGE(failed && world.RequestSetActiveScene(scene),
        "a failed presentation phase must restore safe idle request handling");
    world.PreparePresentation(0.0f);
    GILGAMESH_CHECK_MESSAGE(SameScene(world.GetActiveSceneHandle(), scene),
        "the next presentation must run after the failed system is disabled");
}

GILGAMESH_TEST("Engine.ECS.WorldSystems", "ProtectsImmediateSceneDestructionFromComponentReentry")
{
    int rejected = 0;
    int destructorCount = 0;
    World world;
    const SceneHandle scene = world.CreateScene("Protected destruction");
    const Entity entity = world.CreateEntity(scene);

    // A component callback runs while its registry row and scene membership are being retired.
    const std::function<void()> onDestroy = [&]
    {
        ++destructorCount;
        rejected += RejectsWithLogicError([&] { world.Update(0.0f); });
        rejected += RejectsWithLogicError([&] { world.PreparePresentation(0.0f); });
        rejected += RejectsWithLogicError([&] { (void)world.CreateScene(); });
        rejected += RejectsWithLogicError([&] { (void)world.DestroyScene(scene); });
        rejected += RejectsWithLogicError([&]
        {
            world.GetExtractionSystems().AddSystem(
                std::make_unique<CallbackSystem>([](World&, float) {}));
        });
    };
    world.Emplace<DestructorCallbackComponent>(entity, onDestroy);
    world.Update(0.0f);

    // Even initialization-only immediate destruction guards callback and scheduling reentry.
    GILGAMESH_CHECK_MESSAGE(world.DestroyScene(scene) && destructorCount == 1 && rejected == 5,
        "immediate scene destruction must reject component reentry and scheduling mutation");
    GILGAMESH_CHECK_MESSAGE(!world.IsAlive(entity) && !world.IsSceneHandleValid(scene),
        "protected destruction must still complete the original scene and entity removal");

    // Teardown guards must release after the synchronous operation completes.
    const SceneHandle replacement = world.CreateScene("After destruction");
    GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(replacement),
        "scene destruction must restore idle request handling");
    world.Update(0.0f);
    GILGAMESH_CHECK_MESSAGE(SameScene(world.GetActiveSceneHandle(), replacement),
        "normal world execution must resume after guarded scene destruction");
}

GILGAMESH_TEST("Engine.ECS.WorldSystems", "ExternalTraversalRecordsSceneAndEntityRequests")
{
    World world;
    const SceneHandle scene = world.CreateScene("Traversal");
    const Entity entity = world.CreateEntity(scene);
    world.Emplace<int>(entity, 10);
    world.Update(0.0f);
    SceneHandle requestedScene;
    Entity requestedEntity;

    // Application code records edits and defers world execution until traversal finishes.
    world.ForEachComponent<int>([&](int& value)
    {
        // All changes remain pending while the current component reference is in use.
        requestedScene = world.RequestCreateScene("From traversal");
        requestedEntity = world.CreateEntity(requestedScene);
        world.Emplace<int>(requestedEntity, 30);
        GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(requestedScene),
            "a reserved scene must accept activation during a stable query");
        world.RemoveComponent<int>(entity);
        value += 2;
    });
    GILGAMESH_CHECK_MESSAGE(world.Has<int>(entity) && world.Get<int>(entity) == 12 &&
        world.IsSceneHandleValid(scene) && !world.IsSceneHandleValid(requestedScene) &&
        !world.IsAlive(requestedEntity),
        "recorded query edits must preserve the current row and leave new values unpublished");
    world.Update(0.0f);
    GILGAMESH_CHECK_MESSAGE(world.IsAlive(entity) && !world.Has<int>(entity) &&
        world.IsAlive(requestedEntity) && world.Get<int>(requestedEntity) == 30 &&
        SameScene(world.GetActiveSceneHandle(), requestedScene),
        "world playback after traversal must commit the scene, entity, and selection together");
}

GILGAMESH_TEST("Engine.ECS.WorldSystems", "ReservedScenesPublishOnlyAtWorldPlayback")
{
    World world;
    std::string sceneName = "Deferred scene";
    const SceneHandle scene = world.RequestCreateScene(sceneName);
    sceneName = "Changed by caller";

    // A reservation is usable by queued requests but does not expose a live scene.
    GILGAMESH_CHECK_MESSAGE(!scene.IsNull() && !world.IsSceneHandleValid(scene) &&
        world.TryGetScene(scene) == nullptr && world.TryGetActiveScene() == nullptr,
        "request creation must return a reserved identity without publishing scene storage");
    const Entity entity = world.CreateEntity(scene);
    world.Emplace<int>(entity, 42);
    GILGAMESH_CHECK_MESSAGE(!entity.IsNull() && !world.IsAlive(entity) &&
        world.RequestSetActiveScene(scene),
        "entity creation and activation must accept the same reserved scene identity");

    // One FIFO commits the scene before its entity, component, and active selection.
    world.PreparePresentation(0.0f);
    const Scene* created = world.TryGetScene(scene);
    GILGAMESH_CHECK_MESSAGE(created != nullptr && created->Name() == "Deferred scene" &&
        created->Contains(entity) && world.IsAlive(entity) && world.Has<int>(entity) &&
        world.Get<int>(entity) == 42 && SameScene(world.GetActiveSceneHandle(), scene),
        "playback must own the recorded name and publish dependent requests in recording order");
}

GILGAMESH_TEST("Engine.ECS.WorldSystems", "ReservedSceneCreationUsesDefaultName")
{
    World world;
    const SceneHandle scene = world.RequestCreateScene();

    // Default names use the reserved slot identity, matching immediate initialization.
    world.Update(0.0f);
    const Scene* created = world.TryGetScene(scene);
    GILGAMESH_CHECK_MESSAGE(created != nullptr &&
        created->Name() == "Scene_" + std::to_string(scene.id) &&
        world.GetActiveSceneHandle().IsNull(),
        "deferred default naming must preserve the slot-derived name without selecting the scene");
}

GILGAMESH_TEST("Engine.ECS.WorldSystems", "SimulationPublishesRequestedSceneAtEndBoundary")
{
    World world;
    SceneHandle requestedScene;
    Entity requestedEntity;
    bool sawReservedState = false;

    // A scheduled system can queue scene creation and dependent work during simulation.
    AddEnabledSystem(world.GetSimulationSystems(), [&](World& current, float)
    {
        requestedScene = current.RequestCreateScene("From simulation");
        requestedEntity = current.CreateEntity(requestedScene);
        current.Emplace<int>(requestedEntity, 9);
        sawReservedState = !current.IsSceneHandleValid(requestedScene) &&
            current.TryGetScene(requestedScene) == nullptr && !current.IsAlive(requestedEntity);
        GILGAMESH_CHECK_MESSAGE(current.RequestSetActiveScene(requestedScene),
            "simulation must be able to select the scene it has just reserved");
    });

    // The update's final playback publishes all requests before presentation preparation.
    world.Update(0.0f);
    const Scene* created = world.TryGetScene(requestedScene);
    GILGAMESH_CHECK_MESSAGE(sawReservedState && created != nullptr &&
        created->Contains(requestedEntity) && world.Has<int>(requestedEntity) &&
        world.Get<int>(requestedEntity) == 9 &&
        SameScene(world.GetActiveSceneHandle(), requestedScene),
        "the simulation end boundary must commit the requested scene and its dependent entity");
}

GILGAMESH_TEST("Engine.ECS.WorldSystems", "CancelsReservedSceneWithoutRevivingStaleRequests")
{
    World world;
    const SceneHandle cancelled = world.RequestCreateScene("Cancelled");
    const Entity cancelledEntity = world.CreateEntity(cancelled);
    GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(cancelled) &&
        world.RequestDestroyScene(cancelled),
        "dependent requests must initially accept a pending scene");

    // The initialization-only immediate API may cancel a not-yet-created scene slot.
    GILGAMESH_CHECK_MESSAGE(world.DestroyScene(cancelled) &&
        !world.IsSceneHandleValid(cancelled) && world.TryGetScene(cancelled) == nullptr,
        "immediate destruction of a pending scene must cancel its reservation");
    const SceneHandle replacement = world.RequestCreateScene("Replacement");
    GILGAMESH_CHECK_MESSAGE(replacement.id == cancelled.id &&
        replacement.generation != cancelled.generation &&
        !world.RequestSetActiveScene(cancelled) && !world.RequestDestroyScene(cancelled),
        "reservation cancellation must recycle the slot with a new complete identity");

    // Old creation, activation, and destruction payloads cannot address the replacement.
    world.Update(0.0f);
    const Scene* created = world.TryGetScene(replacement);
    GILGAMESH_CHECK_MESSAGE(created != nullptr && created->Name() == "Replacement" &&
        !world.IsAlive(cancelledEntity) && world.GetActiveSceneHandle().IsNull(),
        "stale queued requests must neither publish cancelled entities nor affect the reused slot");
    world.Update(0.0f);
    GILGAMESH_CHECK_MESSAGE(world.IsSceneHandleValid(replacement),
        "payload cleanup after successful playback must not cancel an already committed scene");
}

GILGAMESH_TEST("Engine.ECS.WorldSystems", "PreservesFIFOForPendingSceneDestruction")
{
    World world;
    const SceneHandle scene = world.RequestCreateScene("Short lived");
    const Entity entity = world.CreateEntity(scene);
    GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(scene) &&
        world.RequestDestroyScene(scene) && world.RequestSetActiveScene(scene),
        "a pending scene must accept later requests before playback validates their order");

    // Creation runs first, destruction retires the entity, and late activation becomes stale.
    world.Update(0.0f);
    GILGAMESH_CHECK_MESSAGE(!world.IsSceneHandleValid(scene) && !world.IsAlive(entity) &&
        world.GetActiveSceneHandle().IsNull() && world.TryGetActiveScene() == nullptr,
        "create and destroy requests in one FIFO must leave no live scene or active selection");
    const SceneHandle replacement = world.RequestCreateScene("After FIFO destruction");
    world.Update(0.0f);
    GILGAMESH_CHECK_MESSAGE(replacement.id == scene.id &&
        replacement.generation != scene.generation && world.IsSceneHandleValid(replacement),
        "deferred destruction must make the retired slot reusable under a new generation");
}

GILGAMESH_TEST("Engine.ECS.WorldSystems", "PlaybackFailureCancelsUncommittedSceneReservations")
{
    World world;
    const SceneHandle committed = world.RequestCreateScene("Committed before failure");
    const Entity entity = world.CreateEntity();
    world.Emplace<PlaybackFailureComponent>(entity, true);
    const SceneHandle discarded = world.RequestCreateScene("Never executed");
    const Entity discardedEntity = world.CreateEntity(discarded);
    GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(discarded),
        "the later reserved scene must accept a dependent activation request");

    // Failure discards the remaining queue while preserving work committed before it.
    bool failed = false;
    try
    {
        world.Update(0.0f);
    }
    catch (const std::runtime_error&)
    {
        failed = true;
    }
    GILGAMESH_CHECK_MESSAGE(failed && world.IsSceneHandleValid(committed) &&
        !world.IsSceneHandleValid(discarded) && world.TryGetScene(discarded) == nullptr &&
        !world.IsAlive(discardedEntity) && world.GetActiveSceneHandle().IsNull(),
        "failed playback must preserve committed scenes and cancel every future reservation");

    // The discarded identity cannot become valid after its scene slot is reused.
    const SceneHandle replacement = world.RequestCreateScene("After failure");
    GILGAMESH_CHECK_MESSAGE(replacement.id == discarded.id &&
        replacement.generation != discarded.generation &&
        !world.RequestSetActiveScene(discarded) && !world.RequestDestroyScene(discarded),
        "clearing failed payloads must retire pending generations before exposing their slots");
    world.Update(0.0f);
    GILGAMESH_CHECK_MESSAGE(world.IsSceneHandleValid(committed) &&
        world.IsSceneHandleValid(replacement) && !world.IsAlive(discardedEntity),
        "the world must recover after failure without reviving discarded requests");
}

GILGAMESH_TEST("Engine.ECS.WorldSystems", "RejectsForeignPendingSceneRequests")
{
    World world;
    World otherWorld;
    const SceneHandle local = world.RequestCreateScene("Local pending");
    const SceneHandle foreign = otherWorld.RequestCreateScene("Foreign pending");

    // Equal slot and generation values do not grant access across world ownership.
    GILGAMESH_CHECK_MESSAGE(local.id == foreign.id && local.generation == foreign.generation &&
        !world.RequestSetActiveScene(foreign) && !world.RequestDestroyScene(foreign) &&
        !world.DestroyScene(foreign),
        "pending scene requests must validate the owning world as well as slot and generation");
    const Entity foreignEntity = world.CreateEntity(foreign);

    // Rejected ownership must not disturb either world's independent creation request.
    world.Update(0.0f);
    otherWorld.Update(0.0f);
    GILGAMESH_CHECK_MESSAGE(world.IsSceneHandleValid(local) &&
        otherWorld.IsSceneHandleValid(foreign) && !world.IsAlive(foreignEntity) &&
        world.GetActiveSceneHandle().IsNull(),
        "foreign membership must never commit while both owning scene reservations remain valid");
}
