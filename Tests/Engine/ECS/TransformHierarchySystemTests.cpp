#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <set>
#include <span>
#include <vector>

#include "Engine/ECS/Component/TransformComponent.h"
#include "Engine/ECS/Framework/World.h"
#include "Engine/ECS/System/Transform/TransformHierarchySystem.h"
#include "Tests/TestFramework.h"

namespace
{
// Queue a transform entity without requiring scene membership or a renderable mesh.
Entity CreateTransform(World& world, SceneHandle scene = {})
{
    const Entity entity = scene.IsNull() ? world.CreateEntity() : world.CreateEntity(scene);
    world.Emplace<TransformComponent>(entity);
    return entity;
}

// Test a borrowed view without imposing sibling or root ordering.
bool ContainsEntity(std::span<const Entity> entities, Entity entity)
{
    return std::find(entities.begin(), entities.end(), entity) != entities.end();
}

// Compare expected membership and count while allowing any valid traversal order.
bool ContainsExactly(std::span<const Entity> actual, std::initializer_list<Entity> expected)
{
    return actual.size() == expected.size() &&
        std::all_of(expected.begin(), expected.end(), [&](Entity entity)
        {
            return ContainsEntity(actual, entity);
        });
}

// Check public forest invariants rather than duplicating hierarchy construction.
bool IsValidForest(const TransformHierarchySystem& hierarchy, std::size_t expectedCount)
{
    const std::span<const Entity> traversal = hierarchy.GetTraversalOrder();
    if (traversal.size() != expectedCount) return false;

    std::set<Entity> visited;
    std::size_t rootCount = 0;
    for (Entity entity : traversal)
    {
        // Every parent must already have been visited, and every entity appears exactly once.
        if (!hierarchy.Contains(entity) || !visited.insert(entity).second) return false;
        const Entity parent = hierarchy.GetParent(entity);
        if (parent.IsNull())
        {
            if (hierarchy.GetDepth(entity) != 0 ||
                !ContainsEntity(hierarchy.GetRoots(), entity)) return false;
            ++rootCount;
        }
        else
        {
            if (visited.find(parent) == visited.end() || parent == entity ||
                hierarchy.GetDepth(entity) != hierarchy.GetDepth(parent) + 1 ||
                !ContainsEntity(hierarchy.GetChildren(parent), entity)) return false;
        }

        // Derived child views must agree with the validated parent of every child.
        for (Entity child : hierarchy.GetChildren(entity))
        {
            if (!hierarchy.Contains(child) || hierarchy.GetParent(child) != entity) return false;
        }
    }
    return rootCount == hierarchy.GetRoots().size();
}
} // Anonymous namespace

GILGAMESH_TEST("Engine.ECS.TransformHierarchySystem", "PublishesFirstBuildAndIgnoresTrsChanges")
{
    World world;
    TransformHierarchySystem hierarchy;

    // An empty world still publishes a completed first evaluation.
    hierarchy.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(hierarchy.GetRevision() == 1 && IsValidForest(hierarchy, 0) &&
        !hierarchy.Contains(Entity{}) && hierarchy.GetParent(Entity{}).IsNull() &&
        hierarchy.GetDepth(Entity{}) == -1 && hierarchy.GetChildren(Entity{}).empty(),
        "an empty first build must publish revision one and safe unknown-entity queries");
    hierarchy.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(hierarchy.GetRevision() == 1,
        "a repeated unchanged empty evaluation must reuse its topology generation");

    // A new transform changes topology, while edits to its local TRS do not.
    const Entity entity = CreateTransform(world);
    world.Update(0.0f);
    hierarchy.Update(world, 0.0f);
    const std::uint64_t revision = hierarchy.GetRevision();
    TransformComponent& transform = world.Get<TransformComponent>(entity);
    transform.position_ = { 1.0f, 2.0f, 3.0f };
    transform.rotation_ = { 0.0f, 0.0f, 1.0f, 0.0f };
    transform.scale = { 2.0f, 3.0f, 4.0f };
    hierarchy.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(revision > 1 && hierarchy.GetRevision() == revision &&
        ContainsExactly(hierarchy.GetRoots(), { entity }) && IsValidForest(hierarchy, 1),
        "local TRS changes must not rebuild an otherwise unchanged transform topology");
}

