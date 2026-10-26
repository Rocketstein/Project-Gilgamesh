#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <utility>

#include "Engine/ECS/Component/TransformComponent.h"
#include "Engine/ECS/Framework/World.h"
#include "Engine/ECS/System/Transform/LocalToWorldSystem.h"
#include "Engine/ECS/System/Transform/LocalTransformSystem.h"
#include "Engine/ECS/System/Transform/TransformHierarchySystem.h"
#include "Tests/TestFramework.h"

namespace
{
// Queue ordinary transform storage without requiring rendering or a scene.
Entity CreateTransform(World& world)
{
    const Entity entity = world.CreateEntity();
    world.Emplace<TransformComponent>(entity);
    return entity;
}

// Run the production dependency order after the test has applied structural changes.
void EvaluateTransforms(
    World& world, TransformHierarchySystem& hierarchy,
    LocalTransformSystem& local, LocalToWorldSystem& global)
{
    hierarchy.Update(world, 0.0f);
    local.Update(world, 0.0f);
    global.Update(world, 0.0f);
}

// Apply authored SRT directly to a point for an expectation independent of matrix composition.
Vector3 ApplyAuthoredTransform(Vector3 point, const TransformComponent& transform)
{
    return Rotate(Scale(point, transform.scale), transform.rotation_) + transform.position_;
}
} // Anonymous namespace

GILGAMESH_TEST("Engine.ECS.LocalToWorldSystem", "InitializesIdentityAndReusesUnchangedResults")
{
    World world;
    const Entity root = CreateTransform(world);
    const Entity child = CreateTransform(world);
    world.Update(0.0f);
    world.Get<TransformComponent>(child).parent = root;
    TransformHierarchySystem hierarchy;
    LocalTransformSystem local;
    LocalToWorldSystem global(hierarchy);

    // Default identity caches still need a first published world revision.
    EvaluateTransforms(world, hierarchy, local, global);
    const TransformComponent rootBefore = world.Get<TransformComponent>(root);
    const TransformComponent childBefore = world.Get<TransformComponent>(child);
    GILGAMESH_CHECK_MESSAGE(rootBefore.worldRevision != 0 && childBefore.worldRevision != 0 &&
        rootBefore.localToWorld == Matrix4::Identity() &&
        childBefore.localToWorld == Matrix4::Identity() && rootBefore.evaluatedParent.IsNull() &&
        rootBefore.evaluatedParentWorldRevision == 0 && childBefore.evaluatedParent == root &&
        childBefore.evaluatedLocalRevision == childBefore.localRevision &&
        childBefore.evaluatedParentWorldRevision == rootBefore.worldRevision,
        "first evaluation must publish identity matrices and record the correct root/parent inputs");

    // Repeated presentation without authored changes must leave all revisions unchanged.
    EvaluateTransforms(world, hierarchy, local, global);
    const TransformComponent& rootAfter = world.Get<TransformComponent>(root);
    const TransformComponent& childAfter = world.Get<TransformComponent>(child);
    GILGAMESH_CHECK_MESSAGE(rootAfter.worldRevision == rootBefore.worldRevision &&
        childAfter.worldRevision == childBefore.worldRevision &&
        rootAfter.localRevision == rootBefore.localRevision &&
        childAfter.localRevision == childBefore.localRevision &&
        childAfter.localToWorld == childBefore.localToWorld,
        "an unchanged evaluation must not advance local or world revisions");
}

