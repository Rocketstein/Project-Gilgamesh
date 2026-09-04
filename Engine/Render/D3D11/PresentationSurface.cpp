#include "PresentationSurface.h"

#include <utility>

// Rename back to CreateBackBuffer after migrating DSV to viewport
HRESULT D3D11PresentationSurface::CreateOutputAttachments(
	ID3D11Device* device,
	Extent2D extent)
{
	if (device == nullptr) return E_POINTER;
	if (extent.width == 0 || extent.height == 0) return E_INVALIDARG;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> newBackBufferView;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> newDepthTexture;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> newDepthStencilView;

	HRESULT result = swapChain_->GetBuffer(
		0,
		IID_PPV_ARGS(backBuffer.ReleaseAndGetAddressOf()));

	if (FAILED(result))
		return result;

	result = device->CreateRenderTargetView(
		backBuffer.Get(),
		nullptr,
		newBackBufferView.GetAddressOf());

	if (FAILED(result))
		return result;

	D3D11_TEXTURE2D_DESC depthDescription{};
	depthDescription.Width = extent.width;
	depthDescription.Height = extent.height;
	depthDescription.MipLevels = 1;
	depthDescription.ArraySize = 1;
	depthDescription.Format = DXGI_FORMAT_D32_FLOAT;
	depthDescription.SampleDesc.Count = 1;
	depthDescription.Usage = D3D11_USAGE_DEFAULT;
	depthDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	result = device->CreateTexture2D(
		&depthDescription,
		nullptr,
		newDepthTexture.GetAddressOf());

	if (FAILED(result))
		return result;

	result = device->CreateDepthStencilView(
		newDepthTexture.Get(),
		nullptr,
		newDepthStencilView.GetAddressOf());

	if (FAILED(result))
		return result;

	// Commit only after every output attachment has been created.
	backBufferView_ = std::move(newBackBufferView);
	depthTexture_ = std::move(newDepthTexture);
	depthStencilView_ = std::move(newDepthStencilView);

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

	result = CreateOutputAttachments(device, extent);

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
	depthStencilView_.Reset();
	depthTexture_.Reset();

	HRESULT result = swapChain_->ResizeBuffers(
		0,
		extent.width,
		extent.height,
		DXGI_FORMAT_UNKNOWN,
		0);

	if (FAILED(result)) return result;

	return CreateOutputAttachments(device, extent);
}

void D3D11PresentationSurface::BeginFrame(
	ID3D11DeviceContext* context,
	const Color4& clearColor) const
{
	Bind(context);

	context->ClearRenderTargetView(
		backBufferView_.Get(),
		clearColor.Data());

	context->ClearDepthStencilView(
		depthStencilView_.Get(),
		D3D11_CLEAR_DEPTH,
		1.0f,
		0);
}

void D3D11PresentationSurface::Bind(
	ID3D11DeviceContext* context) const noexcept
{
	ID3D11RenderTargetView* renderTargets[] = {
		backBufferView_.Get()
	};

	context->OMSetRenderTargets(
		1,
		renderTargets,
		depthStencilView_.Get());

	context->RSSetViewports(1, &viewport_);
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
	return swapChain_ != nullptr
		&& backBufferView_ != nullptr
		&& depthTexture_ != nullptr
		&& depthStencilView_ != nullptr;
}