GILGAMESH_TEST("Engine.ECS.TransformHierarchySystem", "BuildsWorldWideParentFirstForest")
{
    World world;
    const SceneHandle activeScene = world.CreateScene("Active");
    const SceneHandle inactiveScene = world.CreateScene("Inactive");
    const Entity root = CreateTransform(world, activeScene);
    const Entity child = CreateTransform(world, inactiveScene);
    const Entity grandchild = CreateTransform(world);
    const Entity otherRoot = CreateTransform(world);
    const Entity withoutTransform = world.CreateEntity(inactiveScene);
    GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(activeScene),
        "the first scene must become active without restricting world-wide hierarchy evaluation");
    world.Update(0.0f);

    // Parenting may cross scenes and include entities with neither a scene nor a mesh.
    world.Get<TransformComponent>(child).parent = root;
    world.Get<TransformComponent>(grandchild).parent = child;
    TransformHierarchySystem hierarchy;
    hierarchy.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(IsValidForest(hierarchy, 4) &&
        ContainsExactly(hierarchy.GetRoots(), { root, otherRoot }) &&
        ContainsExactly(hierarchy.GetChildren(root), { child }) &&
        ContainsExactly(hierarchy.GetChildren(child), { grandchild }) &&
        hierarchy.GetChildren(grandchild).empty() &&
        hierarchy.GetDepth(root) == 0 && hierarchy.GetDepth(child) == 1 &&
        hierarchy.GetDepth(grandchild) == 2 && !hierarchy.Contains(withoutTransform),
        "derived children, roots, depths, and traversal must describe every world transform");
}

GILGAMESH_TEST("Engine.ECS.TransformHierarchySystem", "SetParentKeepsLocalTrsUntilCacheRefresh")
{
    World world;
    const Entity parent = CreateTransform(world);
    const Entity child = CreateTransform(world);
    world.Update(0.0f);
    TransformComponent& source = world.Get<TransformComponent>(child);
    source.position_ = { 2.0f, 3.0f, 4.0f };
    source.rotation_ = { 0.0f, 1.0f, 0.0f, 0.0f };
    source.scale = { 2.0f, 1.0f, 3.0f };
    const TransformComponent before = source;
    TransformHierarchySystem hierarchy;
    hierarchy.Update(world, 0.0f);
    const std::uint64_t originalRevision = hierarchy.GetRevision();

    // Successful authoring changes are immediate, but borrowed cache queries remain unchanged.
    GILGAMESH_CHECK_MESSAGE(hierarchy.SetParent(world, child, parent),
        "a live transform may become the child of another live transform");
    const TransformComponent& after = world.Get<TransformComponent>(child);
    GILGAMESH_CHECK_MESSAGE(after.parent == parent && after.position_ == before.position_ &&
        after.rotation_ == before.rotation_ && after.scale == before.scale &&
        after.localToParent == before.localToParent && after.localToWorld == before.localToWorld &&
        after.localRevision == before.localRevision && after.worldRevision == before.worldRevision &&
        hierarchy.GetParent(child).IsNull() && hierarchy.GetRevision() == originalRevision,
        "SetParent must preserve local TRS and evaluated matrices until the next hierarchy update");
    hierarchy.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(hierarchy.GetParent(child) == parent &&
        hierarchy.GetDepth(child) == 1 && hierarchy.GetRevision() > originalRevision,
        "the next update must publish the newly authored parent link");

    // Null parenting detaches without changing the authored local transform.
    GILGAMESH_CHECK_MESSAGE(hierarchy.SetParent(world, child, Entity{}) &&
        world.Get<TransformComponent>(child).parent.IsNull() && hierarchy.GetParent(child) == parent,
        "detachment must change the source link before it changes cached hierarchy queries");
    hierarchy.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(hierarchy.GetParent(child).IsNull() &&
        world.Get<TransformComponent>(child).position_ == before.position_ &&
        world.Get<TransformComponent>(child).rotation_ == before.rotation_ &&
        world.Get<TransformComponent>(child).scale == before.scale && IsValidForest(hierarchy, 2),
        "detachment must produce a root while retaining the same local TRS");
}

