#include <algorithm>
#include <cstddef>
#include <vector>

#include "Engine/ECS/Framework/EntityRegistry.h"
#include "Engine/ECS/Framework/World.h"
#include "Tests/TestFramework.h"

namespace
{
// Collect an owned handle snapshot through the shared registry/world query API.
template <typename Source>
std::vector<Entity> CollectEntities(Source& source)
{
    return source.template QueryEntities<>();
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
    const std::vector<Entity> committed = CollectEntities(world);
    GILGAMESH_CHECK_MESSAGE(committed.size() == 3 &&
        ContainsEntity(committed, active) && ContainsEntity(committed, inactive) &&
        ContainsEntity(committed, unassigned) && !ContainsEntity(committed, foreign),
        "world queries must include inactive scenes and componentless/unassigned entities");

    // Requested removal does not change committed visibility before the next playback.
    world.RemoveEntity(inactive);
    const Entity pending = world.CreateEntity();
    const std::vector<Entity> beforePlayback = CollectEntities(world);
    GILGAMESH_CHECK_MESSAGE(beforePlayback.size() == 3 &&
        ContainsEntity(beforePlayback, inactive) && !ContainsEntity(beforePlayback, pending),
        "pending creation and removal must not alter the currently committed entity set");
    world.Update(0.0f);
    const std::vector<Entity> afterPlayback = CollectEntities(world);
    GILGAMESH_CHECK_MESSAGE(afterPlayback.size() == 3 &&
        ContainsEntity(afterPlayback, active) && ContainsEntity(afterPlayback, unassigned) &&
        ContainsEntity(afterPlayback, pending) && !ContainsEntity(afterPlayback, inactive),
        "playback must publish new entities and retire removed handles before later traversal");
}

GILGAMESH_TEST("Engine.ECS.EntityTraversal", "OwnedSnapshotsSurviveLaterWorldChanges")
{
    World world;
    const Entity first = world.CreateEntity();
    const Entity second = world.CreateEntity();
    const Entity third = world.CreateEntity();
    world.Update(0.0f);

    // An owned snapshot remains iterable while reservations grow the registry's slot storage.
    const std::vector<Entity> snapshot = world.QueryEntities<>();
    constexpr std::size_t RequestsPerEntity = 128;
    std::vector<Entity> visited;
    std::vector<Entity> pending;
    pending.reserve(3 * RequestsPerEntity);
    for (Entity entity : snapshot)
    {
        visited.push_back(entity);
        for (std::size_t request = 0; request < RequestsPerEntity; ++request)
        {
            pending.push_back(world.CreateEntity());
        }
    }
    GILGAMESH_CHECK_MESSAGE(visited.size() == 3 && ContainsEntity(visited, first) &&
        ContainsEntity(visited, second) && ContainsEntity(visited, third) &&
        pending.size() == 3 * RequestsPerEntity,
        "an owned result must retain its original membership while more slots are reserved");
    GILGAMESH_CHECK_MESSAGE(CollectEntities(world).size() == 3 &&
        std::all_of(pending.begin(), pending.end(), [&](Entity entity)
        {
            return !world.IsAlive(entity);
        }),
        "deferred creation requests must remain absent from new queries before playback");

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
        "playback must make every queued reservation available to subsequent queries");

    // Snapshots retain copied handles, even when later playback makes one of them stale.
    world.RemoveEntity(first);
    world.Update(0.0f);
    GILGAMESH_CHECK_MESSAGE(snapshot.size() == 3 && ContainsEntity(snapshot, first) &&
        ContainsEntity(snapshot, second) && ContainsEntity(snapshot, third) &&
        !world.IsAlive(first) && !ContainsEntity(world.QueryEntities<>(), first),
        "later structural changes must not rewrite an owned snapshot; retained handles require validation");
}

