#include "Engine/Render/Mesh/MeshManager.h"
#include "Tests/TestFramework.h"

#include <cstdint>
#include <limits>

#include <d3d11.h>
#include <wrl/client.h>

namespace
{
// Shares one WARP device across the mesh resource tests.
struct TestDevice
{
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
};

// Create the reusable device once without requiring a physical GPU.
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

// Build minimal indexed geometry with valid zero-extent bounds by default.
StaticMeshData MakeTriangleData()
{
	StaticMeshData data;
	data.vertices_.resize(3);
	data.indices_ = { 0, 1, 2 };
	return data;
}

// Build a second draw shape for independent handle-lifetime checks.
StaticMeshData MakeQuadData()
{
	StaticMeshData data;
	data.vertices_.resize(4);
	data.indices_ = { 0, 1, 2, 0, 2, 3 };
	return data;
}

// Failed metadata lookups must preserve both caller-owned output values.
void CheckBoundsLookupFailure(
	Gilgamesh::Testing::TestContext& testContext,
	const MeshManager& manager,
	MeshHandle handle)
{
	BoundingBox bounds{ { -7.0f, -8.0f, -9.0f }, { 4.0f, 5.0f, 6.0f } };
	const BoundingBox originalBounds = bounds;
	std::uint64_t revision = 42;
	GILGAMESH_CHECK_MESSAGE(!manager.TryGetLocalBounds(handle, bounds, revision),
		"invalid mesh metadata lookup must fail");
	GILGAMESH_CHECK_MESSAGE(bounds.min == originalBounds.min &&
		bounds.max == originalBounds.max && revision == 42,
		"failed metadata lookup must leave both outputs unchanged");
}
} // Anonymous namespace

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
	CheckBoundsLookupFailure(testContext, manager, handle);
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

