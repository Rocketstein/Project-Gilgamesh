#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "Engine/ECS/Component/TransformComponent.h"
#include "Engine/ECS/Framework/EntityRegistry.h"
#include "Engine/ECS/Framework/QueryTerm.h"
#include "Engine/ECS/Framework/World.h"
#include "Engine/ECS/System/Transform/LocalToWorldSystem.h"
#include "Engine/ECS/System/Transform/LocalTransformSystem.h"
#include "Engine/ECS/System/Transform/TransformHierarchySystem.h"
#include "Tests/TestFramework.h"

namespace
{
// More than half a chunk forces separate rows into separate chunks for isolation checks.
struct PaddedInput
{
    int id = 0; // Stable test identity independent of the value being edited.
    int value = 0;
    std::array<std::byte, 9000> padding{}; // Prevent another complete row from sharing this chunk.
};

using ReadInput = QueryTerm<PaddedInput, QueryFlags::Read>;
using ChangedInput = QueryTerm<PaddedInput, QueryFlags::Read | QueryFlags::Changed>;
using ReadInteger = QueryTerm<int, QueryFlags::Read>;
using ChangedInteger = QueryTerm<int, QueryFlags::Read | QueryFlags::Changed>;

// Populate an input/output archetype while keeping identifiers stable across later writes.
Entity CreateInput(EntityRegistry& registry, int id)
{
    const Entity entity = registry.Create();
    PaddedInput input;
    input.id = id;
    input.value = id * 10;
    registry.Emplace<PaddedInput>(entity, std::move(input));
    registry.Emplace<int>(entity, id * 100);
    return entity;
}

// Return changed input identities without making archetype or chunk ordering part of the test.
template<typename Source>
std::vector<int> ChangedInputIds(Source& source, std::uint64_t since)
{
    std::vector<int> result;
    source.template ForEachComponent<ChangedInput>(since,
        [&](const PaddedInput& input) { result.push_back(input.id); });
    std::sort(result.begin(), result.end());
    return result;
}

// Observe integer writes through a read-only query without advancing the shared change clock.
template<typename Source>
std::vector<int> ChangedIntegers(Source& source, std::uint64_t since)
{
    std::vector<int> result;
    source.template ForEachComponent<ChangedInteger>(since,
        [&](const int& value) { result.push_back(value); });
    std::sort(result.begin(), result.end());
    return result;
}
} // Anonymous namespace

GILGAMESH_TEST("Engine.ECS.QueryChangeTracking", "DistinguishesReadAccessFromConservativeMutableWrites")
{
    EntityRegistry registry;
    const Entity first = registry.Create();
    const Entity second = registry.Create();
    registry.Emplace<int>(first, 1);
    registry.Emplace<int>(second, 2);
    const std::uint64_t beforeReads = registry.GetChangeVersion();

    // Both const access paths and explicit/legacy read queries must leave change metadata alone.
    (void)std::as_const(registry).Get<int>(first);
    (void)registry.Get<const int>(second);
    std::size_t reads = 0;
    registry.ForEachComponent<ReadInteger>([&](auto& value)
    {
        static_assert(std::is_same_v<decltype(value), const int&>);
        ++reads;
    });
    registry.ForEachComponent<const int>([&](auto& value)
    {
        static_assert(std::is_same_v<decltype(value), const int&>);
        ++reads;
    });
    GILGAMESH_CHECK_MESSAGE(reads == 4 && registry.GetChangeVersion() == beforeReads &&
        ChangedIntegers(registry, beforeReads).empty(),
        "read-only access must neither advance the registry clock nor select changed chunks");

    // One mutable entity access marks its entire component column, including neighboring rows.
    registry.Get<int>(first) = 8;
    GILGAMESH_CHECK_MESSAGE(registry.GetChangeVersion() > beforeReads &&
        ChangedIntegers(registry, beforeReads) == std::vector<int>({ 2, 8 }),
        "mutable Get must expose conservative chunk-level changes rather than per-row changes");
}

