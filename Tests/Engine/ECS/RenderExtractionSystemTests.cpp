#include <cmath>
#include <memory>
#include <optional>
#include <utility>

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