GILGAMESH_TEST("Engine.ECS.TransformHierarchySystem", "RejectsInvalidAndUnpublishedParenting")
{
    World world;
    World otherWorld;
    const Entity first = CreateTransform(world);
    const Entity second = CreateTransform(world);
    const Entity third = CreateTransform(world);
    const Entity stale = CreateTransform(world);
    const Entity withoutTransform = world.CreateEntity();
    const Entity foreign = CreateTransform(otherWorld);
    world.Update(0.0f);
    otherWorld.Update(0.0f);
    world.RemoveEntity(stale);
    world.Update(0.0f);
    const Entity reserved = CreateTransform(world);
    TransformHierarchySystem hierarchy;
    hierarchy.Update(world, 0.0f);

    // Neither side of a parent assignment may be a missing, foreign, stale, or reserved transform.
    GILGAMESH_CHECK_MESSAGE(!hierarchy.SetParent(world, Entity{}, first) &&
        !hierarchy.SetParent(world, foreign, first) && !hierarchy.SetParent(world, stale, first) &&
        !hierarchy.SetParent(world, withoutTransform, first) &&
        !hierarchy.SetParent(world, reserved, first) && !hierarchy.SetParent(world, first, foreign) &&
        !hierarchy.SetParent(world, first, stale) && !hierarchy.SetParent(world, first, withoutTransform) &&
        !hierarchy.SetParent(world, first, reserved) && !hierarchy.SetParent(world, first, first),
        "invalid children, invalid parents, reserved entities, and self-parenting must be rejected");

    // Cycle checks must inspect current authored links, including edits not yet rebuilt into the cache.
    GILGAMESH_CHECK_MESSAGE(hierarchy.SetParent(world, first, second) &&
        hierarchy.SetParent(world, second, third) && !hierarchy.SetParent(world, third, first) &&
        !hierarchy.SetParent(world, second, first),
        "multiple authoring operations before Update must not introduce a descendant cycle");
    GILGAMESH_CHECK_MESSAGE(world.Get<TransformComponent>(first).parent == second &&
        world.Get<TransformComponent>(second).parent == third &&
        world.Get<TransformComponent>(third).parent.IsNull(),
        "rejected parent changes must leave the previously authored links intact");
    hierarchy.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(IsValidForest(hierarchy, 3) && hierarchy.GetParent(first) == second &&
        hierarchy.GetParent(second) == third && hierarchy.GetDepth(first) == 2,
        "the accepted pre-update parent assignments must form the expected validated chain");
}