GILGAMESH_TEST("Engine.ECS.QueryChangeTracking", "MarksWritableColumnsOnceBeforeTheirChunkCallbacks")
{
    EntityRegistry registry;
    for (int value : { 1, 2 })
    {
        const Entity entity = registry.Create();
        registry.Emplace<int>(entity, value);
        registry.Emplace<float>(entity, static_cast<float>(value));
    }
    const std::uint64_t before = registry.GetChangeVersion();
    std::uint64_t callbackVersion = 0;
    bool stableWithinChunk = true;
    std::size_t visits = 0;

    // A single populated chunk is marked before its first callback, not once for every row or term.
    registry.ForEachComponent<QueryTerm<int, QueryFlags::ReadWrite>,
        QueryTerm<float, QueryFlags::ReadWrite>>([&](auto& integer, auto& scalar)
    {
        static_assert(std::is_same_v<decltype(integer), int&>);
        static_assert(std::is_same_v<decltype(scalar), float&>);
        const std::uint64_t current = registry.GetChangeVersion();
        if (visits == 0) callbackVersion = current;
        stableWithinChunk = stableWithinChunk && current == callbackVersion && current > before;
        ++integer;
        scalar += 1.0f;
        ++visits;
    });
    GILGAMESH_CHECK_MESSAGE(visits == 2 && stableWithinChunk &&
        registry.GetChangeVersion() == callbackVersion && ChangedIntegers(registry, before).size() == 2,
        "writable terms must share one pre-callback chunk publication without per-row clock changes");

    // Plain mutable component types preserve their original writable traversal behavior.
    const std::uint64_t beforeLegacy = registry.GetChangeVersion();
    registry.ForEachComponent<int>([](int& value) { ++value; });
    GILGAMESH_CHECK_MESSAGE(registry.GetChangeVersion() > beforeLegacy &&
        ChangedIntegers(registry, beforeLegacy) == std::vector<int>({ 3, 4 }),
        "legacy mutable queries must continue writing values and publishing change metadata");
}

GILGAMESH_TEST("Engine.ECS.QueryChangeTracking", "CombinesWatchedChangesWithOrAndIgnoresUnwatchedOutputs")
{
    EntityRegistry registry;
    const Entity first = CreateInput(registry, 1);
    const Entity second = CreateInput(registry, 2);
    (void)CreateInput(registry, 3);
    const std::uint64_t beforeInputEdit = registry.GetChangeVersion();
    registry.Get<PaddedInput>(first).value += 1;
    GILGAMESH_CHECK_MESSAGE(ChangedInputIds(registry, beforeInputEdit) == std::vector<int>({ 1 }),
        "editing one independently stored input chunk must not select other chunks");

    // An output write must not select a query that watches only the input column.
    const std::uint64_t beforeOutputEdit = registry.GetChangeVersion();
    registry.Get<int>(second) = 777;
    const std::uint64_t afterOutputEdit = registry.GetChangeVersion();
    std::size_t outputOnlyVisits = 0;
    registry.ForEachComponent<ChangedInput, QueryTerm<int, QueryFlags::ReadWrite>>(
        beforeOutputEdit, [&](const PaddedInput&, int&) { ++outputOnlyVisits; });
    GILGAMESH_CHECK_MESSAGE(outputOnlyVisits == 0 && registry.GetChangeVersion() == afterOutputEdit,
        "unwatched output writes must not trigger callbacks or additional write stamps");

    // Presence requirements use AND, while changes among watched columns use OR.
    const std::uint64_t beforeBothEdits = registry.GetChangeVersion();
    registry.Get<PaddedInput>(first).value += 1;
    registry.Get<int>(second) = 888;
    const Entity inputOnly = registry.Create();
    PaddedInput unmatched;
    unmatched.id = 4;
    registry.Emplace<PaddedInput>(inputOnly, std::move(unmatched));
    std::vector<int> selected;
    registry.ForEachComponent<ChangedInput, ChangedInteger>(beforeBothEdits,
        [&](const PaddedInput& input, const int&) { selected.push_back(input.id); });
    std::sort(selected.begin(), selected.end());
    GILGAMESH_CHECK_MESSAGE(selected == std::vector<int>({ 1, 2 }),
        "either watched type may select a chunk, but every required type must still be present");

    // Writable Changed terms must be tested before their own write intent is published.
    const std::uint64_t settled = registry.GetChangeVersion();
    std::size_t unchangedVisits = 0;
    registry.ForEachComponent<QueryTerm<PaddedInput, QueryFlags::ReadWrite | QueryFlags::Changed>>(
        settled, [&](PaddedInput&) { ++unchangedVisits; });
    GILGAMESH_CHECK_MESSAGE(unchangedVisits == 0 && registry.GetChangeVersion() == settled,
        "an unchanged writable query must not select itself by marking writes before its predicate");
}

