#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Platform/Windows/Window.h"
#include "Engine/Renderer/Renderer.h"

using Microsoft::WRL::ComPtr;

struct SimpleVertex2D { 
	float x, y;
	float r, g, b; 
};

static std::filesystem::path ExecutableDir()
{
	wchar_t buf[MAX_PATH];
	GetModuleFileNameW(nullptr, buf, MAX_PATH);
	return std::filesystem::path(buf).parent_path();
}

static std::vector<char> LoadFile(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) return {};

	const std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<char> data(static_cast<size_t>(size));
	file.read(data.data(), size);
	return data;
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
	// Must create a window before initializing the renderer because of the stack teardown order of global objects.
	Window window;
	if (!window.Create()) return 1;

	Renderer renderer;
	RendererDesc rendererDesc{};
	rendererDesc.outputWindow = window.NativeHandle();
	rendererDesc.extent = window.ClientExtent();
	rendererDesc.vsync = true;

	if (!renderer.Initialize(rendererDesc))
	{
		MessageBoxW(
			nullptr,
			L"Failed to initialize the renderer.",
			L"Gilgamesh",
			MB_ICONERROR);

		return 1;
	}


	// Incremental Refactor TODO: Move this to a Shader Manager class
	const std::filesystem::path shaderDir = ExecutableDir() / L"Shaders";
	const std::vector<char> vsBytes = LoadFile(shaderDir / L"Primitive.vs.cso");
	const std::vector<char> psBytes = LoadFile(shaderDir / L"Primitive.ps.cso");

	if (vsBytes.empty() || psBytes.empty())
	{
		MessageBox(nullptr, L"Failed to load compiled shaders.", L"Gilgamesh", MB_ICONERROR);
		return 1;
	}

	ID3D11Device* device = renderer.GetDevice();
	ComPtr<ID3D11VertexShader> vs;
	ComPtr<ID3D11PixelShader>  ps;
	HRESULT hr = device->CreateVertexShader(vsBytes.data(), vsBytes.size(), nullptr, &vs);
	if (Failed(hr, L"CreateVertexShader")) return 1;

	hr = device->CreatePixelShader(psBytes.data(), psBytes.size(), nullptr, &ps);
	if (Failed(hr, L"CreatePixelShader")) return 1;

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
	hr = device->CreateBuffer(&bd, &initData, &vertexBuffer);
	if (Failed(hr, L"CreateBuffer")) return 1;

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	ComPtr<ID3D11InputLayout> inputLayout;
	hr = device->CreateInputLayout(layout, 2, vsBytes.data(), vsBytes.size(), &inputLayout);
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
			if (!renderer.Resize(window.ClientExtent()))
				return 1;
		}

		const RenderResult result =
			renderer.Render(Color4{ 0.1f, 0.12f, 0.16f });

		if (result != RenderResult::Ok)
			return 1;
	}

	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	return Launch();
}