GILGAMESH_TEST("Engine.Render.MeshManager", "PublishesLocalBoundsFromAllVertices")
{
	const TestDevice& testDevice = GetTestDevice();
	if (!testDevice.device) return;

	MeshManager manager;
	GILGAMESH_CHECK_MESSAGE(manager.Initialize(testDevice.device.Get()),
		"the manager must initialize for bounds metadata testing");

	// The unindexed vertex contributes extrema, so bounds describe all uploaded positions.
	StaticMeshData data = MakeTriangleData();
	data.vertices_.resize(4);
	data.vertices_[0].position = { -2.0f, 3.0f, 4.0f };
	data.vertices_[1].position = { 5.0f, -6.0f, 7.0f };
	data.vertices_[2].position = { 1.0f, 2.0f, -8.0f };
	data.vertices_[3].position = { -9.0f, 10.0f, 11.0f };
	const MeshHandle handle = manager.CreateMesh(data);
	GILGAMESH_CHECK_MESSAGE(manager.IsValid(handle),
		"finite mesh positions must publish a live mesh");

	BoundingBox bounds{};
	std::uint64_t revision = 0;
	GILGAMESH_CHECK_MESSAGE(manager.TryGetLocalBounds(handle, bounds, revision),
		"a live mesh must expose CPU-local bounds");
	GILGAMESH_CHECK_MESSAGE(bounds.min == Vector3(-9.0f, -6.0f, -8.0f) &&
		bounds.max == Vector3(5.0f, 10.0f, 11.0f) && revision == 1,
		"bounds must enclose every uploaded position with the initial immutable revision");

	// Caller-owned copies cannot mutate the manager's immutable metadata.
	bounds = {};
	revision = 0;
	GILGAMESH_CHECK_MESSAGE(manager.TryGetLocalBounds(handle, bounds, revision) &&
		bounds.min == Vector3(-9.0f, -6.0f, -8.0f) &&
		bounds.max == Vector3(5.0f, 10.0f, 11.0f) && revision == 1,
		"repeated lookup must retain the original local bounds and revision");
	CheckBoundsLookupFailure(testContext, manager, {});
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

GILGAMESH_TEST("Engine.Render.MeshManager", "RejectsNonfinitePositionsAtomically")
{
	const TestDevice& testDevice = GetTestDevice();
	if (!testDevice.device) return;

	MeshManager manager;
	GILGAMESH_CHECK_MESSAGE(manager.Initialize(testDevice.device.Get()),
		"the manager must initialize for finite-position validation");
	const MeshHandle survivor = manager.CreateMesh(MakeTriangleData());

	// Test every coordinate, including an unindexed invalid vertex.
	StaticMeshData invalid = MakeTriangleData();
	invalid.vertices_.resize(4);
	invalid.vertices_[0].position.x = std::numeric_limits<float>::quiet_NaN();
	GILGAMESH_CHECK_MESSAGE(!manager.CreateMesh(invalid).IsValid(),
		"NaN coordinates must reject the entire mesh");
	invalid.vertices_[0].position = {};
	invalid.vertices_[1].position.y = std::numeric_limits<float>::infinity();
	GILGAMESH_CHECK_MESSAGE(!manager.CreateMesh(invalid).IsValid(),
		"positive infinity must reject the entire mesh");
	invalid.vertices_[1].position = {};
	invalid.vertices_[3].position.z = -std::numeric_limits<float>::infinity();
	GILGAMESH_CHECK_MESSAGE(!manager.CreateMesh(invalid).IsValid(),
		"nonfinite unindexed positions must also reject the entire mesh");

	BoundingBox bounds{};
	std::uint64_t revision = 0;
	GILGAMESH_CHECK_MESSAGE(manager.Num() == 1 &&
		manager.TryGetLocalBounds(survivor, bounds, revision) &&
		bounds.min == Vector3{} && bounds.max == Vector3{} && revision == 1,
		"rejected uploads must preserve existing meshes and their zero-extent bounds");
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
	CheckBoundsLookupFailure(testContext, manager, removed);
	GILGAMESH_CHECK_MESSAGE(
		manager.IsValid(survivor) && manager.TryGet(survivor)->GetIndexCount() == 6,
		"removing one resource must not affect another");

	// Recycled slots must publish only the replacement mesh's metadata.
	StaticMeshData replacementData = MakeTriangleData();
	for (NormalVertex& vertex : replacementData.vertices_)
	{
		vertex.position = { 2.0f, 3.0f, 4.0f };
	}
	const MeshHandle replacement = manager.CreateMesh(replacementData);
	GILGAMESH_CHECK_MESSAGE(replacement.IsValid() && replacement != removed,
		"a recycled slot must use a new generation");
	GILGAMESH_CHECK_MESSAGE(!manager.IsValid(removed),
		"slot reuse must not revive the stale handle");
	GILGAMESH_CHECK_MESSAGE(manager.Num() == 2,
		"only the survivor and replacement should remain live");
	BoundingBox bounds{};
	std::uint64_t revision = 0;
	GILGAMESH_CHECK_MESSAGE(manager.TryGetLocalBounds(replacement, bounds, revision) &&
		bounds.min == Vector3(2.0f, 3.0f, 4.0f) &&
		bounds.max == Vector3(2.0f, 3.0f, 4.0f) && revision == 1,
		"recycled slots must overwrite bounds and restart immutable metadata at revision one");
	CheckBoundsLookupFailure(testContext, manager, removed);
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
	CheckBoundsLookupFailure(testContext, manager, oldHandle);
	GILGAMESH_CHECK_MESSAGE(!manager.CreateMesh(MakeTriangleData()).IsValid(),
		"creation must remain disabled until reinitialization");

	GILGAMESH_CHECK_MESSAGE(manager.Initialize(testDevice.device.Get()),
		"the manager must support reinitialization");
	const MeshHandle replacement = manager.CreateMesh(MakeTriangleData());
	GILGAMESH_CHECK_MESSAGE(replacement.IsValid() && replacement != oldHandle,
		"reinitialization must not revive a pre-shutdown handle");
	GILGAMESH_CHECK_MESSAGE(!manager.IsValid(oldHandle),
		"the old generation must remain stale after reinitialization");
	BoundingBox bounds{};
	std::uint64_t revision = 0;
	GILGAMESH_CHECK_MESSAGE(manager.TryGetLocalBounds(replacement, bounds, revision) &&
		bounds.min == Vector3{} && bounds.max == Vector3{} && revision == 1,
		"reinitialization must publish fresh bounds metadata for the new generation");
	CheckBoundsLookupFailure(testContext, manager, oldHandle);
}