GILGAMESH_TEST("Engine.ECS.QueryChangeTracking", "StructuralChangesReachCurrentSourceAndDestinationRows")
{
    EntityRegistry registry;
    const Entity first = registry.Create();
    const Entity second = registry.Create();
    for (Entity entity : { first, second })
    {
        registry.Emplace<int>(entity, entity == first ? 1 : 2);
        registry.Emplace<float>(entity, 1.0f);
    }
    const auto changedMatchingValues = [&](std::uint64_t since)
    {
        std::vector<int> result;
        registry.ForEachComponent<ChangedInteger, QueryTerm<float, QueryFlags::Read>>(since,
            [&](const int& value, const float&) { result.push_back(value); });
        std::sort(result.begin(), result.end());
        return result;
    };

    // Migration must mark surviving source rows and the new destination archetype alike.
    std::uint64_t cursor = registry.GetChangeVersion();
    registry.Emplace<char>(first, 'x');
    GILGAMESH_CHECK_MESSAGE(changedMatchingValues(cursor) == std::vector<int>({ 1, 2 }),
        "moving a component row must publish both source and destination chunk changes");
    cursor = registry.GetChangeVersion();
    registry.Remove<char>(first);
    GILGAMESH_CHECK_MESSAGE(changedMatchingValues(cursor) == std::vector<int>({ 1, 2 }),
        "removing a component must preserve matching queries while stamping the migrated row");

    // Removing the first row swap-pops its neighbor into the source chunk.
    cursor = registry.GetChangeVersion();
    GILGAMESH_CHECK_MESSAGE(registry.Destroy(second), "the source-row entity must be destroyed");
    GILGAMESH_CHECK_MESSAGE(changedMatchingValues(cursor) == std::vector<int>({ 1 }) &&
        registry.Get<const int>(first) == 1,
        "destruction must expose the remaining swap-popped row with its original component identity");
    cursor = registry.GetChangeVersion();
    registry.Remove<float>(first);
    GILGAMESH_CHECK_MESSAGE(changedMatchingValues(cursor).empty() &&
        ChangedIntegers(registry, cursor) == std::vector<int>({ 1 }),
        "removed component types must stop matching even though the remaining column changed");

    // Existing query caches must discover new matching rows after their old archetypes emptied.
    cursor = registry.GetChangeVersion();
    const Entity added = registry.Create();
    registry.Emplace<int>(added, 3);
    registry.Emplace<float>(added, 3.0f);
    GILGAMESH_CHECK_MESSAGE(changedMatchingValues(cursor) == std::vector<int>({ 3 }),
        "newly inserted matching rows must appear without retaining stale query history");
}

GILGAMESH_TEST("Engine.ECS.QueryChangeTracking", "WorldConsumersKeepIndependentObservationCursors")
{
    World world;
    const Entity entity = world.CreateEntity();
    PaddedInput input;
    input.id = 1;
    world.Emplace<PaddedInput>(entity, std::move(input));
    world.Emplace<int>(entity, 10);
    world.Update(0.0f);
    std::uint64_t firstCursor = world.GetChangeVersion();
    const std::uint64_t secondCursor = firstCursor;

    // Const World and explicitly const component access remain untracked reads.
    (void)std::as_const(world).Get<PaddedInput>(entity);
    (void)world.Get<const PaddedInput>(entity);
    GILGAMESH_CHECK_MESSAGE(world.GetChangeVersion() == firstCursor,
        "world read access must preserve the registry's observation clock");
    world.Get<PaddedInput>(entity).value = 42;
    GILGAMESH_CHECK_MESSAGE(ChangedInputIds(world, firstCursor) == std::vector<int>({ 1 }),
        "the first world consumer must observe the new input write");
    firstCursor = world.GetChangeVersion();
    GILGAMESH_CHECK_MESSAGE(ChangedInputIds(world, firstCursor).empty() &&
        ChangedInputIds(world, secondCursor) == std::vector<int>({ 1 }) &&
        ChangedInputIds(world, secondCursor) == std::vector<int>({ 1 }),
        "one consumer advancing its cursor must not consume another consumer's cached query results");

    // A later output-only write advances the clock without changing the input observation.
    world.Get<int>(entity) = 99;
    GILGAMESH_CHECK_MESSAGE(world.GetChangeVersion() > firstCursor &&
        ChangedInputIds(world, firstCursor).empty(),
        "the world wrapper must filter by each watched component version rather than a global dirty flag");
}

