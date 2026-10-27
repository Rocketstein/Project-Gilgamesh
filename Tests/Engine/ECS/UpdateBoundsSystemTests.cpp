#include <algorithm>
#include <cstdint>
#include <limits>
#include <numbers>

#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/ECS/Component/BoundsComponent.h"
#include "Engine/ECS/Component/MeshComponent.h"
#include "Engine/ECS/Component/TransformComponent.h"
#include "Engine/ECS/Framework/World.h"
#include "Engine/ECS/System/Transform/LocalToWorldSystem.h"
#include "Engine/ECS/System/Transform/LocalTransformSystem.h"
#include "Engine/ECS/System/Transform/TransformHierarchySystem.h"
#include "Engine/ECS/System/Transform/UpdateBoundsSystem.h"
#include "Engine/Render/Mesh/MeshManager.h"
#include "Tests/TestFramework.h"

namespace
{
// Upload actual mesh resources through a software device without requiring a physical GPU.
ID3D11Device* GetTestDevice()
{
    static const Microsoft::WRL::ComPtr<ID3D11Device> device = []
    {
        Microsoft::WRL::ComPtr<ID3D11Device> result;
        const HRESULT created = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, result.GetAddressOf(), nullptr, nullptr);
        if (FAILED(created)) result.Reset();
        return result;
    }();
    return device.Get();
}

// A triangle containing both supplied extrema gives the uploaded mesh a known local AABB.
StaticMeshData MakeBoundsMesh(Vector3 minimum, Vector3 maximum)
{
    StaticMeshData data;
    data.vertices_.resize(3);
    data.vertices_[0].position = minimum;
    data.vertices_[1].position = maximum;
    data.vertices_[2].position = { maximum.x, minimum.y, maximum.z };
    data.indices_ = { 0, 1, 2 };
    return data;
}

// Queue all components required by the bounds system before structural playback.
Entity CreateBoundedEntity(World& world, MeshHandle mesh)
{
    const Entity entity = world.CreateEntity();
    world.Emplace<TransformComponent>(entity);
    world.Emplace<MeshComponent>(entity, MeshComponent{ mesh });
    world.Emplace<AABBComponent>(entity);
    return entity;
}

// Evaluate production transform dependencies while keeping the bounds update independently observable.
void EvaluateWorldTransforms(World& world)
{
    TransformHierarchySystem hierarchy;
    LocalTransformSystem local;
    LocalToWorldSystem global(hierarchy);
    hierarchy.Update(world, 0.0f);
    local.Update(world, 0.0f);
    global.Update(world, 0.0f);
}

// Transform points directly from authored SRT for expectations independent of AABB transformation.
Vector3 ApplyAuthoredTransform(Vector3 point, const TransformComponent& transform)
{
    return Rotate(Scale(point, transform.scale), transform.rotation_) + transform.position_;
}
} // Anonymous namespace

