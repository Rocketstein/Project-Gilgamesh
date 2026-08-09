#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Platform/Windows/Window.h"
#include "Engine/Render/Renderer/Renderer.h"
#include "Engine/Render/VertexTypes/VertexTypes.h"

using Microsoft::WRL::ComPtr;

static std::filesystem::path ExecutableDir()
{
	wchar_t buf[MAX_PATH];
	GetModuleFileNameW(nullptr, buf, MAX_PATH);
	return std::filesystem::path(buf).parent_path();
}

static bool Failed(HRESULT hr, const wchar_t* what)
{
	if (SUCCEEDED(hr)) return false;

	wchar_t msg[512];
	swprintf_s(msg, L"%s failed (0x%08X)", what, static_cast<unsigned>(hr));
	MessageBox(nullptr, msg, L"Gilgamesh", MB_ICONERROR);
	return true;
}

// Incremental Refactor TODO: Move this to Application class
int Launch()
{
	// Must create a window before initializing the renderer because of the stack teardown order of objects.
	Window window;
	if (!window.Create()) return 1;

	Renderer renderer;
	RendererDesc rendererDesc{};
	rendererDesc.outputWindow = window.GetNativeHandle();
	rendererDesc.extent = window.GetClientExtent();
	rendererDesc.vsync = true;
	rendererDesc.shaderDirectory = ExecutableDir() / L"Shaders";

	if (!renderer.Initialize(rendererDesc))
	{
		MessageBoxW(
			nullptr,
			L"Failed to initialize the renderer.",
			L"Gilgamesh",
			MB_ICONERROR);

		return 1;
	}

	// Shader Manager test. Move this to graphics pipeline later on.
	ShaderManager& shaders = renderer.GetShaderManager();

	const auto vsLoad = shaders.LoadVertex(L"Primitive");
	if (!vsLoad) return 1;

	const auto psLoad = shaders.LoadPixel(L"Primitive");
	if (!psLoad) return 1;

	const VertexShaderHandle vsHandle = vsLoad.resource;
	const PixelShaderHandle  psHandle = psLoad.resource;

	ID3D11VertexShader* vs = shaders.Get(vsHandle);
	ID3D11PixelShader*  ps = shaders.Get(psHandle);
	const std::span<const std::byte> vsBytecode = shaders.GetBytecode(vsHandle);

	SimpleVertex2D verts[] = {
	{  0.0f,  0.5f,  1, 0, 0 },
	{  0.5f, -0.5f,  0, 1, 0 },
	{ -0.5f, -0.5f,  0, 0, 1 },
	};

	D3D11_BUFFER_DESC bd = {};
	bd.ByteWidth = sizeof(verts);
	bd.Usage = D3D11_USAGE_IMMUTABLE;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA initData = { verts };

	ComPtr<ID3D11Buffer> vertexBuffer;
	HRESULT hr = renderer.GetDevice()->CreateBuffer(&bd, &initData, &vertexBuffer);
	if (Failed(hr, L"CreateBuffer")) return 1;

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	ComPtr<ID3D11InputLayout> inputLayout;
	hr = renderer.GetDevice()->CreateInputLayout(layout, 2, vsBytecode.data(), vsBytecode.size(), &inputLayout);
	if (Failed(hr, L"CreateInputLayout")) return 1;

	// Render Loop
	while (window.PumpMessages())
	{
		// Handle resizing events
		if (window.IsMinimized())
		{
			WaitMessage();
			continue;
		}
		if (window.ConsumePendingResize())
		{
			if (!renderer.Resize(window.GetClientExtent()))
				return 1;
		}

		const RenderResult result =
			renderer.Render(Color4{ 0.1f, 0.12f, 0.16f });

		switch (result)
		{
		case (RenderResult::Ok): {
			break;
		}
		case (RenderResult::Occluded): {
			while (window.PumpMessages() && renderer.IsOccluded())
				Sleep(16);
			break;
		}
		case (RenderResult::DeviceLost):
		case (RenderResult::Failed):
			return 1;
		}
	}

	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	return Launch();
}