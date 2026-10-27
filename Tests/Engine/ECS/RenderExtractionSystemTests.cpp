#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/ECS/Component/BoundsComponent.h"
#include "Engine/ECS/Component/MeshComponent.h"
#include "Engine/ECS/Component/TransformComponent.h"
#include "Engine/ECS/Framework/World.h"
#include "Engine/ECS/System/Core/SystemGroup.h"
#include "Engine/ECS/System/RenderExtractionSystem.h"
#include "Engine/ECS/System/Transform/LocalToWorldSystem.h"
#include "Engine/ECS/System/Transform/LocalTransformSystem.h"
#include "Engine/ECS/System/Transform/TransformHierarchySystem.h"
#include "Engine/ECS/System/Transform/UpdateBoundsSystem.h"
#include "Engine/Render/Mesh/MeshManager.h"
#include "Tests/TestFramework.h"

namespace
{
// Reuse a software device solely to obtain genuine opaque mesh handles for extraction.
ID3D11Device* GetTestDevice()
{
	static const auto device = []
	{
		Microsoft::WRL::ComPtr<ID3D11Device> result;
		const HRESULT status = D3D11CreateDevice(
			nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
			D3D11_SDK_VERSION, result.GetAddressOf(), nullptr, nullptr);
		if (FAILED(status)) result.Reset();
		return result;
	}();
	return device.Get();
}

// Known extrema support bounds integration; extraction itself only copies the opaque handle.
MeshHandle CreateTestMesh(MeshManager& meshes)
{
	if (!meshes.Initialize(GetTestDevice())) return {};
	StaticMeshData data;
	data.vertices_.resize(3);
	data.vertices_[0].position = { -1.0f, -2.0f, -3.0f };
	data.vertices_[1].position = { 1.0f, 2.0f, 3.0f };
	data.vertices_[2].position = { 1.0f, -2.0f, 3.0f };
	data.indices_ = { 0, 1, 2 };
	return meshes.CreateMesh(data);
}

// Register evaluated-transform producers before the enabled world-owned extractor.
RenderExtractionSystem& AddExtractor(World& world)
{
	SystemGroup& transforms = world.GetTransformSystems();
	auto hierarchy = std::make_unique<TransformHierarchySystem>();
	TransformHierarchySystem& hierarchyReference = *hierarchy;
	transforms.AddSystem(std::move(hierarchy)).SetEnabled(true);
	transforms.AddSystem(std::make_unique<LocalTransformSystem>()).SetEnabled(true);
	transforms.AddSystem(std::make_unique<LocalToWorldSystem>(hierarchyReference)).SetEnabled(true);

	// Tests retain an observer only until the owning World tears down its extraction group.
	auto system = std::make_unique<RenderExtractionSystem>();
	RenderExtractionSystem& observer = *system;
	system->SetEnabled(true);
	world.GetExtractionSystems().AddSystem(std::move(system));
	return observer;
}

// Queue a complete mesh instance before the next world playback boundary.
Entity AddMeshEntity(
	World& world,
	SceneHandle scene,
	MeshHandle mesh,
	TransformComponent transform = {})
{
	const Entity entity = world.CreateEntity(scene);
	world.Emplace<TransformComponent>(entity, transform);
	world.Emplace<MeshComponent>(entity, MeshComponent{ mesh });
	return entity;
}

// Recognize twelve unique axis edges without depending on their emitted direction or order.
bool HasAABBWireframe(const std::vector<DebugLine>& lines, const BoundingBox& bounds)
{
	if (lines.size() != 12) return false;
	std::array<Vector3, 8> corners;
	for (std::size_t index = 0; index < corners.size(); ++index)
	{
		corners[index] = {
			(index & 1) ? bounds.max.x : bounds.min.x,
			(index & 2) ? bounds.max.y : bounds.min.y,
			(index & 4) ? bounds.max.z : bounds.min.z
		};
	}

	// Corner indices differ by one bit exactly when their endpoints form a box edge.
	std::set<std::pair<std::size_t, std::size_t>> edges;
	for (const DebugLine& line : lines)
	{
		const auto start = std::find(corners.begin(), corners.end(), line.start);
		const auto end = std::find(corners.begin(), corners.end(), line.end);
		if (start == corners.end() || end == corners.end() ||
			line.color != Vector3(0.0f, 1.0f, 0.0f)) return false;
		const std::size_t first = static_cast<std::size_t>(start - corners.begin());
		const std::size_t second = static_cast<std::size_t>(end - corners.begin());
		const std::size_t difference = first ^ second;
		if (difference != 1 && difference != 2 && difference != 4) return false;
		if (!edges.emplace(std::min(first, second), std::max(first, second)).second) return false;
	}
	return edges.size() == 12;
}

// Provides a visible transform-phase result independent of the simulation phase.
class SetPositionSystem final : public ISystem
{
public:
	SetPositionSystem(Entity entity, float position)
		: entity_(entity), position_(position) {}