GILGAMESH_TEST("Engine.ECS.UpdateBoundsSystem", "ComputesPerInstanceBoundsFromSharedMeshMetadata")
{
    MeshManager meshes;
    GILGAMESH_CHECK_MESSAGE(meshes.Initialize(GetTestDevice()), "a WARP device must be available");
    if (!meshes.IsInitialized()) return;
    const MeshHandle mesh = meshes.CreateMesh(MakeBoundsMesh({ -1.0f, -2.0f, -3.0f }, { 1.0f, 2.0f, 3.0f }));
    GILGAMESH_CHECK_MESSAGE(meshes.IsValid(mesh), "the shared mesh must upload successfully");
    World world;
    const Entity first = CreateBoundedEntity(world, mesh);
    const Entity second = CreateBoundedEntity(world, mesh);
    world.Update(0.0f);
    world.Get<TransformComponent>(first).position_ = { 10.0f, 20.0f, 30.0f };
    world.Get<TransformComponent>(first).scale = { 2.0f, 3.0f, 4.0f };
    world.Get<TransformComponent>(second).position_.x = -10.0f;
    EvaluateWorldTransforms(world);
    const TransformComponent originalTransform = world.Get<TransformComponent>(first);

    // Instances share local metadata but publish independently transformed world boxes.
    UpdateBoundsSystem system(meshes);
    system.Update(world, 0.0f);
    const AABBComponent& firstBounds = world.Get<AABBComponent>(first);
    const AABBComponent& secondBounds = world.Get<AABBComponent>(second);
    BoundingBox localBounds;
    std::uint64_t localRevision = 0;
    GILGAMESH_CHECK_MESSAGE(meshes.TryGetLocalBounds(mesh, localBounds, localRevision) &&
        localBounds.min == Vector3(-1.0f, -2.0f, -3.0f) &&
        localBounds.max == Vector3(1.0f, 2.0f, 3.0f) &&
        firstBounds.valid && secondBounds.valid && firstBounds.revision != 0 &&
        secondBounds.revision != 0 && firstBounds.revision != secondBounds.revision &&
        firstBounds.AABB.min == Vector3(8.0f, 14.0f, 18.0f) &&
        firstBounds.AABB.max == Vector3(12.0f, 26.0f, 42.0f) &&
        secondBounds.AABB.min == Vector3(-11.0f, -2.0f, -3.0f) &&
        secondBounds.AABB.max == Vector3(-9.0f, 2.0f, 3.0f) &&
        firstBounds.sourceMesh == mesh && secondBounds.sourceMesh == mesh &&
        firstBounds.sourceMeshBoundsRevision == localRevision &&
        firstBounds.sourceWorldRevision == originalTransform.worldRevision,
        "each instance must transform the original shared local AABB and record its source revisions");

    // Bounds evaluation reads authored and evaluated transforms without clearing their versions.
    const TransformComponent& afterTransform = world.Get<TransformComponent>(first);
    GILGAMESH_CHECK_MESSAGE(afterTransform.position_ == originalTransform.position_ &&
        afterTransform.rotation_ == originalTransform.rotation_ && afterTransform.scale == originalTransform.scale &&
        afterTransform.localToParent == originalTransform.localToParent &&
        afterTransform.localToWorld == originalTransform.localToWorld &&
        afterTransform.localRevision == originalTransform.localRevision &&
        afterTransform.worldRevision == originalTransform.worldRevision,
        "bounds evaluation must leave authored transform data, matrices, and revisions unchanged");
}

GILGAMESH_TEST("Engine.ECS.UpdateBoundsSystem", "ConsumesChangedInputsWithoutRepublishingIdenticalBounds")
{
    MeshManager meshes;
    GILGAMESH_CHECK_MESSAGE(meshes.Initialize(GetTestDevice()), "a WARP device must be available");
    if (!meshes.IsInitialized()) return;
    const StaticMeshData geometry = MakeBoundsMesh({ -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f });
    const MeshHandle firstMesh = meshes.CreateMesh(geometry);
    const MeshHandle secondMesh = meshes.CreateMesh(geometry);
    GILGAMESH_CHECK_MESSAGE(meshes.IsValid(firstMesh) && meshes.IsValid(secondMesh),
        "both independently uploaded meshes must be valid");
    World world;
    const Entity entity = CreateBoundedEntity(world, firstMesh);
    world.Update(0.0f);
    EvaluateWorldTransforms(world);
    UpdateBoundsSystem system(meshes);
    system.Update(world, 0.0f);
    const AABBComponent original = world.Get<AABBComponent>(entity);
    const std::uint64_t originalWorldRevision = world.Get<TransformComponent>(entity).worldRevision;

    // Mirroring a symmetric local box changes the matrix while preserving its world enclosure.
    world.Get<TransformComponent>(entity).scale.x = -1.0f;
    EvaluateWorldTransforms(world);
    system.Update(world, 0.0f);
    const AABBComponent afterMirror = world.Get<AABBComponent>(entity);
    GILGAMESH_CHECK_MESSAGE(afterMirror.valid && afterMirror.revision == original.revision &&
        afterMirror.AABB.min == original.AABB.min && afterMirror.AABB.max == original.AABB.max &&
        afterMirror.sourceWorldRevision == world.Get<TransformComponent>(entity).worldRevision &&
        afterMirror.sourceWorldRevision > originalWorldRevision,
        "equal-result matrix changes must be consumed without issuing a new bounds token");

    // Switching resource identity must also be acknowledged even when both meshes have equal bounds.
    world.Get<MeshComponent>(entity).mesh = secondMesh;
    system.Update(world, 0.0f);
    const AABBComponent afterSwitch = world.Get<AABBComponent>(entity);
    system.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(afterSwitch.sourceMesh == secondMesh &&
        afterSwitch.revision == original.revision &&
        world.Get<AABBComponent>(entity).revision == original.revision &&
        world.Get<AABBComponent>(entity).sourceWorldRevision == afterMirror.sourceWorldRevision,
        "equal-result mesh changes and subsequent unchanged updates must retain the published bounds token");
}