GILGAMESH_TEST("Engine.ECS.LocalToWorldSystem", "ComposesParentsBeforeEarlierCreatedChildren")
{
    World world;
    const Entity leaf = CreateTransform(world);
    const Entity child = CreateTransform(world);
    const Entity root = CreateTransform(world);
    world.Update(0.0f);
    world.Get<TransformComponent>(leaf).parent = child;
    world.Get<TransformComponent>(leaf).position_ = { 1.0f, 2.0f, 0.0f };
    world.Get<TransformComponent>(child).parent = root;
    world.Get<TransformComponent>(child).position_ = { 0.0f, 3.0f, 1.0f };
    world.Get<TransformComponent>(child).rotation_ =
        FromAxisAngle({ 0.0f, 0.0f, 1.0f }, std::numbers::pi_v<float> * 0.5f);
    world.Get<TransformComponent>(root).position_ = { 10.0f, 20.0f, 30.0f };
    world.Get<TransformComponent>(root).scale = { 2.0f, 3.0f, 4.0f };
    TransformHierarchySystem hierarchy;
    LocalTransformSystem local;
    LocalToWorldSystem global(hierarchy);

    // Creation order is deliberately the reverse of the required evaluation order.
    EvaluateTransforms(world, hierarchy, local, global);
    const TransformComponent& rootTransform = world.Get<TransformComponent>(root);
    const TransformComponent& childTransform = world.Get<TransformComponent>(child);
    const TransformComponent& leafTransform = world.Get<TransformComponent>(leaf);
    const Vector3 point{ 2.0f, 1.0f, 3.0f };
    const Vector3 expected = ApplyAuthoredTransform(
        ApplyAuthoredTransform(ApplyAuthoredTransform(point, leafTransform), childTransform),
        rootTransform);
    GILGAMESH_CHECK_MESSAGE(rootTransform.localToWorld == rootTransform.localToParent &&
        NearlyEquals(TransformPoint(point, leafTransform.localToWorld), expected, 1e-4f) &&
        childTransform.evaluatedParent == root && leafTransform.evaluatedParent == child &&
        leafTransform.evaluatedParentWorldRevision == childTransform.worldRevision,
        "parent-first evaluation must compose child-local transforms before every ancestor");
}

GILGAMESH_TEST("Engine.ECS.LocalToWorldSystem", "PropagatesChangesWithoutUpdatingUnrelatedBranches")
{
    World world;
    const Entity root = CreateTransform(world);
    const Entity child = CreateTransform(world);
    const Entity leaf = CreateTransform(world);
    const Entity otherRoot = CreateTransform(world);
    const Entity otherChild = CreateTransform(world);
    world.Update(0.0f);
    world.Get<TransformComponent>(root).position_ = { 10.0f, 0.0f, 0.0f };
    world.Get<TransformComponent>(child).parent = root;
    world.Get<TransformComponent>(child).position_ = { 1.0f, 0.0f, 0.0f };
    world.Get<TransformComponent>(leaf).parent = child;
    world.Get<TransformComponent>(leaf).position_ = { 0.0f, 2.0f, 0.0f };
    world.Get<TransformComponent>(otherChild).parent = otherRoot;
    TransformHierarchySystem hierarchy;
    LocalTransformSystem local;
    LocalToWorldSystem global(hierarchy);
    EvaluateTransforms(world, hierarchy, local, global);
    const TransformComponent rootBefore = world.Get<TransformComponent>(root);
    const TransformComponent childBefore = world.Get<TransformComponent>(child);
    const TransformComponent leafBefore = world.Get<TransformComponent>(leaf);
    const TransformComponent otherRootBefore = world.Get<TransformComponent>(otherRoot);
    const TransformComponent otherChildBefore = world.Get<TransformComponent>(otherChild);

    // A direct local edit updates the edited node and its descendants, not ancestors or siblings.
    world.Get<TransformComponent>(child).position_.x = 4.0f;
    EvaluateTransforms(world, hierarchy, local, global);
    GILGAMESH_CHECK_MESSAGE(world.Get<TransformComponent>(root).worldRevision == rootBefore.worldRevision &&
        world.Get<TransformComponent>(child).localRevision > childBefore.localRevision &&
        world.Get<TransformComponent>(child).worldRevision > childBefore.worldRevision &&
        world.Get<TransformComponent>(leaf).localRevision == leafBefore.localRevision &&
        world.Get<TransformComponent>(leaf).worldRevision > leafBefore.worldRevision,
        "a local edit must propagate through descendant world matrices without changing their local matrices");
    const std::uint64_t childRevision = world.Get<TransformComponent>(child).worldRevision;
    const std::uint64_t leafRevision = world.Get<TransformComponent>(leaf).worldRevision;

    // Ancestor motion must reach unchanged descendant local transforms in the same pass.
    world.Get<TransformComponent>(root).position_.z = 3.0f;
    EvaluateTransforms(world, hierarchy, local, global);
    GILGAMESH_CHECK_MESSAGE(world.Get<TransformComponent>(root).worldRevision > rootBefore.worldRevision &&
        world.Get<TransformComponent>(child).worldRevision > childRevision &&
        world.Get<TransformComponent>(leaf).worldRevision > leafRevision &&
        NearlyEquals(TransformPoint({}, world.Get<TransformComponent>(leaf).localToWorld),
            Vector3{ 14.0f, 2.0f, 3.0f }) &&
        world.Get<TransformComponent>(otherRoot).worldRevision == otherRootBefore.worldRevision &&
        world.Get<TransformComponent>(otherChild).worldRevision == otherChildBefore.worldRevision &&
        world.Get<TransformComponent>(otherChild).localToWorld == otherChildBefore.localToWorld,
        "ancestor motion must propagate once while independent branches retain their published state");
}