	void Update(World& world, float /*deltaTime*/) override
	{
		world.Get<TransformComponent>(entity_).position_.x = position_;
	}

private:
	Entity entity_; // Test entity whose final world transform is evaluated.
	float position_; // Distinguishes the simulation and transform phases.
};
} // Anonymous namespace

GILGAMESH_TEST("Engine.ECS.RenderExtractionSystem", "DefaultsToNeutralTransform")
{
	const TransformComponent transform;
	const MeshComponent mesh;
	const Vector3 unitScale{ 1.0f, 1.0f, 1.0f };
	GILGAMESH_CHECK_MESSAGE(transform.position_ == Vector3{} &&
		transform.scale == unitScale &&
		transform.rotation_ == Quat::Identity(),
		"a new transform must preserve geometry with zero position and unit scale");
	GILGAMESH_CHECK_MESSAGE(!mesh.mesh.IsValid(),
		"a new mesh component must explicitly await geometry assignment");
	const DebugLine line;
	GILGAMESH_CHECK_MESSAGE(line.start == Vector3{} && line.end == Vector3{} &&
		line.color == Vector3(1.0f, 1.0f, 1.0f),
		"a default diagnostic segment must have neutral endpoints and a white color");
}

GILGAMESH_TEST("Engine.ECS.RenderExtractionSystem", "SelectsOnlyCompleteActiveSceneInstances")
{
	MeshManager meshes;
	const MeshHandle mesh = CreateTestMesh(meshes);
	GILGAMESH_CHECK_MESSAGE(mesh.IsValid(), "a WARP mesh handle must be available");
	if (!mesh.IsValid()) return;

	World world;
	RenderExtractionSystem& extraction = AddExtractor(world);
	const SceneHandle firstScene = world.CreateScene("First");
	const SceneHandle secondScene = world.CreateScene("Second");
	TransformComponent firstTransform;
	firstTransform.position_.x = 3.0f;
	AddMeshEntity(world, firstScene, mesh, firstTransform);
	TransformComponent secondTransform;
	secondTransform.position_.x = 8.0f;
	AddMeshEntity(world, secondScene, mesh, secondTransform);

	// Incomplete, unassigned, and world-only entities must not enter the active scene result.
	const Entity transformOnly = world.CreateEntity(firstScene);
	world.Emplace<TransformComponent>(transformOnly);
	const Entity meshOnly = world.CreateEntity(firstScene);
	world.Emplace<MeshComponent>(meshOnly, MeshComponent{ mesh });
	AddMeshEntity(world, firstScene, {});
	const Entity worldOnly = world.CreateEntity();
	world.Emplace<TransformComponent>(worldOnly);
	world.Emplace<MeshComponent>(worldOnly, MeshComponent{ mesh });

	world.PreparePresentation(0.0f);
	auto inactive = extraction.TakeFrame();
	GILGAMESH_CHECK_MESSAGE(inactive && inactive->staticMeshes.empty(),
		"creating scenes must not implicitly select an extraction source");

	GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(firstScene),
		"a live scene must be selectable");
	world.PreparePresentation(0.0f);
	auto first = extraction.TakeFrame();
	GILGAMESH_CHECK_MESSAGE(first && first->staticMeshes.size() == 1,
		"only a complete mesh instance in the active scene must be extracted");
	if (first && first->staticMeshes.size() == 1)
	{
		GILGAMESH_CHECK_MESSAGE(first->staticMeshes[0].mesh == mesh &&
			first->staticMeshes[0].localToWorld.Translation().x == 3.0f,
			"the selected scene must supply its own copied instance data");
	}

	world.RequestSetActiveScene(secondScene);
	world.PreparePresentation(0.0f);
	auto second = extraction.TakeFrame();
	GILGAMESH_CHECK_MESSAGE(second && second->staticMeshes.size() == 1 &&
		second->staticMeshes[0].localToWorld.Translation().x == 8.0f,
		"a scene switch must replace the old scene on the next presentation boundary");

	world.RequestClearActiveScene();
	world.PreparePresentation(0.0f);
	auto cleared = extraction.TakeFrame();
	GILGAMESH_CHECK_MESSAGE(cleared && cleared->staticMeshes.empty(),
		"clearing the active scene must publish an empty frame instead of stale draws");

	// A newer empty result must also replace an old result that was never consumed.
	world.RequestSetActiveScene(firstScene);
	world.PreparePresentation(0.0f);
	world.RequestClearActiveScene();
	world.PreparePresentation(0.0f);
	auto replaced = extraction.TakeFrame();
	GILGAMESH_CHECK_MESSAGE(replaced && replaced->staticMeshes.empty(),
		"a new extraction must replace unconsumed data from a previous scene");
}