GILGAMESH_TEST("Engine.ECS.UpdateBoundsSystem", "PublishesInitialInvalidStateAndRecoversWhenSourcesAreReady")
{
    MeshManager meshes;
    GILGAMESH_CHECK_MESSAGE(meshes.Initialize(GetTestDevice()), "a WARP device must be available");
    if (!meshes.IsInitialized()) return;
    const MeshHandle mesh = meshes.CreateMesh(MakeBoundsMesh({ -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f }));
    GILGAMESH_CHECK_MESSAGE(meshes.IsValid(mesh), "the recovery mesh must upload successfully");
    World world;
    const Entity entity = CreateBoundedEntity(world, mesh);
    world.Update(0.0f);
    UpdateBoundsSystem system(meshes);

    // An unevaluated world matrix is an evaluated invalid result, not a valid identity box.
    system.Update(world, 0.0f);
    const AABBComponent unavailable = world.Get<AABBComponent>(entity);
    system.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(!unavailable.valid && unavailable.revision != 0 &&
        unavailable.sourceWorldRevision == 0 && unavailable.sourceMesh == mesh &&
        unavailable.sourceMeshBoundsRevision != 0 &&
        world.Get<AABBComponent>(entity).revision == unavailable.revision &&
        world.Get<TransformComponent>(entity).worldRevision == 0,
        "first invalid evaluation must publish and consume inputs without inventing a world transform");

    // Later transform production makes the same resource and component usable.
    EvaluateWorldTransforms(world);
    system.Update(world, 0.0f);
    const AABBComponent recovered = world.Get<AABBComponent>(entity);
    GILGAMESH_CHECK_MESSAGE(recovered.valid && recovered.revision > unavailable.revision,
        "ready world input must transition previously invalid bounds to valid");
    world.Get<MeshComponent>(entity).mesh = {};
    system.Update(world, 0.0f);
    const AABBComponent missingMesh = world.Get<AABBComponent>(entity);
    system.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(!missingMesh.valid && missingMesh.revision > recovered.revision &&
        missingMesh.sourceMeshBoundsRevision == 0 &&
        world.Get<AABBComponent>(entity).revision == missingMesh.revision,
        "missing geometry must publish invalidation once and consume its unavailable metadata state");
    world.Get<MeshComponent>(entity).mesh = mesh;
    system.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(world.Get<AABBComponent>(entity).valid &&
        world.Get<AABBComponent>(entity).revision > missingMesh.revision,
        "reassigning available geometry must restore valid bounds");
}

GILGAMESH_TEST("Engine.ECS.UpdateBoundsSystem", "InvalidatesRemovedMeshAndRespectsReplacementGeneration")
{
    MeshManager meshes;
    GILGAMESH_CHECK_MESSAGE(meshes.Initialize(GetTestDevice()), "a WARP device must be available");
    if (!meshes.IsInitialized()) return;
    const MeshHandle removed = meshes.CreateMesh(MakeBoundsMesh({ -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f }));
    GILGAMESH_CHECK_MESSAGE(meshes.IsValid(removed), "the removable mesh must upload successfully");
    World world;
    const Entity entity = CreateBoundedEntity(world, removed);
    world.Update(0.0f);
    EvaluateWorldTransforms(world);
    UpdateBoundsSystem system(meshes);
    system.Update(world, 0.0f);
    const AABBComponent before = world.Get<AABBComponent>(entity);

    // Removal changes metadata availability while the entity still stores the old mesh handle.
    GILGAMESH_CHECK_MESSAGE(meshes.RemoveMesh(removed), "the source resource must be removable");
    system.Update(world, 0.0f);
    const AABBComponent invalid = world.Get<AABBComponent>(entity);
    GILGAMESH_CHECK_MESSAGE(!invalid.valid && invalid.revision > before.revision &&
        invalid.sourceMesh == removed && invalid.sourceMeshBoundsRevision == 0 &&
        invalid.sourceWorldRevision == before.sourceWorldRevision,
        "resource deletion must invalidate bounds even when transform and stored mesh identity are unchanged");

    // Reusing a slot cannot make the stale component handle refer to the new resource.
    const MeshHandle replacement = meshes.CreateMesh(MakeBoundsMesh({ -4.0f, -2.0f, -3.0f }, { 5.0f, 2.0f, 3.0f }));
    GILGAMESH_CHECK_MESSAGE(meshes.IsValid(replacement) && replacement != removed,
        "a new resource in a recycled slot must use a distinct generation");
    system.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(!world.Get<AABBComponent>(entity).valid &&
        world.Get<AABBComponent>(entity).revision == invalid.revision,
        "a replacement generation must not revive a stale mesh reference");
    world.Get<MeshComponent>(entity).mesh = replacement;
    system.Update(world, 0.0f);
    const AABBComponent& recovered = world.Get<AABBComponent>(entity);
    GILGAMESH_CHECK_MESSAGE(recovered.valid && recovered.revision > invalid.revision &&
        recovered.AABB.min == Vector3(-4.0f, -2.0f, -3.0f) &&
        recovered.AABB.max == Vector3(5.0f, 2.0f, 3.0f),
        "explicitly assigning the replacement must evaluate its new local bounds");
}

