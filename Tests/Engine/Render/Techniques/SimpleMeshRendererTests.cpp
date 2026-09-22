#include "Engine/Render/D3D11/Shader/D3D11ShaderManager.h"
#include "Engine/Render/Mesh/MeshManager.h"
#include "Engine/Render/Mesh/PrimitiveMeshes.h"
#include "Engine/Render/Pipeline/GraphicsPipeline.h"
#include "Engine/Render/Renderer/RenderView.h"
#include "Engine/Render/Renderer/StaticMeshDrawItem.h"
#include "Engine/Render/Techniques/SimpleMeshRenderer.h"
#include "Tests/TestFramework.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <string_view>
#include <utility>

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

GILGAMESH_TEST("Engine.Render.GraphicsPipeline", "BindsConfiguredStateAndMovesSafely")
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

	const auto vertexShader = shaders.LoadVertex(L"Primitive");
	GILGAMESH_CHECK_MESSAGE(vertexShader,
		"the graphics pipeline test must load its vertex shader");
	if (!vertexShader) return;

	const GraphicsPipelineDesc pipelineDesc{
		.vertexShader = vertexShader.resource,
		.drawState = {
			.topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
			.cullMode = D3D11_CULL_NONE,
			.fillMode = D3D11_FILL_WIREFRAME,
			.blendEnabled = true
		}
	};

	GraphicsPipeline source;
	const HRESULT initializeResult = source.Initialize(
		testDevice.device.Get(),
		shaders,
		pipelineDesc);
	GILGAMESH_CHECK_MESSAGE(SUCCEEDED(initializeResult),
		"the graphics pipeline must initialize with non-default state");
	if (FAILED(initializeResult)) return;

	GraphicsPipeline moved{ std::move(source) };
	GILGAMESH_CHECK_MESSAGE(
		!source.IsInitialized() && moved.IsInitialized(),
		"move construction must transfer initialized pipeline state");

	GraphicsPipeline assigned;
	assigned = std::move(moved);
	GILGAMESH_CHECK_MESSAGE(
		!moved.IsInitialized() && assigned.IsInitialized(),
		"move assignment must transfer initialized pipeline state");
	if (!assigned.IsInitialized()) return;

	assigned.Bind(testDevice.context.Get());

	D3D11_PRIMITIVE_TOPOLOGY topology =
		D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	testDevice.context->IAGetPrimitiveTopology(&topology);

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;
	testDevice.context->RSGetState(rasterizerState.GetAddressOf());

	Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
	FLOAT blendFactor[4]{};
	UINT sampleMask = 0;
	testDevice.context->OMGetBlendState(
		blendState.GetAddressOf(),
		blendFactor,
		&sampleMask);

	GILGAMESH_CHECK_MESSAGE(
		topology == D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
		"the pipeline must bind its configured topology");
	GILGAMESH_CHECK_MESSAGE(rasterizerState != nullptr,
		"the pipeline must bind its configured rasterizer state");
	GILGAMESH_CHECK_MESSAGE(blendState != nullptr,
		"the pipeline must bind its configured blend state");

	if (rasterizerState)
	{
		D3D11_RASTERIZER_DESC rasterizerDesc{};
		rasterizerState->GetDesc(&rasterizerDesc);
		GILGAMESH_CHECK_MESSAGE(
			rasterizerDesc.FillMode == D3D11_FILL_WIREFRAME &&
			rasterizerDesc.CullMode == D3D11_CULL_NONE &&
			rasterizerDesc.DepthClipEnable == TRUE,
			"the pipeline must preserve non-default rasterizer choices");
	}

	if (blendState)
	{
		D3D11_BLEND_DESC blendDesc{};
		blendState->GetDesc(&blendDesc);
		const D3D11_RENDER_TARGET_BLEND_DESC& target =
			blendDesc.RenderTarget[0];
		GILGAMESH_CHECK_MESSAGE(
			target.BlendEnable == TRUE &&
			target.SrcBlend == D3D11_BLEND_SRC_ALPHA &&
			target.DestBlend == D3D11_BLEND_INV_SRC_ALPHA &&
			target.BlendOp == D3D11_BLEND_OP_ADD &&
			target.RenderTargetWriteMask == D3D11_COLOR_WRITE_ENABLE_ALL &&
			sampleMask == 0xFFFFFFFFu,
			"the pipeline must bind conventional straight-alpha blending");
	}

	testDevice.context->ClearState();
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

	testDevice.context->RSSetState(nullptr);
	testDevice.context->OMSetBlendState(nullptr, nullptr, 0u);

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

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;
	testDevice.context->RSGetState(rasterizerState.GetAddressOf());

	Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
	FLOAT blendFactor[4]{};
	UINT sampleMask = 0;
	testDevice.context->OMGetBlendState(
		blendState.GetAddressOf(),
		blendFactor,
		&sampleMask);

	GILGAMESH_CHECK_MESSAGE(
		topology == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		"the renderer must bind triangle-list topology");
	GILGAMESH_CHECK_MESSAGE(inputLayout != nullptr,
		"the renderer must bind its NormalVertex input layout");
	GILGAMESH_CHECK_MESSAGE(vertexShader != nullptr && pixelShader != nullptr,
		"the renderer must bind both fixed shaders");
	GILGAMESH_CHECK_MESSAGE(depthStencilState != nullptr,
		"the renderer must bind its depth-stencil state");
	GILGAMESH_CHECK_MESSAGE(rasterizerState != nullptr,
		"the renderer must bind its rasterizer state");
	GILGAMESH_CHECK_MESSAGE(blendState != nullptr,
		"the renderer must bind its blend state");

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

	if (rasterizerState)
	{
		D3D11_RASTERIZER_DESC rasterizerDesc{};
		rasterizerState->GetDesc(&rasterizerDesc);
		GILGAMESH_CHECK_MESSAGE(
			rasterizerDesc.FillMode == D3D11_FILL_SOLID &&
			rasterizerDesc.CullMode == D3D11_CULL_BACK &&
			rasterizerDesc.DepthClipEnable == TRUE &&
			rasterizerDesc.FrontCounterClockwise == FALSE,
			"the renderer must restore solid back-face culling with depth clipping");
	}

	if (blendState)
	{
		D3D11_BLEND_DESC blendDesc{};
		blendState->GetDesc(&blendDesc);
		GILGAMESH_CHECK_MESSAGE(
			blendDesc.AlphaToCoverageEnable == FALSE &&
			blendDesc.IndependentBlendEnable == FALSE &&
			blendDesc.RenderTarget[0].BlendEnable == FALSE &&
			blendDesc.RenderTarget[0].RenderTargetWriteMask ==
				D3D11_COLOR_WRITE_ENABLE_ALL &&
			sampleMask == 0xFFFFFFFFu,
			"the renderer must restore opaque blending and a full sample mask");
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
