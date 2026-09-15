#include "Engine/Render/D3D11/Shader/D3D11ShaderManager.h"
#include "Engine/Render/Mesh/MeshManager.h"
#include "Engine/Render/Mesh/PrimitiveMeshes.h"
#include "Engine/Render/Renderer/RenderView.h"
#include "Engine/Render/Renderer/StaticMeshDrawItem.h"
#include "Engine/Render/Techniques/SimpleMeshRenderer.h"
#include "Tests/TestFramework.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <string_view>

#include <Windows.h>
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

	std::filesystem::path GetShaderDirectory()
	{
		std::array<wchar_t, MAX_PATH> executablePath{};
		const DWORD length = GetModuleFileNameW(
			nullptr,
			executablePath.data(),
			static_cast<DWORD>(executablePath.size()));
		if (length == 0 || length == executablePath.size()) return {};

		return std::filesystem::path{
			std::wstring_view{ executablePath.data(), length }
		}.parent_path() / L"Shaders";
	}
}

GILGAMESH_TEST("Engine.Render.SimpleMeshRenderer", "RejectsInvalidInitialization")
{
	const TestDevice& testDevice = GetTestDevice();
	ShaderManager uninitializedShaders;
	SimpleMeshRenderer renderer;

	GILGAMESH_CHECK_MESSAGE(
		!renderer.Initialize(nullptr, uninitializedShaders),
		"initialization must reject a null device");

	if (testDevice.device)
	{
		GILGAMESH_CHECK_MESSAGE(
			!renderer.Initialize(testDevice.device.Get(), uninitializedShaders),
			"initialization must reject an uninitialized shader manager");
	}

	GILGAMESH_CHECK_MESSAGE(!renderer.IsInitialized(),
		"failed initialization must not publish partial renderer state");
}