GILGAMESH_TEST("Engine.ECS.UpdateBoundsSystem", "EnclosesAffineShearWithoutAggregatingDescendantBounds")
{
    MeshManager meshes;
    GILGAMESH_CHECK_MESSAGE(meshes.Initialize(GetTestDevice()), "a WARP device must be available");
    if (!meshes.IsInitialized()) return;
    const MeshHandle mesh = meshes.CreateMesh(MakeBoundsMesh({ -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f }));
    GILGAMESH_CHECK_MESSAGE(meshes.IsValid(mesh), "the affine test mesh must upload successfully");
    World world;
    const Entity parent = CreateBoundedEntity(world, mesh);
    const Entity child = CreateBoundedEntity(world, mesh);
    world.Update(0.0f);
    world.Get<TransformComponent>(parent).position_ = { 10.0f, 20.0f, 30.0f };
    world.Get<TransformComponent>(parent).scale = { 3.0f, -1.0f, 2.0f };
    world.Get<TransformComponent>(child).parent = parent;
    world.Get<TransformComponent>(child).position_.x = 100.0f;
    world.Get<TransformComponent>(child).scale = { -2.0f, 0.5f, 1.0f };
    world.Get<TransformComponent>(child).rotation_ =
        FromAxisAngle({ 0.0f, 0.0f, 1.0f }, std::numbers::pi_v<float> * 0.25f);
    EvaluateWorldTransforms(world);
    UpdateBoundsSystem system(meshes);
    system.Update(world, 0.0f);

    // Independently transform all original local-box corners through authored child and parent SRT.
    const TransformComponent& parentTransform = world.Get<TransformComponent>(parent);
    const TransformComponent& childTransform = world.Get<TransformComponent>(child);
    const float infinity = std::numeric_limits<float>::infinity();
    Vector3 expectedMin{ infinity, infinity, infinity };
    Vector3 expectedMax{ -infinity, -infinity, -infinity };
    for (unsigned corner = 0; corner < 8; ++corner)
    {
        const Vector3 localPoint{
            (corner & 1) ? 1.0f : -1.0f,
            (corner & 2) ? 1.0f : -1.0f,
            (corner & 4) ? 1.0f : -1.0f
        };
        const Vector3 point = ApplyAuthoredTransform(
            ApplyAuthoredTransform(localPoint, childTransform), parentTransform);
        expectedMin = { (std::min)(expectedMin.x, point.x), (std::min)(expectedMin.y, point.y),
            (std::min)(expectedMin.z, point.z) };
        expectedMax = { (std::max)(expectedMax.x, point.x), (std::max)(expectedMax.y, point.y),
            (std::max)(expectedMax.z, point.z) };
    }
    const AABBComponent& childBounds = world.Get<AABBComponent>(child);
    const AABBComponent& parentBounds = world.Get<AABBComponent>(parent);
    GILGAMESH_CHECK_MESSAGE(childBounds.valid && parentBounds.valid &&
        NearlyEquals(childBounds.AABB.min, expectedMin, 1e-4f) &&
        NearlyEquals(childBounds.AABB.max, expectedMax, 1e-4f) &&
        parentBounds.AABB.min == Vector3(7.0f, 19.0f, 28.0f) &&
        parentBounds.AABB.max == Vector3(13.0f, 21.0f, 32.0f) &&
        childBounds.AABB.min.x > parentBounds.AABB.max.x,
        "world bounds must enclose affine corners while each parent retains only its own mesh bounds");
}