GILGAMESH_TEST("Engine.ECS.RenderExtractionSystem", "OwnsTransformsAfterSceneAndWorldDestruction")
{
	MeshManager meshes;
	const MeshHandle mesh = CreateTestMesh(meshes);
	GILGAMESH_CHECK_MESSAGE(mesh.IsValid(), "a WARP mesh handle must be available");
	if (!mesh.IsValid()) return;

	std::optional<RenderFrameData> retained;
	{
		World world;
		RenderExtractionSystem& extraction = AddExtractor(world);
		const SceneHandle scene = world.CreateScene();
		TransformComponent transform;
		transform.position_ = { 10.0f, 20.0f, 30.0f };
		transform.scale = { 2.0f, 3.0f, 4.0f };
		transform.rotation_ = FromAxisAngle({ 0.0f, 0.0f, 1.0f },
			1.5707963267948966f);
		const Entity entity = AddMeshEntity(world, scene, mesh, transform);
		world.RequestSetActiveScene(scene);
		world.PreparePresentation(0.0f);
		retained = extraction.TakeFrame();

		// A transferred result is independent of later value changes and structural teardown.
		world.Get<TransformComponent>(entity).position_ = {};
		world.RequestDestroyScene(scene);
		world.PreparePresentation(0.0f);
		auto destroyed = extraction.TakeFrame();
		GILGAMESH_CHECK_MESSAGE(destroyed && destroyed->staticMeshes.empty(),
			"destroying the active scene must publish an empty frame");
	}

	GILGAMESH_CHECK_MESSAGE(retained && retained->staticMeshes.size() == 1,
		"the consumer must own its snapshot after the world and extractor are destroyed");
	if (!retained || retained->staticMeshes.size() != 1) return;

	// Nonuniform scale and a quarter turn distinguish S*R*T from incorrect composition orders.
	const Vector4 transformed = Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }
		* retained->staticMeshes[0].localToWorld;
	GILGAMESH_CHECK_MESSAGE(std::fabs(transformed.x - 7.0f) < 1e-5f &&
		std::fabs(transformed.y - 22.0f) < 1e-5f &&
		std::fabs(transformed.z - 34.0f) < 1e-5f && transformed.w == 1.0f,
		"extraction must copy the row-vector scale, rotation, and translation result");
}