GILGAMESH_TEST("Engine.ECS.TransformHierarchySystem", "InvalidDirectLinksProduceStableValidatedForest")
{
    World world;
    World otherWorld;
    const Entity missingParent = world.CreateEntity();
    const Entity staleParent = CreateTransform(world);
    const Entity missingChild = CreateTransform(world);
    const Entity staleChild = CreateTransform(world);
    const Entity foreignChild = CreateTransform(world);
    const Entity selfParented = CreateTransform(world);
    const Entity cycleFirst = CreateTransform(world);
    const Entity cycleSecond = CreateTransform(world);
    const Entity cycleThird = CreateTransform(world);
    const Entity descendant = CreateTransform(world);
    const Entity validRoot = CreateTransform(world);
    const Entity foreignParent = CreateTransform(otherWorld);
    world.Update(0.0f);
    otherWorld.Update(0.0f);
    world.RemoveEntity(staleParent);
    world.Update(0.0f);

    // Direct component edits can contain invalid links; evaluation must preserve the source data.
    world.Get<TransformComponent>(missingChild).parent = missingParent;
    world.Get<TransformComponent>(staleChild).parent = staleParent;
    world.Get<TransformComponent>(foreignChild).parent = foreignParent;
    world.Get<TransformComponent>(selfParented).parent = selfParented;
    world.Get<TransformComponent>(cycleFirst).parent = cycleSecond;
    world.Get<TransformComponent>(cycleSecond).parent = cycleThird;
    world.Get<TransformComponent>(cycleThird).parent = cycleFirst;
    world.Get<TransformComponent>(descendant).parent = cycleSecond;
    TransformHierarchySystem hierarchy;
    hierarchy.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(IsValidForest(hierarchy, 9) &&
        hierarchy.GetParent(missingChild).IsNull() && hierarchy.GetParent(staleChild).IsNull() &&
        hierarchy.GetParent(foreignChild).IsNull() && hierarchy.GetParent(selfParented).IsNull() &&
        hierarchy.GetParent(descendant) == cycleSecond,
        "bad direct links must be evaluated as roots while a cycle is reduced to a complete forest");
    GILGAMESH_CHECK_MESSAGE(world.Get<TransformComponent>(missingChild).parent == missingParent &&
        world.Get<TransformComponent>(staleChild).parent == staleParent &&
        world.Get<TransformComponent>(foreignChild).parent == foreignParent &&
        world.Get<TransformComponent>(selfParented).parent == selfParented &&
        world.Get<TransformComponent>(cycleFirst).parent == cycleSecond &&
        world.Get<TransformComponent>(cycleSecond).parent == cycleThird &&
        world.Get<TransformComponent>(cycleThird).parent == cycleFirst,
        "repairing the evaluated topology must never overwrite authored parent links");

    // Unchanged invalid source links must not trigger a perpetual rebuild or revision churn.
    const std::uint64_t revision = hierarchy.GetRevision();
    hierarchy.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(hierarchy.GetRevision() == revision && IsValidForest(hierarchy, 9),
        "repeated evaluation of identical invalid source links must retain the cached generation");
    world.Get<TransformComponent>(selfParented).parent = validRoot;
    hierarchy.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(hierarchy.GetRevision() > revision &&
        hierarchy.GetParent(selfParented) == validRoot && IsValidForest(hierarchy, 9),
        "a repaired direct source link must be detected even when other invalid links remain");
}

GILGAMESH_TEST("Engine.ECS.TransformHierarchySystem", "DetectsComponentLifetimeAndGenerationChanges")
{
    World world;
    const Entity root = CreateTransform(world);
    const Entity child = CreateTransform(world);
    world.Update(0.0f);
    world.Get<TransformComponent>(child).parent = root;
    TransformHierarchySystem hierarchy;
    hierarchy.Update(world, 0.0f);

    // Parent edits are detected without requiring callers to use SetParent.
    std::uint64_t revision = hierarchy.GetRevision();
    world.Get<TransformComponent>(child).parent = {};
    hierarchy.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(hierarchy.GetRevision() > revision && hierarchy.GetParent(child).IsNull(),
        "direct parent edits must invalidate topology independently of the authoring API");

    // Reservations stay absent, then become hierarchy nodes only after structural playback.
    revision = hierarchy.GetRevision();
    const Entity created = CreateTransform(world);
    hierarchy.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(hierarchy.GetRevision() == revision && !hierarchy.Contains(created),
        "an unpublished transform reservation must not change the committed topology");
    world.Update(0.0f);
    hierarchy.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(hierarchy.GetRevision() > revision && hierarchy.Contains(created),
        "newly committed transform components must join the evaluated hierarchy");
    revision = hierarchy.GetRevision();
    world.RemoveComponent<TransformComponent>(created);
    world.Update(0.0f);
    hierarchy.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(hierarchy.GetRevision() > revision && world.IsAlive(created) &&
        !hierarchy.Contains(created) && hierarchy.GetDepth(created) == -1,
        "removing a transform must remove its hierarchy node without deleting the entity");

    // Deleting and reusing a parent's slot must never reconnect a child to the replacement.
    world.Get<TransformComponent>(child).parent = root;
    hierarchy.Update(world, 0.0f);
    revision = hierarchy.GetRevision();
    world.RemoveEntity(root);
    world.Update(0.0f);
    hierarchy.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(hierarchy.GetRevision() > revision && !hierarchy.Contains(root) &&
        hierarchy.GetParent(child).IsNull() && world.Get<TransformComponent>(child).parent == root,
        "deleting a parent must retain its stale authored link while evaluating the child as a root");
    revision = hierarchy.GetRevision();
    const Entity replacement = CreateTransform(world);
    world.Update(0.0f);
    hierarchy.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(replacement != root && hierarchy.GetRevision() > revision &&
        hierarchy.Contains(replacement) && !hierarchy.Contains(root) &&
        hierarchy.GetParent(child).IsNull() && IsValidForest(hierarchy, 2),
        "full entity generations must distinguish a new parent slot occupant from the stale handle");
}

