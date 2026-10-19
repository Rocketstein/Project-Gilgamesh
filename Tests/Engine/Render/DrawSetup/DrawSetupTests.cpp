#include "Engine/Render/Shader/ShaderManager.h"
#include "Engine/Render/DrawSetup/DrawSetup.h"
#include "Tests/TestFramework.h"

#include <array>
#include <filesystem>
#include <string_view>
#include <utility>

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

namespace
{
	// Shares one software device between draw setup checks.
	struct TestDevice
	{
		Microsoft::WRL::ComPtr<ID3D11Device> device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	};

	// Creates a software context without requiring a physical graphics adapter.
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

	// Shader assets are registered separately for the test executable.
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
} // Anonymous namespace

GILGAMESH_TEST("Engine.Render.DrawSetup", "AppliesConfiguredStateAndMovesSafely")
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
		"the draw setup test must load its vertex shader");
	if (!vertexShader) return;

	const DrawSetupDesc drawSetupDesc{
		.vertexShader = vertexShader.resource,
		.drawState = {
			.topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
			.cullMode = D3D11_CULL_NONE,
			.fillMode = D3D11_FILL_WIREFRAME,
			.blendEnabled = true
		}
	};

	DrawSetup source;
	const HRESULT initializeResult = source.Initialize(
		testDevice.device.Get(),
		shaders,
		drawSetupDesc);
	GILGAMESH_CHECK_MESSAGE(SUCCEEDED(initializeResult),
		"the draw setup must initialize with non-default state");
	if (FAILED(initializeResult)) return;

	DrawSetup moved{ std::move(source) };
	GILGAMESH_CHECK_MESSAGE(
		!source.IsInitialized() && moved.IsInitialized(),
		"move construction must transfer the initialized draw setup");

	DrawSetup assigned;
	assigned = std::move(moved);
	GILGAMESH_CHECK_MESSAGE(
		!moved.IsInitialized() && assigned.IsInitialized(),
		"move assignment must transfer the initialized draw setup");
	if (!assigned.IsInitialized()) return;

	assigned.Apply(testDevice.context.Get());

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
		"the draw setup must apply its configured topology");
	GILGAMESH_CHECK_MESSAGE(rasterizerState != nullptr,
		"the draw setup must apply its configured rasterizer state");
	GILGAMESH_CHECK_MESSAGE(blendState != nullptr,
		"the draw setup must apply its configured blend state");

	if (rasterizerState)
	{
		D3D11_RASTERIZER_DESC rasterizerDesc{};
		rasterizerState->GetDesc(&rasterizerDesc);
		GILGAMESH_CHECK_MESSAGE(
			rasterizerDesc.FillMode == D3D11_FILL_WIREFRAME &&
			rasterizerDesc.CullMode == D3D11_CULL_NONE &&
			rasterizerDesc.DepthClipEnable == TRUE,
			"the draw setup must preserve non-default rasterizer choices");
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
			"the draw setup must apply conventional straight-alpha blending");
	}

	testDevice.context->ClearState();
}