GILGAMESH_TEST("Engine.ECS.RenderExtractionSystem", "ConsumesOnceAndClearsOnLifecycleChanges")
{
	World world;
	RenderExtractionSystem& extraction = AddExtractor(world);
	GILGAMESH_CHECK_MESSAGE(!extraction.TakeFrame(),
		"no frame must exist before extraction runs");
	world.PreparePresentation(0.0f);
	GILGAMESH_CHECK_MESSAGE(extraction.TakeFrame().has_value() && !extraction.TakeFrame(),
		"even an empty completed frame must transfer exactly once");

	// Explicit discard and disable both invalidate any completed result.
	world.PreparePresentation(0.0f);
	extraction.DiscardFrame();
	GILGAMESH_CHECK_MESSAGE(!extraction.TakeFrame(),
		"discard must remove an unconsumed frame");
	world.PreparePresentation(0.0f);
	extraction.SetEnabled(false);
	GILGAMESH_CHECK_MESSAGE(!extraction.TakeFrame(),
		"disabling the system must discard its pending result");
	world.PreparePresentation(0.0f);
	GILGAMESH_CHECK_MESSAGE(!extraction.TakeFrame(),
		"a disabled extractor must not publish a replacement frame");

	// A standalone lifecycle must clear its snapshot through the normal shutdown API.
	RenderExtractionSystem standalone;
	standalone.Initialize();
	standalone.SetEnabled(true);
	standalone.Update(world, 0.0f);
	standalone.Shutdown(world);
	GILGAMESH_CHECK_MESSAGE(!standalone.TakeFrame(),
		"destruction must release the pending snapshot");
}

GILGAMESH_TEST("Engine.ECS.RenderExtractionSystem", "CopiesOpaqueHandlesWithoutResourceLookups")
{
	MeshManager meshes;
	const MeshHandle mesh = CreateTestMesh(meshes);
	GILGAMESH_CHECK_MESSAGE(mesh.IsValid(), "a WARP mesh handle must be available");
	if (!mesh.IsValid()) return;
	GILGAMESH_CHECK_MESSAGE(meshes.RemoveMesh(mesh),
		"the resource must be removed while retaining the issued handle value");

	World world;
	RenderExtractionSystem& extraction = AddExtractor(world);
	const SceneHandle scene = world.CreateScene();
	AddMeshEntity(world, scene, mesh);
	world.RequestSetActiveScene(scene);
	world.PreparePresentation(0.0f);
	auto frame = extraction.TakeFrame();
	GILGAMESH_CHECK_MESSAGE(frame && frame->staticMeshes.size() == 1 &&
		frame->staticMeshes[0].mesh == mesh && !meshes.IsValid(mesh),
		"extraction must copy assigned opaque handles and defer generation checks to rendering");
	GILGAMESH_CHECK_MESSAGE(frame && frame->staticMeshes.size() == 1 &&
		frame->staticMeshes[0].localToWorld == Matrix4::Identity(),
		"a default component transform must extract as identity");
}

GILGAMESH_TEST("Engine.ECS.RenderExtractionSystem", "UsesFinalTransformsWhileSimulationIsPaused")
{
	MeshManager meshes;
	const MeshHandle mesh = CreateTestMesh(meshes);
	GILGAMESH_CHECK_MESSAGE(mesh.IsValid(), "a WARP mesh handle must be available");
	if (!mesh.IsValid()) return;

	World world;
	const SceneHandle scene = world.CreateScene();
	const Entity entity = AddMeshEntity(world, scene, mesh);
	world.RequestSetActiveScene(scene);
	world.Update(0.0f);

	// Distinct phase values prove that pause skips simulation while final transforms still run.
	auto simulation = std::make_unique<SetPositionSystem>(entity, 100.0f);
	simulation->SetEnabled(true);
	world.GetSimulationSystems().AddSystem(std::move(simulation));
	auto transform = std::make_unique<SetPositionSystem>(entity, 9.0f);
	transform->SetEnabled(true);
	world.GetTransformSystems().AddSystem(std::move(transform));

	// Authored source writers must run before local/world matrix evaluation and extraction.
	RenderExtractionSystem& extraction = AddExtractor(world);
	world.SetSimulationPaused(true);
	world.Update(0.1f);
	GILGAMESH_CHECK_MESSAGE(world.Get<TransformComponent>(entity).position_.x == 0.0f,
		"paused simulation must not execute its systems");
	world.PreparePresentation(0.1f);
	auto frame = extraction.TakeFrame();
	GILGAMESH_CHECK_MESSAGE(frame && frame->staticMeshes.size() == 1 &&
		frame->staticMeshes[0].localToWorld.Translation().x == 9.0f,
		"presentation must evaluate transforms before extraction even while paused");
}

