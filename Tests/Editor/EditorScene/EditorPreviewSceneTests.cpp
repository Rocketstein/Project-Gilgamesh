#include "Editor/EditorScene/EditorPreviewScene.h"
#include "Engine/Render/Mesh/MeshManager.h"
#include "Engine/Render/Mesh/PrimitiveMeshes.h"
#include "Engine/Render/Mesh/StaticMesh.h"
#include "Tests/TestFramework.h"

#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

namespace
{
	struct TestDevice
	{
		Microsoft::WRL::ComPtr<ID3D11Device> device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	};

	const TestDevice& GetTestDevice()
	{
		static const TestDevice device = []
		{
			TestDevice result;
			const HRESULT createResult = D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_WARP,
				nullptr,
				0,
				nullptr,
				0,
				D3D11_SDK_VERSION,
				result.device.GetAddressOf(),
				nullptr,
				result.context.GetAddressOf());

			if (FAILED(createResult))
			{
				result.device.Reset();
				result.context.Reset();
			}
			return result;
		}();

		return device;
	}
}

GILGAMESH_TEST("Editor.EditorPreviewScene", "RequiresInitializedManagerAndCanRetry")
{
	EditorPreviewScene scene;
	MeshManager meshes;
	std::vector<StaticMeshDrawItem> drawItems;

	GILGAMESH_CHECK_MESSAGE(!scene.Initialize(meshes),
		"the preview scene must reject an uninitialized mesh manager");
	scene.GatherDrawItems(meshes, drawItems);
	GILGAMESH_CHECK_MESSAGE(drawItems.empty() && meshes.Num() == 0,
		"failed initialization must not publish a mesh or draw item");

	const TestDevice& testDevice = GetTestDevice();
	GILGAMESH_CHECK_MESSAGE(testDevice.device != nullptr,
		"the WARP test device must be available");
	if (!testDevice.device) return;

	GILGAMESH_CHECK_MESSAGE(meshes.Initialize(testDevice.device.Get()),
		"the mesh manager must initialize for a retry");
	GILGAMESH_CHECK_MESSAGE(scene.Initialize(meshes),
		"preview-scene initialization must remain retryable");

	scene.Shutdown(meshes);
}

GILGAMESH_TEST("Editor.EditorPreviewScene", "OwnsAndGathersCube")
{
	const TestDevice& testDevice = GetTestDevice();
	if (!testDevice.device) return;

	MeshManager meshes;
	GILGAMESH_CHECK_MESSAGE(meshes.Initialize(testDevice.device.Get()),
		"the mesh manager must initialize for preview-scene testing");

	EditorPreviewScene scene;
	GILGAMESH_CHECK_MESSAGE(scene.Initialize(meshes),
		"the preview scene must create its cube");
	GILGAMESH_CHECK_MESSAGE(scene.Initialize(meshes) && meshes.Num() == 1,
		"same-manager initialization must be idempotent");

	std::vector<StaticMeshDrawItem> drawItems{
		StaticMeshDrawItem{}
	};
	scene.GatherDrawItems(meshes, drawItems);

	GILGAMESH_CHECK_MESSAGE(
		drawItems.size() == 2 && drawItems.front().mesh == nullptr,
		"gathering must append without clearing existing draw items");
	if (drawItems.size() == 2)
	{
		GILGAMESH_CHECK_MESSAGE(
			drawItems.back().mesh != nullptr &&
			drawItems.back().mesh->GetIndexCount() == 36,
			"the preview scene must submit the indexed unit cube");
		GILGAMESH_CHECK_MESSAGE(
			NearlyEquals(
				drawItems.back().localToWorld,
				Matrix4::Identity()),
			"the preview cube must begin at the identity transform");
	}

	drawItems.clear();
	scene.Shutdown(meshes);
	GILGAMESH_CHECK_MESSAGE(meshes.Num() == 0,
		"shutdown must remove the preview cube");
	scene.Shutdown(meshes);
	scene.GatherDrawItems(meshes, drawItems);
	GILGAMESH_CHECK_MESSAGE(drawItems.empty() && meshes.Num() == 0,
		"repeated shutdown must be harmless and leave no draw items");
}

