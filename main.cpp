#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Platform/Windows/Window.h"

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

int Launch()
{
	Window window;

	if (!window.Create()) return 1;

	const Extent2D extent = window.ClientExtent();
	HWND hwnd = window.NativeHandle();

	// Device and Swap Chain
	ComPtr<ID3D11Device>		   device;
	ComPtr<ID3D11DeviceContext>	   context;
	ComPtr<IDXGISwapChain>		   swapChain;
	ComPtr<ID3D11RenderTargetView> rtv;

	DXGI_SWAP_CHAIN_DESC scd = {};
	scd.BufferCount = 2;
	scd.BufferDesc.Width = 1280;
	scd.BufferDesc.Height = 720;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.OutputWindow = hwnd;
	scd.SampleDesc.Count = 1;
	scd.Windowed = TRUE;
	scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	
	UINT flags = 0;
#ifdef _DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
		nullptr, 0, D3D11_SDK_VERSION,
		&scd, &swapChain, &device, nullptr, &context);
	if (Failed(hr, L"D3D11CreateDeviceAndSwapChain")) return 1;

	ComPtr<ID3D11Texture2D> backBuffer;
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
	if (Failed(hr, L"GetBuffer")) return 1;

	hr = device->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv);
	if (Failed(hr, L"CreateRenderTargetView")) return 1;

	D3D11_VIEWPORT vp = {};
	vp.Width	= 1280.f;
	vp.Height	= 720.f;
	vp.MaxDepth = 1.f;
	context->RSSetViewports(1, &vp);

	const std::filesystem::path shaderDir = ExecutableDir() / L"Shaders";
	const std::vector<char> vsBytes = LoadFile(shaderDir / L"Primitive.vs.cso");
	const std::vector<char> psBytes = LoadFile(shaderDir / L"Primitive.ps.cso");

	if (vsBytes.empty() || psBytes.empty())
	{
		MessageBox(nullptr, L"Failed to load compiled shaders.", L"Gilgamesh", MB_ICONERROR);
		return 1;
	}

	ComPtr<ID3D11VertexShader> vs;
	ComPtr<ID3D11PixelShader>  ps;
	hr = device->CreateVertexShader(vsBytes.data(), vsBytes.size(), nullptr, &vs);
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
		const float clearColor[4] = { 0.1f, 0.12f, 0.16f, 1.0f };
		context->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
		context->ClearRenderTargetView(rtv.Get(), clearColor);

		UINT stride = sizeof(SimpleVertex2D), offset = 0;
		context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		context->IASetInputLayout(inputLayout.Get());
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->VSSetShader(vs.Get(), nullptr, 0);
		context->PSSetShader(ps.Get(), nullptr, 0);
		context->Draw(3, 0);

		swapChain->Present(1, 0);
	}



	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	return Launch();
}