GILGAMESH_TEST("Engine.ECS.RenderExtractionSystem", "PausedHierarchyExtractionMatchesWorldBounds")
{
	MeshManager meshes;
	const MeshHandle mesh = CreateTestMesh(meshes);
	GILGAMESH_CHECK_MESSAGE(mesh.IsValid(), "a WARP mesh handle with known local bounds must be available");
	if (!mesh.IsValid()) return;

	World world;
	RenderExtractionSystem& extraction = AddExtractor(world);
	world.GetTransformSystems().AddSystem(std::make_unique<UpdateBoundsSystem>(meshes)).SetEnabled(true);
	const SceneHandle scene = world.CreateScene("Visible");
	const Entity parent = world.CreateEntity();
	TransformComponent parentTransform;
	parentTransform.position_ = { 10.0f, 20.0f, 30.0f };
	parentTransform.scale = { 2.0f, 3.0f, 4.0f };
	parentTransform.rotation_ = FromAxisAngle({ 0.0f, 0.0f, 1.0f }, 1.5707963267948966f);
	world.Emplace<TransformComponent>(parent, parentTransform);
	TransformComponent childTransform;
	childTransform.parent = parent;
	childTransform.position_ = { 1.0f, 2.0f, 3.0f };
	childTransform.scale = { 1.0f, 2.0f, 1.0f };
	const Entity child = AddMeshEntity(world, scene, mesh, childTransform);
	world.Emplace<AABBComponent>(child);
	GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(scene),
		"the mesh scene must be selected without requiring its parent to belong to that scene");

	// Paused presentation evaluates world-wide parents, then bounds, then the active-scene snapshot.
	world.SetSimulationPaused(true);
	world.PreparePresentation(0.1f);
	auto firstFrame = extraction.TakeFrame();
	const TransformComponent evaluated = world.Get<TransformComponent>(child);
	const AABBComponent firstBounds = world.Get<AABBComponent>(child);
	GILGAMESH_CHECK_MESSAGE(firstFrame && firstFrame->staticMeshes.size() == 1 &&
		firstFrame->staticMeshes[0].mesh == mesh &&
		firstFrame->staticMeshes[0].localToWorld == evaluated.localToWorld &&
		NearlyEquals(TransformPoint({}, evaluated.localToWorld), Vector3{ 4.0f, 22.0f, 42.0f }, 1e-4f) &&
		firstBounds.valid && firstBounds.sourceWorldRevision == evaluated.worldRevision &&
		NearlyEquals(firstBounds.AABB.min, Vector3{ -8.0f, 20.0f, 30.0f }, 1e-4f) &&
		NearlyEquals(firstBounds.AABB.max, Vector3{ 16.0f, 24.0f, 54.0f }, 1e-4f),
		"the extracted mesh and its bounds must share the evaluated hierarchy transform while simulation is paused");

	// A later paused editor edit must update both outputs while the previous owned frame stays unchanged.
	world.Get<TransformComponent>(parent).position_.x = 15.0f;
	world.PreparePresentation(0.1f);
	auto secondFrame = extraction.TakeFrame();
	const AABBComponent& secondBounds = world.Get<AABBComponent>(child);
	GILGAMESH_CHECK_MESSAGE(secondFrame && secondFrame->staticMeshes.size() == 1 &&
		NearlyEquals(TransformPoint({}, secondFrame->staticMeshes[0].localToWorld),
			Vector3{ 9.0f, 22.0f, 42.0f }, 1e-4f) &&
		secondBounds.valid && secondBounds.revision > firstBounds.revision &&
		secondBounds.sourceWorldRevision == world.Get<TransformComponent>(child).worldRevision &&
		NearlyEquals(secondBounds.AABB.min, Vector3{ -3.0f, 20.0f, 30.0f }, 1e-4f) &&
		NearlyEquals(secondBounds.AABB.max, Vector3{ 21.0f, 24.0f, 54.0f }, 1e-4f) &&
		firstFrame && firstFrame->staticMeshes.size() == 1 &&
		firstFrame->staticMeshes[0].localToWorld == evaluated.localToWorld,
		"paused parent edits must refresh world bounds and extraction without mutating the prior snapshot");
}