GILGAMESH_TEST("Engine.ECS.EntityTraversal", "FilteredQueriesFollowCurrentArchetypeRows")
{
    EntityRegistry registry(17);
    const Entity integerOnly = registry.Create();
    registry.Emplace<int>(integerOnly, 1);
    const Entity matched = registry.Create();
    registry.Emplace<int>(matched, 2);
    registry.Emplace<float>(matched, 3.0f);
    const Entity scalarOnly = registry.Create();
    registry.Emplace<float>(scalarOnly, 4.0f);
    const Entity componentless = registry.Create();

    // Queries require every requested component and copy full stored entity identities.
    const std::vector<Entity> initial = registry.QueryEntities<int, float>();
    GILGAMESH_CHECK_MESSAGE(initial.size() == 1 && ContainsEntity(initial, matched) &&
        !ContainsEntity(initial, integerOnly) && !ContainsEntity(initial, scalarOnly) &&
        !ContainsEntity(initial, componentless) && registry.Get<int>(initial.front()) == 2,
        "filtered results must contain only entities owning every requested component");

    // Cached archetypes expose inserted rows, and new matching archetypes join the same query.
    const Entity sameArchetype = registry.Create();
    registry.Emplace<int>(sameArchetype, 5);
    registry.Emplace<float>(sameArchetype, 6.0f);
    const Entity newArchetype = registry.Create();
    registry.Emplace<int>(newArchetype, 7);
    registry.Emplace<float>(newArchetype, 8.0f);
    registry.Emplace<char>(newArchetype, 'x');
    const std::vector<Entity> expanded = registry.QueryEntities<int, float>();
    GILGAMESH_CHECK_MESSAGE(expanded.size() == 3 && ContainsEntity(expanded, matched) &&
        ContainsEntity(expanded, sameArchetype) && ContainsEntity(expanded, newArchetype) &&
        initial.size() == 1 && ContainsEntity(initial, matched),
        "query caches must follow current rows and newly created matching archetypes without changing older results");

    // Migration swap-pops the source chunk; identity lookup must stay aligned with its moved row.
    registry.Emplace<double>(matched, 9.0);
    const std::vector<Entity> migrated = registry.QueryEntities<int, float>();
    GILGAMESH_CHECK_MESSAGE(migrated.size() == 3 && ContainsEntity(migrated, matched) &&
        ContainsEntity(migrated, sameArchetype) && ContainsEntity(migrated, newArchetype) &&
        registry.Get<int>(sameArchetype) == 5 && registry.Get<float>(sameArchetype) == 6.0f,
        "migrated entities and the source row's displaced identity must each remain queryable once");
    registry.Remove<float>(sameArchetype);
    GILGAMESH_CHECK_MESSAGE(registry.Destroy(matched),
        "the matched entity must be destroyed before checking filtered removal");
    const std::vector<Entity> remaining = registry.QueryEntities<int, float>();
    GILGAMESH_CHECK_MESSAGE(remaining.size() == 1 && ContainsEntity(remaining, newArchetype) &&
        !ContainsEntity(remaining, sameArchetype) && !ContainsEntity(remaining, matched) &&
        expanded.size() == 3 && ContainsEntity(expanded, matched),
        "component removal and destruction must affect new results while older snapshots retain copied handles");

    // A recycled matching entity must carry its new generation through the chunk identity column.
    const Entity replacement = registry.Create();
    registry.Emplace<int>(replacement, 10);
    registry.Emplace<float>(replacement, 11.0f);
    const std::vector<Entity> reused = registry.QueryEntities<int, float>();
    GILGAMESH_CHECK_MESSAGE(replacement != matched && reused.size() == 2 &&
        ContainsEntity(reused, replacement) && ContainsEntity(reused, newArchetype) &&
        !ContainsEntity(reused, matched) && registry.Get<int>(replacement) == 10,
        "filtered query identities must distinguish a recycled slot from its destroyed generation");
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
    const std::vector<Entity> matches = world.QueryEntities<int, float>();
    GILGAMESH_CHECK_MESSAGE(matches.size() == 1 && ContainsEntity(matches, matched) &&
        !ContainsEntity(matches, unmatched),
        "the World query wrapper must preserve the registry's component filtering");
}