GILGAMESH_TEST("Engine.ECS.LocalToWorldSystem", "ReparentingUpdatesWorldWithoutLocalChanges")
{
    World world;
    const Entity firstParent = CreateTransform(world);
    const Entity secondParent = CreateTransform(world);
    const Entity child = CreateTransform(world);
    world.Update(0.0f);
    world.Get<TransformComponent>(firstParent).position_.x = 1.0f;
    world.Get<TransformComponent>(secondParent).position_.x = 9.0f;
    world.Get<TransformComponent>(child).position_.x = 2.0f;
    world.Get<TransformComponent>(child).parent = firstParent;
    TransformHierarchySystem hierarchy;
    LocalTransformSystem local;
    LocalToWorldSystem global(hierarchy);
    EvaluateTransforms(world, hierarchy, local, global);
    const TransformComponent before = world.Get<TransformComponent>(child);
    GILGAMESH_CHECK_MESSAGE(world.Get<TransformComponent>(firstParent).worldRevision !=
        world.Get<TransformComponent>(secondParent).worldRevision,
        "independent world-matrix publications must receive distinct change tokens");

    // Parent changes must update the world matrix without rewriting the authored local transform.
    GILGAMESH_CHECK_MESSAGE(hierarchy.SetParent(world, child, secondParent),
        "the child must be reparented while preserving its authored local transform");
    EvaluateTransforms(world, hierarchy, local, global);
    const TransformComponent& after = world.Get<TransformComponent>(child);
    GILGAMESH_CHECK_MESSAGE(after.evaluatedParent == secondParent &&
        after.localRevision == before.localRevision && after.worldRevision > before.worldRevision &&
        NearlyEquals(TransformPoint({}, before.localToWorld), Vector3{ 3.0f, 0.0f, 0.0f }) &&
        NearlyEquals(TransformPoint({}, after.localToWorld), Vector3{ 11.0f, 0.0f, 0.0f }),
        "the new parent must change the child's world matrix while local evaluation remains unchanged");
}

GILGAMESH_TEST("Engine.ECS.LocalToWorldSystem", "ConsumesChangedInputsWhenWorldResultIsIdentical")
{
    World world;
    const Entity firstParent = CreateTransform(world);
    const Entity secondParent = CreateTransform(world);
    const Entity child = CreateTransform(world);
    const Entity leaf = CreateTransform(world);
    world.Update(0.0f);
    world.Get<TransformComponent>(firstParent).position_ = { 8.0f, 1.0f, 2.0f };
    world.Get<TransformComponent>(secondParent).position_ = { 8.0f, 1.0f, 2.0f };
    world.Get<TransformComponent>(child).position_.x = 2.0f;
    world.Get<TransformComponent>(child).parent = firstParent;
    world.Get<TransformComponent>(leaf).parent = child;
    TransformHierarchySystem hierarchy;
    LocalTransformSystem local;
    LocalToWorldSystem global(hierarchy);
    EvaluateTransforms(world, hierarchy, local, global);
    const TransformComponent childBefore = world.Get<TransformComponent>(child);
    const std::uint64_t leafRevision = world.Get<TransformComponent>(leaf).worldRevision;

    // A different parent with the same world matrix changes consumed inputs, not published output.
    GILGAMESH_CHECK_MESSAGE(hierarchy.SetParent(world, child, secondParent),
        "equal world matrices must not prevent a valid parent reassignment");
    EvaluateTransforms(world, hierarchy, local, global);
    const TransformComponent afterReparent = world.Get<TransformComponent>(child);
    GILGAMESH_CHECK_MESSAGE(afterReparent.evaluatedParent == secondParent &&
        afterReparent.localToWorld == childBefore.localToWorld &&
        afterReparent.worldRevision == childBefore.worldRevision &&
        world.Get<TransformComponent>(leaf).worldRevision == leafRevision,
        "equal-result reparenting must consume the new identity without advancing world revisions");

    // Opposing local and parent edits also require acknowledgement when their final result cancels out.
    world.Get<TransformComponent>(secondParent).position_.x = 9.0f;
    world.Get<TransformComponent>(child).position_.x = 1.0f;
    EvaluateTransforms(world, hierarchy, local, global);
    const TransformComponent afterCancellation = world.Get<TransformComponent>(child);
    GILGAMESH_CHECK_MESSAGE(afterCancellation.localRevision > afterReparent.localRevision &&
        afterCancellation.evaluatedLocalRevision == afterCancellation.localRevision &&
        afterCancellation.evaluatedParentWorldRevision ==
            world.Get<TransformComponent>(secondParent).worldRevision &&
        afterCancellation.localToWorld == childBefore.localToWorld &&
        afterCancellation.worldRevision == childBefore.worldRevision &&
        world.Get<TransformComponent>(leaf).worldRevision == leafRevision,
        "changed source revisions must be consumed even when the resulting world matrix is unchanged");
    EvaluateTransforms(world, hierarchy, local, global);
    GILGAMESH_CHECK_MESSAGE(world.Get<TransformComponent>(child).worldRevision == childBefore.worldRevision &&
        world.Get<TransformComponent>(child).evaluatedLocalRevision == afterCancellation.localRevision,
        "acknowledged equal results must remain stable on the following evaluation");
}