GILGAMESH_TEST("Engine.ECS.RenderExtractionSystem", "BoundsDiagnosticsAreOptInOwnedAndRebuiltEachFrame")
{
	MeshManager meshes;
	const MeshHandle mesh = CreateTestMesh(meshes);
	GILGAMESH_CHECK_MESSAGE(mesh.IsValid(), "a WARP mesh with bounds must be available");
	if (!mesh.IsValid()) return;
	std::optional<RenderFrameData> retained;
	BoundingBox retainedBounds;
	{
		World world;
		RenderExtractionSystem& extraction = AddExtractor(world);
		world.GetTransformSystems().AddSystem(std::make_unique<UpdateBoundsSystem>(meshes)).SetEnabled(true);
		const SceneHandle scene = world.CreateScene("Bounds");
		TransformComponent transform;
		transform.position_ = { 10.0f, 20.0f, 30.0f };
		transform.scale = { 2.0f, 3.0f, 4.0f };
		const Entity entity = AddMeshEntity(world, scene, mesh, transform);
		world.Emplace<AABBComponent>(entity);
		GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(scene), "the bounded mesh scene must become active");

		// Bounds are evaluated normally while default extraction leaves diagnostics disabled.
		world.PreparePresentation(0.0f);
		auto disabled = extraction.TakeFrame();
		const AABBComponent initialBounds = world.Get<AABBComponent>(entity);
		retainedBounds = initialBounds.AABB;
		GILGAMESH_CHECK_MESSAGE(initialBounds.valid && disabled && disabled->staticMeshes.size() == 1 &&
			disabled->debugLines.empty(),
			"valid bounds must not emit diagnostic geometry until extraction is explicitly enabled");

		// A setting change affects the next extraction, never an already completed pending frame.
		extraction.SetShowBounds(true);
		world.PreparePresentation(0.0f);
		extraction.SetShowBounds(false);
		retained = extraction.TakeFrame();
		GILGAMESH_CHECK_MESSAGE(retained && retained->staticMeshes.size() == 1 &&
			HasAABBWireframe(retained->debugLines, retainedBounds),
			"enabling bounds must copy twelve unique green world-space edges without applying the model again");
		world.PreparePresentation(0.0f);
		auto hidden = extraction.TakeFrame();
		GILGAMESH_CHECK_MESSAGE(hidden && hidden->staticMeshes.size() == 1 && hidden->debugLines.empty() &&
			retained && HasAABBWireframe(retained->debugLines, retainedBounds),
			"disabling bounds must clear the next frame while previously owned diagnostics remain intact");

		// Unchanged cached bounds still contribute complete diagnostics on every requested frame.
		extraction.SetShowBounds(true);
		world.PreparePresentation(0.0f);
		auto repeated = extraction.TakeFrame();
		GILGAMESH_CHECK_MESSAGE(repeated && HasAABBWireframe(repeated->debugLines, retainedBounds) &&
			world.Get<AABBComponent>(entity).revision == initialBounds.revision,
			"a stable bounds revision must still emit all twelve edges in each newly extracted frame");
	}
	GILGAMESH_CHECK_MESSAGE(retained && HasAABBWireframe(retained->debugLines, retainedBounds),
		"diagnostic endpoints and colors must remain owned by the snapshot after world destruction");
}