GILGAMESH_TEST("Engine.ECS.QueryChangeTracking", "ThrowingCallbacksLeaveTheirPossibleWritesObservable")
{
    EntityRegistry registry;
    for (int value : { 1, 2 })
    {
        const Entity entity = registry.Create();
        registry.Emplace<int>(entity, value);
    }
    const std::uint64_t cursor = registry.GetChangeVersion();
    bool threw = false;
    try
    {
        registry.ForEachComponent<QueryTerm<int, QueryFlags::ReadWrite>>([](int& value)
        {
            value = 99;
            throw std::runtime_error("query callback failed after a possible write");
        });
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    // Pre-callback stamps retain evidence of partial writes for retries and other consumers.
    const auto observed = ChangedIntegers(registry, cursor);
    GILGAMESH_CHECK_MESSAGE(threw && registry.GetChangeVersion() > cursor && observed.size() == 2 &&
        std::find(observed.begin(), observed.end(), 99) != observed.end() &&
        ChangedIntegers(registry, cursor) == observed,
        "callback exceptions must propagate while possible writes remain visible through the unchanged cursor");
    std::size_t retries = 0;
    registry.ForEachComponent<QueryTerm<int, QueryFlags::ReadWrite | QueryFlags::Changed>>(
        cursor, [&](int& value) { ++value; ++retries; });
    const std::uint64_t completed = registry.GetChangeVersion();
    GILGAMESH_CHECK_MESSAGE(retries == 2 && ChangedIntegers(registry, completed).empty(),
        "a successful retry may advance its own cursor after consuming the changed chunk");
}

GILGAMESH_TEST("Engine.ECS.QueryChangeTracking", "LocalTransformSkipsConsumedChunksWithoutRepublishingEqualMatrices")
{
    World world;
    const Entity first = world.CreateEntity();
    const Entity unrelated = world.CreateEntity();
    world.Emplace<TransformComponent>(first);
    world.Emplace<TransformComponent>(unrelated);
    world.Emplace<int>(unrelated, 1);
    world.Update(0.0f);
    LocalTransformSystem local;
    local.Update(world, 0.0f);
    const TransformComponent firstBefore = world.Get<const TransformComponent>(first);
    const TransformComponent unrelatedBefore = world.Get<const TransformComponent>(unrelated);
    const std::uint64_t settled = world.GetChangeVersion();

    // The system consumes its own writable-query stamps before returning.
    local.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(firstBefore.localRevision == 1 && unrelatedBefore.localRevision == 1 &&
        world.GetChangeVersion() == settled,
        "a completed local pass must not select its own previous write intent forever");

    // Separate archetypes keep a changed transform from selecting the unrelated chunk.
    world.Get<TransformComponent>(first).position_.x = 4.0f;
    const std::uint64_t beforeEvaluation = world.GetChangeVersion();
    local.Update(world, 0.0f);
    std::size_t writtenTransforms = 0;
    world.ForEachComponent<QueryTerm<TransformComponent, QueryFlags::Read | QueryFlags::Changed>>(
        beforeEvaluation, [&](const TransformComponent&) { ++writtenTransforms; });
    GILGAMESH_CHECK_MESSAGE(writtenTransforms == 1 &&
        world.Get<const TransformComponent>(first).localRevision > firstBefore.localRevision &&
        world.Get<const TransformComponent>(first).localToParent.Translation().x == 4.0f &&
        world.Get<const TransformComponent>(unrelated).localRevision == unrelatedBefore.localRevision,
        "local evaluation must visit changed chunks while retaining unaffected matrices and revisions");

    // Conservative mutable access with equal TRS still must not publish a new local-matrix revision.
    const std::uint64_t actualRevision = world.Get<const TransformComponent>(first).localRevision;
    (void)world.Get<TransformComponent>(first);
    local.Update(world, 0.0f);
    const std::uint64_t afterEqualEvaluation = world.GetChangeVersion();
    local.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(world.Get<const TransformComponent>(first).localRevision == actualRevision &&
        world.GetChangeVersion() == afterEqualEvaluation,
        "equal recomposition must consume tracked input without changing the matrix revision or repeatedly selecting itself");
}

GILGAMESH_TEST("Engine.ECS.QueryChangeTracking", "PausedPipelineReenablementAndParentChangesPreserveTransformDependencies")
{
    World world;
    auto hierarchy = std::make_unique<TransformHierarchySystem>();
    TransformHierarchySystem& hierarchyReference = *hierarchy;
    auto local = std::make_unique<LocalTransformSystem>();
    LocalTransformSystem& localReference = *local;
    auto& systems = world.GetTransformSystems();
    systems.AddSystem(std::move(hierarchy)).SetEnabled(true);
    systems.AddSystem(std::move(local)).SetEnabled(true);
    systems.AddSystem(std::make_unique<LocalToWorldSystem>(hierarchyReference)).SetEnabled(true);
    const Entity parent = world.CreateEntity();
    const Entity child = world.CreateEntity();
    TransformComponent parentSource;
    parentSource.position_.x = 5.0f;
    TransformComponent childSource;
    childSource.position_.x = 2.0f;
    childSource.parent = parent;
    world.Emplace<TransformComponent>(parent, parentSource);
    world.Emplace<TransformComponent>(child, childSource);
    world.SetSimulationPaused(true);

    // Presentation still evaluates changed transforms when simulation is paused.
    world.PreparePresentation(0.0f);
    const TransformComponent childBefore = world.Get<const TransformComponent>(child);
    GILGAMESH_CHECK_MESSAGE(childBefore.localToWorld.Translation().x == 7.0f,
        "paused presentation must evaluate the registered hierarchy/local/world dependency order");
    localReference.SetEnabled(false);
    world.Get<TransformComponent>(parent).position_.x = 9.0f;
    world.PreparePresentation(0.0f);
    GILGAMESH_CHECK_MESSAGE(world.Get<const TransformComponent>(parent).localToParent.Translation().x == 5.0f,
        "disabled local evaluation must leave its existing local matrix untouched");
    localReference.SetEnabled(true);
    world.PreparePresentation(0.0f);
    GILGAMESH_CHECK_MESSAGE(world.IsSimulationPaused() &&
        world.Get<const TransformComponent>(child).localToWorld.Translation().x == 11.0f &&
        world.Get<const TransformComponent>(child).localRevision == childBefore.localRevision,
        "reenabling local evaluation must consume writes made while disabled and propagate ancestor motion");

    // Parenting changes the world dependency while the child's local TRS and matrix remain equal.
    GILGAMESH_CHECK_MESSAGE(hierarchyReference.SetParent(world, child, {}),
        "the paused child must support detachment through the hierarchy authoring API");
    world.PreparePresentation(0.0f);
    GILGAMESH_CHECK_MESSAGE(world.Get<const TransformComponent>(child).localToWorld.Translation().x == 2.0f &&
        world.Get<const TransformComponent>(child).localRevision == childBefore.localRevision &&
        world.Get<const TransformComponent>(child).worldRevision > childBefore.worldRevision,
        "change filtering must preserve parent identity dependencies independently of local matrix changes");

    // Consume world-cache writes once; later unchanged presentation must stop dirtying its own inputs.
    world.PreparePresentation(0.0f);
    const std::uint64_t settled = world.GetChangeVersion();
    world.PreparePresentation(0.0f);
    GILGAMESH_CHECK_MESSAGE(world.GetChangeVersion() == settled,
        "unchanged hierarchy/local/world evaluation must settle instead of generating perpetual transform writes");
}