GILGAMESH_TEST("Engine.ECS.LocalToWorldSystem", "UsesValidatedParentsForInvalidLinksAndCycles")
{
    World world;
    const Entity missingParent = world.CreateEntity();
    const Entity missingChild = CreateTransform(world);
    const Entity selfParented = CreateTransform(world);
    const Entity cycleFirst = CreateTransform(world);
    const Entity cycleSecond = CreateTransform(world);
    const Entity leaf = CreateTransform(world);
    world.Update(0.0f);
    world.Get<TransformComponent>(missingChild).parent = missingParent;
    world.Get<TransformComponent>(missingChild).position_.x = 4.0f;
    world.Get<TransformComponent>(selfParented).parent = selfParented;
    world.Get<TransformComponent>(selfParented).position_.y = 5.0f;
    world.Get<TransformComponent>(cycleFirst).parent = cycleSecond;
    world.Get<TransformComponent>(cycleFirst).position_.x = 1.0f;
    world.Get<TransformComponent>(cycleSecond).parent = cycleFirst;
    world.Get<TransformComponent>(cycleSecond).position_.y = 2.0f;
    world.Get<TransformComponent>(leaf).parent = cycleFirst;
    TransformHierarchySystem hierarchy;
    LocalTransformSystem local;
    LocalToWorldSystem global(hierarchy);

    // The repaired cache determines effective roots without rewriting invalid authored links.
    EvaluateTransforms(world, hierarchy, local, global);
    GILGAMESH_CHECK_MESSAGE(world.Get<TransformComponent>(missingChild).parent == missingParent &&
        world.Get<TransformComponent>(selfParented).parent == selfParented &&
        world.Get<TransformComponent>(missingChild).evaluatedParent.IsNull() &&
        world.Get<TransformComponent>(selfParented).evaluatedParent.IsNull() &&
        world.Get<TransformComponent>(missingChild).localToWorld ==
            world.Get<TransformComponent>(missingChild).localToParent &&
        world.Get<TransformComponent>(selfParented).localToWorld ==
            world.Get<TransformComponent>(selfParented).localToParent,
        "invalid authored parents must evaluate as validated roots rather than being followed directly");
    const auto traversal = hierarchy.GetTraversalOrder();
    GILGAMESH_CHECK_MESSAGE(traversal.size() == 5 &&
        std::all_of(traversal.begin(), traversal.end(), [&](Entity entity)
        {
            const TransformComponent& transform = world.Get<TransformComponent>(entity);
            const Entity parent = hierarchy.GetParent(entity);
            return transform.worldRevision != 0 && transform.evaluatedParent == parent &&
                transform.evaluatedParentWorldRevision == (parent.IsNull() ? 0 :
                    world.Get<TransformComponent>(parent).worldRevision);
        }),
        "every node in a repaired cyclic forest must publish using its validated parent inputs");
}