GILGAMESH_TEST("Engine.ECS.UpdateBoundsSystem", "RecreatedBoundsReceiveFreshPublicationTokens")
{
    MeshManager meshes;
    GILGAMESH_CHECK_MESSAGE(meshes.Initialize(GetTestDevice()), "a WARP device must be available");
    if (!meshes.IsInitialized()) return;
    const MeshHandle mesh = meshes.CreateMesh(MakeBoundsMesh({ -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f }));
    GILGAMESH_CHECK_MESSAGE(meshes.IsValid(mesh), "the replacement test mesh must upload successfully");
    World world;
    const Entity entity = CreateBoundedEntity(world, mesh);
    world.Update(0.0f);
    EvaluateWorldTransforms(world);
    UpdateBoundsSystem system(meshes);
    system.Update(world, 0.0f);
    const AABBComponent original = world.Get<AABBComponent>(entity);

    // Structural replacement starts unevaluated bounds on the same live Entity identity.
    world.RemoveComponent<AABBComponent>(entity);
    world.Emplace<AABBComponent>(entity);
    world.Update(0.0f);
    system.Update(world, 0.0f);
    const AABBComponent& replacement = world.Get<AABBComponent>(entity);
    GILGAMESH_CHECK_MESSAGE(replacement.valid && replacement.revision > original.revision &&
        replacement.AABB.min == original.AABB.min && replacement.AABB.max == original.AABB.max &&
        replacement.sourceWorldRevision == original.sourceWorldRevision,
        "recreated bounds must issue a fresh token even when their geometry and transform are unchanged");
}

GILGAMESH_TEST("Engine.ECS.UpdateBoundsSystem", "InvalidWorldResultsRemainStableUntilRecovery")
{
    MeshManager meshes;
    GILGAMESH_CHECK_MESSAGE(meshes.Initialize(GetTestDevice()), "a WARP device must be available");
    if (!meshes.IsInitialized()) return;
    const MeshHandle mesh = meshes.CreateMesh(MakeBoundsMesh({ -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f }));
    GILGAMESH_CHECK_MESSAGE(meshes.IsValid(mesh), "the invalid-input test mesh must upload successfully");
    World world;
    const Entity entity = CreateBoundedEntity(world, mesh);
    world.Update(0.0f);
    EvaluateWorldTransforms(world);
    UpdateBoundsSystem system(meshes);
    system.Update(world, 0.0f);
    const std::uint64_t validRevision = world.Get<AABBComponent>(entity).revision;

    // Nonfinite authored input reaches the world matrix and invalidates previously usable bounds.
    world.Get<TransformComponent>(entity).position_.x = std::numeric_limits<float>::quiet_NaN();
    EvaluateWorldTransforms(world);
    system.Update(world, 0.0f);
    const AABBComponent firstInvalid = world.Get<AABBComponent>(entity);
    GILGAMESH_CHECK_MESSAGE(!firstInvalid.valid && firstInvalid.revision > validRevision,
        "a nonfinite world matrix must invalidate the last valid world bounds");

    // NaN matrices may be republished by transform evaluation, but invalid bounds remain the same result.
    EvaluateWorldTransforms(world);
    system.Update(world, 0.0f);
    const AABBComponent repeatedInvalid = world.Get<AABBComponent>(entity);
    GILGAMESH_CHECK_MESSAGE(!repeatedInvalid.valid && repeatedInvalid.revision == firstInvalid.revision &&
        repeatedInvalid.sourceWorldRevision == world.Get<TransformComponent>(entity).worldRevision &&
        repeatedInvalid.sourceWorldRevision > firstInvalid.sourceWorldRevision,
        "invalid-to-invalid evaluations must consume new source tokens without republishing bounds");
    world.Get<TransformComponent>(entity).position_.x = 0.0f;
    EvaluateWorldTransforms(world);
    system.Update(world, 0.0f);
    GILGAMESH_CHECK_MESSAGE(world.Get<AABBComponent>(entity).valid &&
        world.Get<AABBComponent>(entity).revision > firstInvalid.revision,
        "finite source recovery must publish usable bounds again");
}