GILGAMESH_TEST("Editor.EditorPreviewScene", "RejectsDifferentMeshManager")
{
	const TestDevice& testDevice = GetTestDevice();
	if (!testDevice.device) return;

	MeshManager firstMeshes;
	MeshManager secondMeshes;
	GILGAMESH_CHECK_MESSAGE(
		firstMeshes.Initialize(testDevice.device.Get()) &&
		secondMeshes.Initialize(testDevice.device.Get()),
		"both mesh managers must initialize for identity testing");

	EditorPreviewScene scene;
	GILGAMESH_CHECK_MESSAGE(scene.Initialize(firstMeshes),
		"the preview scene must initialize against its owning manager");
	const MeshHandle unrelatedMesh = secondMeshes.CreateMesh(
		PrimitiveMeshes::MakeUnitCubeMeshData());
	GILGAMESH_CHECK_MESSAGE(secondMeshes.IsValid(unrelatedMesh),
		"the second manager must contain an alias-shaped live handle");

	std::vector<StaticMeshDrawItem> wrongManagerItems;
	scene.GatherDrawItems(secondMeshes, wrongManagerItems);
	GILGAMESH_CHECK_MESSAGE(wrongManagerItems.empty(),
		"a handle must never resolve through a different manager");
	GILGAMESH_CHECK_MESSAGE(!scene.Initialize(secondMeshes),
		"an active preview scene must reject a different manager");

	scene.Shutdown(secondMeshes);
	GILGAMESH_CHECK_MESSAGE(
		firstMeshes.Num() == 1 && secondMeshes.IsValid(unrelatedMesh),
		"wrong-manager shutdown must not detach or remove either resource");

	std::vector<StaticMeshDrawItem> owningManagerItems;
	scene.GatherDrawItems(firstMeshes, owningManagerItems);
	GILGAMESH_CHECK_MESSAGE(owningManagerItems.size() == 1,
		"the original manager association must remain intact");
	owningManagerItems.clear();

	scene.Shutdown(firstMeshes);
	GILGAMESH_CHECK_MESSAGE(
		firstMeshes.Num() == 0 && secondMeshes.IsValid(unrelatedMesh),
		"normal shutdown must remove only the preview scene's mesh");
	(void)secondMeshes.RemoveMesh(unrelatedMesh);
}

GILGAMESH_TEST("Editor.EditorPreviewScene", "RecoversFromStaleMesh")
{
	const TestDevice& testDevice = GetTestDevice();
	if (!testDevice.device) return;

	MeshManager meshes;
	GILGAMESH_CHECK_MESSAGE(meshes.Initialize(testDevice.device.Get()),
		"the mesh manager must initialize for stale-handle testing");

	EditorPreviewScene scene;
	GILGAMESH_CHECK_MESSAGE(scene.Initialize(meshes),
		"the preview scene must create its initial cube");

	meshes.Shutdown();
	GILGAMESH_CHECK_MESSAGE(meshes.Initialize(testDevice.device.Get()),
		"the manager must reinitialize after discarding the preview mesh");
	const MeshHandle replacement = meshes.CreateMesh(
		PrimitiveMeshes::MakeUnitCubeMeshData());
	GILGAMESH_CHECK_MESSAGE(meshes.IsValid(replacement),
		"the manager must create a replacement in its recycled storage");

	std::vector<StaticMeshDrawItem> drawItems;
	scene.GatherDrawItems(meshes, drawItems);
	GILGAMESH_CHECK_MESSAGE(drawItems.empty(),
		"a stale preview handle must not resolve to a replacement mesh");

	GILGAMESH_CHECK_MESSAGE(scene.Initialize(meshes),
		"the scene must rebuild a mesh after its old handle becomes stale");
	GILGAMESH_CHECK_MESSAGE(
		meshes.Num() == 2 && meshes.IsValid(replacement),
		"recovery must preserve unrelated replacement resources");
	scene.GatherDrawItems(meshes, drawItems);
	GILGAMESH_CHECK_MESSAGE(drawItems.size() == 1,
		"the rebuilt preview cube must be gathered normally");

	drawItems.clear();
	scene.Shutdown(meshes);
	GILGAMESH_CHECK_MESSAGE(
		meshes.Num() == 1 && meshes.IsValid(replacement),
		"scene shutdown must preserve the replacement mesh");
	(void)meshes.RemoveMesh(replacement);
}