GILGAMESH_TEST("Engine.ECS.LocalToWorldSystem", "PreservesAffineShearAndNegativeScale")
{
    World world;
    const Entity parent = CreateTransform(world);
    const Entity child = CreateTransform(world);
    world.Update(0.0f);
    world.Get<TransformComponent>(parent).position_ = { 7.0f, 8.0f, 9.0f };
    world.Get<TransformComponent>(parent).scale = { 3.0f, -1.0f, 2.0f };
    world.Get<TransformComponent>(child).parent = parent;
    world.Get<TransformComponent>(child).position_ = { 1.0f, 2.0f, 3.0f };
    world.Get<TransformComponent>(child).rotation_ =
        FromAxisAngle({ 0.0f, 0.0f, 1.0f }, std::numbers::pi_v<float> * 0.25f);
    TransformHierarchySystem hierarchy;
    LocalTransformSystem local;
    LocalToWorldSystem global(hierarchy);

    // Rotated child axes under unequal parent scales become non-orthogonal world axes.
    EvaluateTransforms(world, hierarchy, local, global);
    const TransformComponent& parentTransform = world.Get<TransformComponent>(parent);
    const TransformComponent& childTransform = world.Get<TransformComponent>(child);
    const Vector3 point{ 2.0f, -1.0f, 4.0f };
    const Vector3 expected = ApplyAuthoredTransform(
        ApplyAuthoredTransform(point, childTransform), parentTransform);
    const Vector3 axisX = TransformDirection({ 1.0f, 0.0f, 0.0f }, childTransform.localToWorld);
    const Vector3 axisY = TransformDirection({ 0.0f, 1.0f, 0.0f }, childTransform.localToWorld);
    GILGAMESH_CHECK_MESSAGE(NearlyEquals(TransformPoint(point, childTransform.localToWorld), expected, 1e-4f) &&
        std::fabs(Dot(axisX, axisY)) > 1.0f,
        "world propagation must retain the full affine shear instead of decomposing back into TRS");
}

GILGAMESH_TEST("Engine.ECS.LocalToWorldSystem", "SkipsUnreadyInputsWithoutPublishingOrConsumingThem")
{
    World world;
    const Entity parent = CreateTransform(world);
    const Entity child = CreateTransform(world);
    world.Update(0.0f);
    world.Get<TransformComponent>(child).parent = parent;
    world.Get<TransformComponent>(child).position_.x = 3.0f;
    TransformHierarchySystem hierarchy;
    LocalTransformSystem local;
    LocalToWorldSystem global(hierarchy);
    hierarchy.Update(world, 0.0f);

    // World propagation must not treat default local identity caches as evaluated input.
    global.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(world.Get<TransformComponent>(parent).worldRevision == 0 &&
        world.Get<TransformComponent>(child).worldRevision == 0 &&
        world.Get<TransformComponent>(child).evaluatedParent.IsNull() &&
        world.Get<TransformComponent>(child).evaluatedLocalRevision == 0,
        "unevaluated local matrices must leave published world state and consumed inputs untouched");

    // Simulate a locally ready child whose parent has not published a world matrix yet.
    TransformComponent& childTransform = world.Get<TransformComponent>(child);
    childTransform.localToParent = Matrix4{ Matrix::Identity(), { 3.0f, 0.0f, 0.0f } };
    childTransform.localRevision = 1;
    global.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(childTransform.worldRevision == 0 &&
        childTransform.evaluatedLocalRevision == 0 && childTransform.evaluatedParent.IsNull() &&
        childTransform.localToWorld == Matrix4::Identity(),
        "an unavailable parent world matrix must defer a ready child without consuming its inputs");
    EvaluateTransforms(world, hierarchy, local, global);
    GILGAMESH_CHECK_MESSAGE(world.Get<TransformComponent>(parent).worldRevision != 0 &&
        childTransform.worldRevision != 0 && childTransform.evaluatedParent == parent &&
        childTransform.evaluatedLocalRevision == childTransform.localRevision &&
        NearlyEquals(TransformPoint({}, childTransform.localToWorld), Vector3{ 3.0f, 0.0f, 0.0f }),
        "a later correctly ordered evaluation must recover and publish every deferred transform");
}

