#include "PresentationSurface.h"


HRESULT D3D11PresentationSurface::CreateBackBuffer(ID3D11Device* device, Extent2D extent)
{
	if (!device) return E_POINTER;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;

	HRESULT result = swapChain_->GetBuffer(
		0,
		IID_PPV_ARGS(backBuffer.ReleaseAndGetAddressOf()));

	if (FAILED(result))
		return result;

	result = device->CreateRenderTargetView(
		backBuffer.Get(),
		nullptr,
		backBufferView_.ReleaseAndGetAddressOf());

	if (FAILED(result))
		return result;

	viewport_ = {};
	viewport_.TopLeftX = 0.0f;
	viewport_.TopLeftY = 0.0f;
	viewport_.Width = static_cast<float>(extent.width);
	viewport_.Height = static_cast<float>(extent.height);
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;

	extent_ = extent;
	return S_OK;
}

HRESULT D3D11PresentationSurface::Initialize(
	ID3D11Device* device,
	HWND outputWindow,
	Extent2D extent,
	bool vsync)
{
	if (device == nullptr || outputWindow == nullptr)
		return E_INVALIDARG;

	if (extent.width == 0 || extent.height == 0)
		return E_INVALIDARG;

	if (IsInitialized())
		return S_FALSE;

	Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
	HRESULT result = device->QueryInterface(
		IID_PPV_ARGS(dxgiDevice.ReleaseAndGetAddressOf()));

	if (FAILED(result))
		return result;

	Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
	result = dxgiDevice->GetAdapter(
		adapter.ReleaseAndGetAddressOf());

	if (FAILED(result))
		return result;

	Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
	result = adapter->GetParent(
		IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()));

	if (FAILED(result))
		return result;

	DXGI_SWAP_CHAIN_DESC1 description{};
	description.Width = extent.width;
	description.Height = extent.height;
	description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	description.SampleDesc.Count = 1;
	description.SampleDesc.Quality = 0;
	description.BufferUsage =
		DXGI_USAGE_RENDER_TARGET_OUTPUT;
	description.BufferCount = 2;
	description.Scaling = DXGI_SCALING_STRETCH;
	description.SwapEffect =
		DXGI_SWAP_EFFECT_FLIP_DISCARD;
	description.AlphaMode =
		DXGI_ALPHA_MODE_UNSPECIFIED;
	description.Flags = 0;

	result = factory->CreateSwapChainForHwnd(
		device,
		outputWindow,
		&description,
		nullptr,
		nullptr,
		swapChain_.ReleaseAndGetAddressOf());

	if (FAILED(result))
		return result;

	// The application owns display-mode behavior. Prevent DXGI from handling Alt+Enter automatically.
	factory->MakeWindowAssociation(
		outputWindow,
		DXGI_MWA_NO_ALT_ENTER);

	result = CreateBackBuffer(device, extent);

	if (FAILED(result))
	{
		swapChain_.Reset();
		return result;
	}

	vsync_ = vsync;
	return S_OK;
}

HRESULT D3D11PresentationSurface::Resize(
	ID3D11Device* device,
	ID3D11DeviceContext* context,
	Extent2D extent)
{
	// Device sanity check
	if (device == nullptr || context == nullptr) return E_INVALIDARG;

	// Surface sanity check
	if (!IsInitialized()) return DXGI_ERROR_INVALID_CALL;

	// Window sanity check
	if (extent.width == 0 || extent.height == 0) return S_FALSE;
	if (extent.width == extent_.width &&
		extent.height == extent_.height)
	{
		return S_FALSE;
	}

	// ResizeBuffers fails while the old back buffer remains bound or referenced.
	context->OMSetRenderTargets(0, nullptr, nullptr);
	backBufferView_.Reset();

	HRESULT result = swapChain_->ResizeBuffers(
		0,
		extent.width,
		extent.height,
		DXGI_FORMAT_UNKNOWN,
		0);

	if (FAILED(result)) return result;

	return CreateBackBuffer(device, extent);
}

void D3D11PresentationSurface::BeginFrame(
	ID3D11DeviceContext* context,
	const Color4& clearColor) const
{
	ID3D11RenderTargetView* renderTargets[] = {
		backBufferView_.Get()
	};

	context->OMSetRenderTargets(
		1,
		renderTargets,
		nullptr);

	context->RSSetViewports(1, &viewport_);

	context->ClearRenderTargetView(
		backBufferView_.Get(),
		clearColor.Data());
}

HRESULT D3D11PresentationSurface::Present()
{
	if (!IsInitialized()) return DXGI_ERROR_INVALID_CALL;

	const UINT synchronizationInterval =
		vsync_ ? 1u : 0u;

	return swapChain_->Present(
		synchronizationInterval,
		0);
}

HRESULT D3D11PresentationSurface::PresentTest() const
{
	if (!IsInitialized()) return DXGI_ERROR_INVALID_CALL;

	return swapChain_->Present(0, DXGI_PRESENT_TEST);
}

bool D3D11PresentationSurface::IsInitialized() const noexcept
{
	return swapChain_ != nullptr && backBufferView_ != nullptr;
}