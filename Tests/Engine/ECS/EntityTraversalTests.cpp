#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include "Engine/ECS/Framework/EntityRegistry.h"
#include "Engine/ECS/Framework/World.h"
#include "Tests/TestFramework.h"

namespace
{
// Collect handles through the same const traversal API exposed by a registry or world.
template <typename Source>
std::vector<Entity> CollectEntities(const Source& source)
{
    std::vector<Entity> entities;
    source.ForEachEntity([&](Entity entity)
    {
        entities.push_back(entity);
    });
    return entities;
}

// Compare membership without making traversal order part of the public contract.
bool ContainsEntity(const std::vector<Entity>& entities, Entity entity)
{
    return std::find(entities.begin(), entities.end(), entity) != entities.end();
}
} // Anonymous namespace

GILGAMESH_TEST("Engine.ECS.EntityTraversal", "EnumeratesOnlyLiveRegistryHandles")
{
    EntityRegistry registry(7);
    EntityRegistry sameWorldRegistry(7);
    EntityRegistry otherWorldRegistry(8);

    // Keep every non-live state present while replacing a previously issued generation.
    const Entity componentless = registry.Create();
    const Entity withComponent = registry.Create();
    registry.Emplace<int>(withComponent, 42);
    const Entity destroyed = registry.Create();
    const Entity recycled = registry.Create();
    const Entity reserved = registry.Reserve();
    const Entity canceled = registry.Reserve();
    GILGAMESH_CHECK_MESSAGE(registry.Destroy(recycled),
        "the recycled entity must be destroyed before replacing its generation");
    const Entity replacement = registry.Create();
    GILGAMESH_CHECK_MESSAGE(registry.Destroy(destroyed),
        "the destroyed slot must remain absent from traversal");
    registry.CancelEntityReservation(canceled);

    // Enumeration includes entities without component rows and preserves complete identities.
    const std::vector<Entity> entities = CollectEntities(registry);
    GILGAMESH_CHECK_MESSAGE(entities.size() == 3 &&
        ContainsEntity(entities, componentless) && ContainsEntity(entities, withComponent) &&
        ContainsEntity(entities, replacement),
        "all and only live handles must be returned, with or without components");
    GILGAMESH_CHECK_MESSAGE(replacement != recycled &&
        !ContainsEntity(entities, recycled) && !ContainsEntity(entities, destroyed) &&
        !ContainsEntity(entities, reserved) && !ContainsEntity(entities, canceled),
        "stale, destroyed, reserved, and canceled handles must never be returned");
    GILGAMESH_CHECK_MESSAGE(std::all_of(entities.begin(), entities.end(), [&](Entity entity)
        {
            return registry.GetEntityState(entity) == EntityState::Alive &&
                sameWorldRegistry.GetEntityState(entity) == EntityState::Invalid &&
                otherWorldRegistry.GetEntityState(entity) == EntityState::Invalid;
        }),
        "enumerated handles must retain the current generation and registry/world identities");
}

GILGAMESH_TEST("Engine.ECS.EntityTraversal", "WorldTraversalUsesCommittedWholeWorldState")
{
    World world;
    World otherWorld;
    const SceneHandle activeScene = world.CreateScene("Active");
    const SceneHandle inactiveScene = world.CreateScene("Inactive");
    const Entity active = world.CreateEntity(activeScene);
    const Entity inactive = world.CreateEntity(inactiveScene);
    const Entity unassigned = world.CreateEntity();
    world.Emplace<int>(active, 11);
    GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(activeScene),
        "the first scene must be selected to distinguish active and inactive membership");

    // Pending entities have identities but must stay invisible until world playback.
    GILGAMESH_CHECK_MESSAGE(CollectEntities(world).empty(),
        "entity reservations must not appear before their first playback");
    world.Update(0.0f);
    const Entity foreign = otherWorld.CreateEntity();
    otherWorld.Update(0.0f);
    const World& readOnlyWorld = world;
    const std::vector<Entity> committed = CollectEntities(readOnlyWorld);
    GILGAMESH_CHECK_MESSAGE(committed.size() == 3 &&
        ContainsEntity(committed, active) && ContainsEntity(committed, inactive) &&
        ContainsEntity(committed, unassigned) && !ContainsEntity(committed, foreign),
        "const world traversal must include inactive scenes and componentless/unassigned entities");

    // Requested removal does not change committed visibility before the next playback.
    world.RemoveEntity(inactive);
    const Entity pending = world.CreateEntity();
    const std::vector<Entity> beforePlayback = CollectEntities(readOnlyWorld);
    GILGAMESH_CHECK_MESSAGE(beforePlayback.size() == 3 &&
        ContainsEntity(beforePlayback, inactive) && !ContainsEntity(beforePlayback, pending),
        "pending creation and removal must not alter the currently committed entity set");
    world.Update(0.0f);
    const std::vector<Entity> afterPlayback = CollectEntities(readOnlyWorld);
    GILGAMESH_CHECK_MESSAGE(afterPlayback.size() == 3 &&
        ContainsEntity(afterPlayback, active) && ContainsEntity(afterPlayback, unassigned) &&
        ContainsEntity(afterPlayback, pending) && !ContainsEntity(afterPlayback, inactive),
        "playback must publish new entities and retire removed handles before later traversal");
}