GILGAMESH_TEST("Engine.ECS.RenderExtractionSystem", "BoundsDiagnosticsRespectEligibilityAndActiveScene")
{
	MeshManager meshes;
	const MeshHandle mesh = CreateTestMesh(meshes);
	GILGAMESH_CHECK_MESSAGE(mesh.IsValid(), "a WARP mesh handle must be available");
	if (!mesh.IsValid()) return;
	World world;
	RenderExtractionSystem& extraction = AddExtractor(world);
	extraction.SetShowBounds(true);
	const SceneHandle firstScene = world.CreateScene("First");
	const SceneHandle secondScene = world.CreateScene("Second");
	AABBComponent firstBounds;
	firstBounds.AABB = { { 2.0f, 3.0f, 4.0f }, { 5.0f, 7.0f, 9.0f } };
	firstBounds.valid = true;
	AABBComponent secondBounds = firstBounds;
	secondBounds.AABB = { { 20.0f, 30.0f, 40.0f }, { 25.0f, 37.0f, 49.0f } };

	// Supply cache variants directly to isolate diagnostic extraction from bounds evaluation.
	const Entity complete = AddMeshEntity(world, firstScene, mesh);
	world.Emplace<AABBComponent>(complete, firstBounds);
	AddMeshEntity(world, firstScene, mesh);
	const Entity invalid = AddMeshEntity(world, firstScene, mesh);
	AABBComponent invalidBounds = firstBounds;
	invalidBounds.valid = false;
	world.Emplace<AABBComponent>(invalid, invalidBounds);
	const Entity unassigned = AddMeshEntity(world, firstScene, {});
	world.Emplace<AABBComponent>(unassigned, firstBounds);
	const Entity withoutTransform = world.CreateEntity(firstScene);
	world.Emplace<MeshComponent>(withoutTransform, MeshComponent{ mesh });
	world.Emplace<AABBComponent>(withoutTransform, firstBounds);
	const Entity withoutMesh = world.CreateEntity(firstScene);
	world.Emplace<TransformComponent>(withoutMesh);
	world.Emplace<AABBComponent>(withoutMesh, firstBounds);
	const Entity unscened = world.CreateEntity();
	world.Emplace<TransformComponent>(unscened);
	world.Emplace<MeshComponent>(unscened, MeshComponent{ mesh });
	world.Emplace<AABBComponent>(unscened, firstBounds);
	const Entity otherScene = AddMeshEntity(world, secondScene, mesh);
	world.Emplace<AABBComponent>(otherScene, secondBounds);

	// Diagnostic enablement does not select a scene or bypass ordinary mesh eligibility.
	world.PreparePresentation(0.0f);
	const auto unselected = extraction.TakeFrame();
	GILGAMESH_CHECK_MESSAGE(unselected && unselected->staticMeshes.empty() && unselected->debugLines.empty(),
		"a world without an active scene must publish no mesh or bounds diagnostics");
	GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(firstScene), "the first scene must become active");
	world.PreparePresentation(0.0f);
	const auto first = extraction.TakeFrame();
	GILGAMESH_CHECK_MESSAGE(first && first->staticMeshes.size() == 3 &&
		HasAABBWireframe(first->debugLines, firstBounds.AABB),
		"missing/invalid bounds and incomplete/unassigned/world-only entities must not add debug segments");

	// Switching, invalidating, and clearing the source must remove the previous frame's lines.
	GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(secondScene), "the second scene must become active");
	world.PreparePresentation(0.0f);
	const auto second = extraction.TakeFrame();
	GILGAMESH_CHECK_MESSAGE(second && second->staticMeshes.size() == 1 &&
		HasAABBWireframe(second->debugLines, secondBounds.AABB),
		"bounds diagnostics must switch to the selected scene instead of accumulating inactive-scene lines");
	world.Get<AABBComponent>(otherScene).valid = false;
	world.PreparePresentation(0.0f);
	const auto invalidated = extraction.TakeFrame();
	GILGAMESH_CHECK_MESSAGE(invalidated && invalidated->staticMeshes.size() == 1 &&
		invalidated->debugLines.empty(),
		"a newly invalid bounds cache must remove its diagnostics without suppressing the mesh");
	world.RequestClearActiveScene();
	world.PreparePresentation(0.0f);
	const auto cleared = extraction.TakeFrame();
	GILGAMESH_CHECK_MESSAGE(cleared && cleared->staticMeshes.empty() && cleared->debugLines.empty(),
		"clearing the active scene must publish a fresh empty diagnostics list");
}
