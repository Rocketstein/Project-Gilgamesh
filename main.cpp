#include <iostream>

#include <d3d11.h>
#include <wrl/client.h>
#include <windows.h>

using Microsoft::WRL::ComPtr;

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

int main()
{
	std::cout << "Hello Main." << std::endl;

	// Window Creation
	const wchar_t* kClassName = L"GilgameshWindowClass";
	HINSTANCE hInstance = GetModuleHandle(nullptr);

	WNDCLASSEX wc = {};
	wc.cbSize		 = sizeof(WNDCLASSEX);
	wc.style		 = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc	 = WndProc;
	wc.hInstance	 = hInstance;
	wc.hCursor		 = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = kClassName;
	RegisterClassEx(&wc);

	RECT rc = { 0, 0, 1280 ,720 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	HWND hwnd = CreateWindowEx(0, kClassName, L"Project Gilgamesh", WS_OVERLAPPEDWINDOW,
							   CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
							   nullptr, nullptr, hInstance, nullptr);

	ShowWindow(hwnd, SW_SHOW);

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

	if (FAILED(hr)) return 1;

	ComPtr<ID3D11Texture2D> backBuffer;
	swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
	device->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv);

	D3D11_VIEWPORT vp = {};
	vp.Width	= 1280.f;
	vp.Height	= 720.f;
	vp.MaxDepth = 1.f;
	context->RSSetViewports(1, &vp);
	bool running = true;

	// Render Loop
	while (running)
	{
		MSG msg;
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT) running = false;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		const float clearColor[4] = { 0.1f, 0.12f, 0.16f, 1.0f };
		context->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
		context->ClearRenderTargetView(rtv.Get(), clearColor);
		swapChain->Present(1, 0);
	}


	return 0;
}