GILGAMESH_TEST("Engine.ECS.TransformHierarchySystem", "LifecycleInvalidatesViewsWithoutResettingRevision")
{
    World world;
    const Entity root = CreateTransform(world);
    world.Update(0.0f);
    TransformHierarchySystem hierarchy;
    hierarchy.Initialize();
    hierarchy.SetEnabled(true);
    hierarchy.Update(world, 0.0f);
    const std::uint64_t firstRevision = hierarchy.GetRevision();

    // Disable clears all query storage while retaining the last published generation.
    hierarchy.SetEnabled(false);
    GILGAMESH_CHECK_MESSAGE(!hierarchy.Contains(root) && hierarchy.GetRoots().empty() &&
        hierarchy.GetChildren(root).empty() && hierarchy.GetTraversalOrder().empty() &&
        hierarchy.GetDepth(root) == -1 && hierarchy.GetRevision() == firstRevision,
        "disable must invalidate derived views without rolling back the topology generation");
    hierarchy.SetEnabled(true);
    hierarchy.Update(world, 0.0f);
    const std::uint64_t rebuiltRevision = hierarchy.GetRevision();
    GILGAMESH_CHECK_MESSAGE(rebuiltRevision > firstRevision && IsValidForest(hierarchy, 1),
        "re-enabling must publish a fresh generation after rebuilding the invalidated cache");

    // Shutdown also clears views, and later initialization must not reuse an older revision.
    hierarchy.Shutdown(world);
    GILGAMESH_CHECK_MESSAGE(hierarchy.GetTraversalOrder().empty() &&
        hierarchy.GetRoots().empty() && !hierarchy.Contains(root) &&
        hierarchy.GetRevision() == rebuiltRevision,
        "shutdown must clear the derived hierarchy and retain its last published generation");
    hierarchy.Initialize();
    hierarchy.SetEnabled(true);
    hierarchy.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(hierarchy.GetRevision() > rebuiltRevision && IsValidForest(hierarchy, 1),
        "a reused system lifecycle must rebuild with a strictly newer topology generation");
    hierarchy.Shutdown(world);
}

GILGAMESH_TEST("Engine.ECS.TransformHierarchySystem", "HandlesDeepChainsAndCyclesIteratively")
{
    World world;
    constexpr std::size_t ChainLength = 4096;
    std::vector<Entity> chain;
    chain.reserve(ChainLength);
    for (std::size_t index = 0; index < ChainLength; ++index)
    {
        chain.push_back(CreateTransform(world));
    }
    world.Update(0.0f);

    // A deep authored chain must not require recursive cache construction or traversal.
    for (std::size_t index = 1; index < chain.size(); ++index)
    {
        world.Get<TransformComponent>(chain[index]).parent = chain[index - 1];
    }
    TransformHierarchySystem hierarchy;
    hierarchy.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(IsValidForest(hierarchy, ChainLength) &&
        ContainsExactly(hierarchy.GetRoots(), { chain.front() }) &&
        hierarchy.GetDepth(chain.back()) == static_cast<std::int32_t>(ChainLength - 1),
        "a deep hierarchy must preserve every ancestor and publish the correct leaf depth");
    const std::uint64_t revision = hierarchy.GetRevision();
    GILGAMESH_CHECK_MESSAGE(!hierarchy.SetParent(world, chain.front(), chain.back()) &&
        world.Get<TransformComponent>(chain.front()).parent.IsNull(),
        "cycle validation must safely follow a deep authored parent chain before rejecting it");

    // Directly authored deep cycles must still terminate and publish every node exactly once.
    world.Get<TransformComponent>(chain.front()).parent = chain.back();
    hierarchy.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(hierarchy.GetRevision() > revision &&
        hierarchy.GetRoots().size() == 1 && IsValidForest(hierarchy, ChainLength),
        "breaking one cycle-closing edge must yield a complete forest without recursion failure");
}