GILGAMESH_TEST("Engine.Render.SimpleMeshRenderer", "BindsAndDrawsStaticMesh")
{
	const TestDevice& testDevice = GetTestDevice();
	GILGAMESH_CHECK_MESSAGE(
		testDevice.device != nullptr && testDevice.context != nullptr,
		"the WARP test device must be available");
	if (!testDevice.device || !testDevice.context) return;

	ShaderManager shaders;
	GILGAMESH_CHECK_MESSAGE(
		shaders.Initialize(testDevice.device.Get(), GetShaderDirectory()),
		"the shader manager must initialize against the test shader directory");
	if (!shaders.IsInitialized()) return;

	SimpleMeshRenderer renderer;
	GILGAMESH_CHECK_MESSAGE(
		renderer.Initialize(testDevice.device.Get(), shaders),
		"the mesh renderer must initialize with its fixed shaders");
	if (!renderer.IsInitialized()) return;

	MeshManager meshes;
	GILGAMESH_CHECK_MESSAGE(meshes.Initialize(testDevice.device.Get()),
		"the mesh manager must initialize for the draw test");

	const MeshHandle cubeHandle = meshes.CreateMesh(
		PrimitiveMeshes::MakeUnitCubeMeshData());
	const StaticMesh* cube = meshes.TryGet(cubeHandle);
	GILGAMESH_CHECK_MESSAGE(cube != nullptr,
		"the unit cube must upload before drawing");
	if (!cube) return;

	Matrix4 localToWorld = Matrix4::Identity();
	localToWorld(3, 0) = 2.0f;
	localToWorld(3, 1) = 3.0f;
	localToWorld(3, 2) = 4.0f;

	Matrix4 view = Matrix4::Identity();
	view(0, 0) = 2.0f;
	view(1, 1) = 3.0f;
	view(2, 2) = 4.0f;

	Matrix4 projection = Matrix4::Identity();
	projection(3, 0) = -1.0f;
	projection(3, 1) = 5.0f;

	const RenderView renderView{
		.view = view,
		.projection = projection
	};
	const std::array drawItems{
		StaticMeshDrawItem{
			.mesh = cube,
			.localToWorld = localToWorld
		}
	};

	renderer.Draw(testDevice.context.Get(), renderView, drawItems);

	D3D11_PRIMITIVE_TOPOLOGY topology =
		D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	testDevice.context->IAGetPrimitiveTopology(&topology);

	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
	testDevice.context->IAGetInputLayout(inputLayout.GetAddressOf());

	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
	testDevice.context->VSGetShader(
		vertexShader.GetAddressOf(),
		nullptr,
		nullptr);

	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
	testDevice.context->PSGetShader(
		pixelShader.GetAddressOf(),
		nullptr,
		nullptr);

	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState;
	UINT stencilReference = 0;
	testDevice.context->OMGetDepthStencilState(
		depthStencilState.GetAddressOf(),
		&stencilReference);

	GILGAMESH_CHECK_MESSAGE(
		topology == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		"the renderer must bind triangle-list topology");
	GILGAMESH_CHECK_MESSAGE(inputLayout != nullptr,
		"the renderer must bind its NormalVertex input layout");
	GILGAMESH_CHECK_MESSAGE(vertexShader != nullptr && pixelShader != nullptr,
		"the renderer must bind both fixed shaders");
	GILGAMESH_CHECK_MESSAGE(depthStencilState != nullptr,
		"the renderer must bind its depth-stencil state");

	if (depthStencilState)
	{
		D3D11_DEPTH_STENCIL_DESC depthDesc{};
		depthStencilState->GetDesc(&depthDesc);
		GILGAMESH_CHECK_MESSAGE(
			depthDesc.DepthEnable &&
			depthDesc.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ALL &&
			depthDesc.DepthFunc == D3D11_COMPARISON_LESS,
			"the renderer must enable depth testing and writes with LESS comparison");
	}

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

	Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
	testDevice.context->VSGetConstantBuffers(
		0,
		1,
		constantBuffer.GetAddressOf());

	GILGAMESH_CHECK_MESSAGE(
		vertexBuffer != nullptr &&
		vertexStride == sizeof(NormalVertex) &&
		vertexOffset == 0,
		"the draw must bind the mesh vertex buffer and stride");
	GILGAMESH_CHECK_MESSAGE(
		indexBuffer != nullptr &&
		indexFormat == DXGI_FORMAT_R32_UINT &&
		indexOffset == 0,
		"the draw must bind the mesh 32-bit index buffer");
	GILGAMESH_CHECK_MESSAGE(constantBuffer != nullptr,
		"the renderer must bind its object constants to vertex slot zero");

	if (constantBuffer)
	{
		D3D11_BUFFER_DESC sourceDesc{};
		constantBuffer->GetDesc(&sourceDesc);

		D3D11_BUFFER_DESC stagingDesc = sourceDesc;
		stagingDesc.Usage = D3D11_USAGE_STAGING;
		stagingDesc.BindFlags = 0;
		stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		stagingDesc.MiscFlags = 0;
		stagingDesc.StructureByteStride = 0;

		Microsoft::WRL::ComPtr<ID3D11Buffer> stagingBuffer;
		const HRESULT stagingResult = testDevice.device->CreateBuffer(
			&stagingDesc,
			nullptr,
			stagingBuffer.GetAddressOf());
		GILGAMESH_CHECK_MESSAGE(SUCCEEDED(stagingResult),
			"the test must create a staging copy of the object constants");

		if (SUCCEEDED(stagingResult))
		{
			testDevice.context->CopyResource(
				stagingBuffer.Get(),
				constantBuffer.Get());

			D3D11_MAPPED_SUBRESOURCE mapped{};
			const HRESULT mapResult = testDevice.context->Map(
				stagingBuffer.Get(),
				0,
				D3D11_MAP_READ,
				0,
				&mapped);
			GILGAMESH_CHECK_MESSAGE(SUCCEEDED(mapResult),
				"the uploaded object constants must be readable through staging");

			if (SUCCEEDED(mapResult))
			{
				Matrix4 uploadedConstants;
				std::memcpy(
					uploadedConstants.Data(),
					mapped.pData,
					sizeof(Matrix4));
				testDevice.context->Unmap(stagingBuffer.Get(), 0);

				const Matrix4 expected = Transpose(
					localToWorld * view * projection);
				GILGAMESH_CHECK_MESSAGE(
					NearlyEquals(uploadedConstants, expected),
					"the renderer must upload local-to-world, view, and projection in order");
			}
		}
	}

	testDevice.context->ClearState();
}