GILGAMESH_TEST("Engine.ECS.LocalToWorldSystem", "DetectsReplacementOfParentTransformOnSameEntity")
{
    World world;
    const Entity parent = CreateTransform(world);
    const Entity child = CreateTransform(world);
    world.Update(0.0f);
    world.Get<TransformComponent>(parent).position_.x = 2.0f;
    world.Get<TransformComponent>(child).position_.x = 3.0f;
    world.Get<TransformComponent>(child).parent = parent;
    TransformHierarchySystem hierarchy;
    LocalTransformSystem local;
    LocalToWorldSystem global(hierarchy);
    EvaluateTransforms(world, hierarchy, local, global);
    const std::uint64_t hierarchyRevision = hierarchy.GetRevision();
    const std::uint64_t parentRevision = world.Get<TransformComponent>(parent).worldRevision;
    const TransformComponent childBefore = world.Get<TransformComponent>(child);

    // Component replacement starts fresh caches without changing Entity identity or topology.
    TransformComponent replacement;
    replacement.position_.x = 10.0f;
    world.RemoveComponent<TransformComponent>(parent);
    world.Emplace<TransformComponent>(parent, replacement);
    world.Update(0.0f);
    EvaluateTransforms(world, hierarchy, local, global);
    const TransformComponent& parentAfter = world.Get<TransformComponent>(parent);
    const TransformComponent& childAfter = world.Get<TransformComponent>(child);
    GILGAMESH_CHECK_MESSAGE(world.IsAlive(parent) && hierarchy.GetRevision() == hierarchyRevision &&
        parentAfter.worldRevision > parentRevision && childAfter.evaluatedParent == parent &&
        childAfter.evaluatedParentWorldRevision == parentAfter.worldRevision &&
        childAfter.worldRevision > childBefore.worldRevision &&
        NearlyEquals(TransformPoint({}, childAfter.localToWorld), Vector3{ 13.0f, 0.0f, 0.0f }),
        "a replaced parent component must receive a fresh token and refresh its existing descendants");

    // Even replacement with identical authored values must never reuse an earlier publication token.
    const std::uint64_t previousParentRevision = parentAfter.worldRevision;
    const TransformComponent childBeforeEqualReplacement = childAfter;
    world.RemoveComponent<TransformComponent>(parent);
    world.Emplace<TransformComponent>(parent, replacement);
    world.Update(0.0f);
    EvaluateTransforms(world, hierarchy, local, global);
    const TransformComponent& equalParent = world.Get<TransformComponent>(parent);
    const TransformComponent& equalChild = world.Get<TransformComponent>(child);
    GILGAMESH_CHECK_MESSAGE(equalParent.worldRevision > previousParentRevision &&
        equalChild.evaluatedParentWorldRevision == equalParent.worldRevision &&
        equalChild.localToWorld == childBeforeEqualReplacement.localToWorld &&
        equalChild.worldRevision == childBeforeEqualReplacement.worldRevision,
        "identical replacement must publish a fresh parent token while an unchanged child retains its output token");
}

GILGAMESH_TEST("Engine.ECS.LocalToWorldSystem", "RegisteredPipelineEvaluatesWhileSimulationIsPaused")
{
    World world;
    const Entity child = CreateTransform(world);
    const Entity parent = CreateTransform(world);
    world.Update(0.0f);
    world.Get<TransformComponent>(child).parent = parent;
    world.Get<TransformComponent>(child).position_.x = 2.0f;
    world.Get<TransformComponent>(parent).position_.x = 5.0f;

    // Composition explicitly registers and enables dependencies in presentation order.
    auto hierarchy = std::make_unique<TransformHierarchySystem>();
    TransformHierarchySystem& hierarchyReference = *hierarchy;
    SystemGroup& transforms = world.GetTransformSystems();
    transforms.AddSystem(std::move(hierarchy)).SetEnabled(true);
    transforms.AddSystem(std::make_unique<LocalTransformSystem>()).SetEnabled(true);
    transforms.AddSystem(std::make_unique<LocalToWorldSystem>(hierarchyReference)).SetEnabled(true);
    world.SetSimulationPaused(true);
    world.PreparePresentation(0.0f);
    GILGAMESH_CHECK_MESSAGE(NearlyEquals(
        TransformPoint({}, world.Get<TransformComponent>(child).localToWorld),
        Vector3{ 7.0f, 0.0f, 0.0f }),
        "the registered presentation pipeline must evaluate transform dependencies while paused");

    // Editor-style source changes must become visible on the next paused presentation.
    world.Get<TransformComponent>(parent).position_.x = 9.0f;
    world.PreparePresentation(0.0f);
    GILGAMESH_CHECK_MESSAGE(world.IsSimulationPaused() && NearlyEquals(
        TransformPoint({}, world.Get<TransformComponent>(child).localToWorld),
        Vector3{ 11.0f, 0.0f, 0.0f }),
        "paused presentation must propagate later authored ancestor changes without simulation updates");
}