GILGAMESH_TEST("Engine.ECS.EntityTraversal", "QueuedCreationDoesNotInvalidateTraversal")
{
    World world;
    const Entity first = world.CreateEntity();
    const Entity second = world.CreateEntity();
    const Entity third = world.CreateEntity();
    world.Update(0.0f);

    // Many reservations exercise slot-vector growth while the original handles are visited.
    constexpr std::size_t RequestsPerVisitor = 128;
    std::vector<Entity> visited;
    std::vector<Entity> pending;
    pending.reserve(3 * RequestsPerVisitor);
    world.ForEachEntity([&](Entity entity)
    {
        visited.push_back(entity);
        for (std::size_t request = 0; request < RequestsPerVisitor; ++request)
        {
            pending.push_back(world.CreateEntity());
        }
    });
    GILGAMESH_CHECK_MESSAGE(visited.size() == 3 && ContainsEntity(visited, first) &&
        ContainsEntity(visited, second) && ContainsEntity(visited, third) &&
        pending.size() == 3 * RequestsPerVisitor,
        "reserving more slots during traversal must visit each original committed entity once");
    GILGAMESH_CHECK_MESSAGE(CollectEntities(world).size() == 3 &&
        std::all_of(pending.begin(), pending.end(), [&](Entity entity)
        {
            return !world.IsAlive(entity);
        }),
        "callback creation requests must remain reserved after traversal returns");

    // The next traversal must see every reservation only after playback commits it.
    world.Update(0.0f);
    const std::vector<Entity> committed = CollectEntities(world);
    GILGAMESH_CHECK_MESSAGE(committed.size() == 3 + pending.size() &&
        ContainsEntity(committed, first) && ContainsEntity(committed, second) &&
        ContainsEntity(committed, third) &&
        std::all_of(pending.begin(), pending.end(), [&](Entity entity)
        {
            return world.IsAlive(entity) && ContainsEntity(committed, entity);
        }),
        "playback must make every queued reservation available to subsequent traversal");
}

GILGAMESH_TEST("Engine.ECS.EntityTraversal", "VisitorExceptionsLeaveTraversalUsable")
{
    EntityRegistry registry;
    const Entity registryEntity = registry.Create();
    bool registryException = false;

    // Registry traversal must propagate visitor failures without changing entity lifetime.
    try
    {
        registry.ForEachEntity([](Entity)
        {
            throw std::runtime_error("Registry visitor failed");
        });
    }
    catch (const std::runtime_error&)
    {
        registryException = true;
    }
    const std::vector<Entity> registryEntities = CollectEntities(registry);
    GILGAMESH_CHECK_MESSAGE(registryException && registryEntities.size() == 1 &&
        ContainsEntity(registryEntities, registryEntity),
        "a visitor exception must propagate and leave registry traversal usable");

    // World delegation must preserve the same exception and recovery behavior.
    World world;
    const Entity worldEntity = world.CreateEntity();
    world.Update(0.0f);
    bool worldException = false;
    try
    {
        world.ForEachEntity([](Entity)
        {
            throw std::runtime_error("World visitor failed");
        });
    }
    catch (const std::runtime_error&)
    {
        worldException = true;
    }
    const std::vector<Entity> worldEntities = CollectEntities(world);
    GILGAMESH_CHECK_MESSAGE(worldException && worldEntities.size() == 1 &&
        ContainsEntity(worldEntities, worldEntity),
        "a visitor exception must propagate and leave world traversal usable");
}

GILGAMESH_TEST("Engine.ECS.EntityTraversal", "ComponentTraversalStillFiltersAndUpdatesMatchingRows")
{
    World world;
    const Entity matched = world.CreateEntity();
    const Entity unmatched = world.CreateEntity();
    world.Emplace<int>(matched, 3);
    world.Emplace<float>(matched, 4.0f);
    world.Emplace<int>(unmatched, 10);
    world.Update(0.0f);

    // The renamed component traversal keeps its typed filtering and mutable references.
    std::size_t visits = 0;
    world.ForEachComponent<int, float>([&](int& integer, float& scalar)
    {
        ++visits;
        integer += 4;
        scalar *= 2.0f;
    });
    GILGAMESH_CHECK_MESSAGE(visits == 1 && world.Get<int>(matched) == 7 &&
        world.Get<float>(matched) == 8.0f && world.Get<int>(unmatched) == 10,
        "component traversal must update only entities that contain every requested component");
}
