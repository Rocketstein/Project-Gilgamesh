#include "Engine/Render/Mesh/MeshManager.h"
#include "Tests/TestFramework.h"

#include <cstdint>

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

StaticMeshData MakeTriangleData()
{
	StaticMeshData data;
	data.vertices_.resize(3);
	data.indices_ = { 0, 1, 2 };
	return data;
}

StaticMeshData MakeQuadData()
{
	StaticMeshData data;
	data.vertices_.resize(4);
	data.indices_ = { 0, 1, 2, 0, 2, 3 };
	return data;
}
}

GILGAMESH_TEST("Engine.Render.MeshManager", "RequiresInitialization")
{
	MeshManager manager;
	const MeshHandle handle = manager.CreateMesh(MakeTriangleData());

	GILGAMESH_CHECK_MESSAGE(!manager.IsInitialized(),
		"a default manager must not have a device");
	GILGAMESH_CHECK_MESSAGE(!handle.IsValid(),
		"creation without a device must return an invalid handle");
	GILGAMESH_CHECK_MESSAGE(manager.Num() == 0,
		"failed creation must not publish a mesh");
	GILGAMESH_CHECK_MESSAGE(!manager.Initialize(nullptr),
		"initialization must reject a null device");
}

GILGAMESH_TEST("Engine.Render.MeshManager", "UploadsAndBindsMesh")
{
	const TestDevice& testDevice = GetTestDevice();
	GILGAMESH_CHECK_MESSAGE(testDevice.device != nullptr && testDevice.context != nullptr,
		"the WARP test device must be available");
	if (!testDevice.device || !testDevice.context) return;

	MeshManager manager;
	GILGAMESH_CHECK_MESSAGE(manager.Initialize(testDevice.device.Get()),
		"the manager must accept a valid D3D11 device");

	const MeshHandle handle = manager.CreateMesh(MakeTriangleData());
	const StaticMesh* mesh = manager.TryGet(handle);
	GILGAMESH_CHECK_MESSAGE(handle.IsValid() && manager.IsValid(handle),
		"a valid upload must publish a live handle");
	GILGAMESH_CHECK_MESSAGE(manager.Num() == 1,
		"a successful upload must increment the live count");
	GILGAMESH_CHECK_MESSAGE(mesh != nullptr && mesh->GetIndexCount() == 3,
		"the resolved mesh must retain its draw count");
	if (!mesh) return;

	mesh->Bind(testDevice.context.Get());

	Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
	UINT vertexStride = 0;
	UINT vertexOffset = 0;
	testDevice.context->IAGetVertexBuffers(
		0,
		1,
		vertexBuffer.GetAddressOf(),
		&vertexStride,
		&vertexOffset);

	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
	DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
	UINT indexOffset = 0;
	testDevice.context->IAGetIndexBuffer(
		indexBuffer.GetAddressOf(),
		&indexFormat,
		&indexOffset);

	GILGAMESH_CHECK_MESSAGE(vertexBuffer != nullptr,
		"binding must expose the uploaded vertex buffer to the input assembler");
	GILGAMESH_CHECK_MESSAGE(vertexStride == sizeof(NormalVertex) && vertexOffset == 0,
		"binding must use the mesh vertex stride and a zero offset");
	GILGAMESH_CHECK_MESSAGE(indexBuffer != nullptr,
		"binding must expose the uploaded index buffer to the input assembler");
	GILGAMESH_CHECK_MESSAGE(indexFormat == DXGI_FORMAT_R32_UINT && indexOffset == 0,
		"binding must interpret the uploaded indices as 32-bit values");

	testDevice.context->ClearState();
}

GILGAMESH_TEST("Engine.Render.MeshManager", "RejectsInvalidDataAtomically")
{
	const TestDevice& testDevice = GetTestDevice();
	if (!testDevice.device) return;

	MeshManager manager;
	GILGAMESH_CHECK_MESSAGE(manager.Initialize(testDevice.device.Get()),
		"the manager must initialize for validation testing");

	const MeshHandle empty = manager.CreateMesh({});
	StaticMeshData outOfRange = MakeTriangleData();
	outOfRange.indices_.back() = 3;
	const MeshHandle invalidIndex = manager.CreateMesh(outOfRange);

	GILGAMESH_CHECK_MESSAGE(!empty.IsValid() && !invalidIndex.IsValid(),
		"empty or out-of-range geometry must be rejected");
	GILGAMESH_CHECK_MESSAGE(manager.Num() == 0,
		"rejected geometry must not publish a partial mesh");
}

GILGAMESH_TEST("Engine.Render.MeshManager", "RejectsRemovedAndReusedHandles")
{
	const TestDevice& testDevice = GetTestDevice();
	if (!testDevice.device) return;

	MeshManager manager;
	GILGAMESH_CHECK_MESSAGE(manager.Initialize(testDevice.device.Get()),
		"the manager must initialize for lifecycle testing");

	const MeshHandle removed = manager.CreateMesh(MakeTriangleData());
	const MeshHandle survivor = manager.CreateMesh(MakeQuadData());
	GILGAMESH_CHECK_MESSAGE(manager.RemoveMesh(removed),
		"removing a live mesh must succeed");
	GILGAMESH_CHECK_MESSAGE(!manager.IsValid(removed) && manager.TryGet(removed) == nullptr,
		"removal must invalidate the old handle");
	GILGAMESH_CHECK_MESSAGE(!manager.RemoveMesh(removed),
		"removing a stale handle must be a no-op");
	GILGAMESH_CHECK_MESSAGE(
		manager.IsValid(survivor) && manager.TryGet(survivor)->GetIndexCount() == 6,
		"removing one resource must not affect another");

	const MeshHandle replacement = manager.CreateMesh(MakeTriangleData());
	GILGAMESH_CHECK_MESSAGE(replacement.IsValid() && replacement != removed,
		"a recycled slot must use a new generation");
	GILGAMESH_CHECK_MESSAGE(!manager.IsValid(removed),
		"slot reuse must not revive the stale handle");
	GILGAMESH_CHECK_MESSAGE(manager.Num() == 2,
		"only the survivor and replacement should remain live");
}

GILGAMESH_TEST("Engine.Render.MeshManager", "ShutdownInvalidatesExistingHandles")
{
	const TestDevice& testDevice = GetTestDevice();
	if (!testDevice.device) return;

	MeshManager manager;
	GILGAMESH_CHECK_MESSAGE(manager.Initialize(testDevice.device.Get()),
		"the manager must initialize for shutdown testing");
	const MeshHandle oldHandle = manager.CreateMesh(MakeTriangleData());

	manager.Shutdown();
	GILGAMESH_CHECK_MESSAGE(!manager.IsInitialized() && manager.Num() == 0,
		"shutdown must release the device and all live meshes");
	GILGAMESH_CHECK_MESSAGE(!manager.IsValid(oldHandle),
		"shutdown must invalidate existing handles");
	GILGAMESH_CHECK_MESSAGE(!manager.CreateMesh(MakeTriangleData()).IsValid(),
		"creation must remain disabled until reinitialization");

	GILGAMESH_CHECK_MESSAGE(manager.Initialize(testDevice.device.Get()),
		"the manager must support reinitialization");
	const MeshHandle replacement = manager.CreateMesh(MakeTriangleData());
	GILGAMESH_CHECK_MESSAGE(replacement.IsValid() && replacement != oldHandle,
		"reinitialization must not revive a pre-shutdown handle");
	GILGAMESH_CHECK_MESSAGE(!manager.IsValid(oldHandle),
		"the old generation must remain stale after reinitialization");
}
