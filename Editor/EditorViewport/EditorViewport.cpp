#include "EditorViewport.h"

#include "Engine/Core/Logging/Logger.h"

#include <utility>

void EditorViewport::Initialize(ID3D11Device* device)
{
	device_ = device;
}

void EditorViewport::Shutdown() noexcept
{
	ReleaseResources();
	device_.Reset();
}

void EditorViewport::RequestResize(Extent2D extent) noexcept
{
	if (extent.width == 0 || extent.height == 0 || extent == extent_)
	{
		return;
	}

	pendingExtent_ = extent;
	resizePending_ = true;
}

bool EditorViewport::ApplyPendingResize()
{
	if (!resizePending_)
	{
		return false;
	}

	// Keep the current resources alive if replacement allocation fails.
	if (!CreateResources(pendingExtent_))
	{
		return false;
	}

	extent_ = pendingExtent_;
	resizePending_ = false;
	return true;
}

bool EditorViewport::IsReady() const noexcept
{
	return device_ != nullptr
		&& extent_.width > 0
		&& extent_.height > 0
		&& colorTexture_ != nullptr
		&& renderTargetView_ != nullptr
		&& shaderResourceView_ != nullptr
		&& depthTexture_ != nullptr
		&& depthStencilView_ != nullptr
		&& readOnlyDepthStencilView_ != nullptr;
}

Extent2D EditorViewport::GetExtent() const noexcept
{
	return extent_;
}

ID3D11Texture2D* EditorViewport::GetColorTexture() const noexcept
{
	return colorTexture_.Get();
}

ID3D11RenderTargetView*
EditorViewport::GetColorRenderTargetView() const noexcept
{
	return renderTargetView_.Get();
}

ID3D11ShaderResourceView*
EditorViewport::GetColorShaderResourceView() const noexcept
{
	return shaderResourceView_.Get();
}

ID3D11Texture2D* EditorViewport::GetDepthTexture() const noexcept
{
	return depthTexture_.Get();
}

ID3D11DepthStencilView*
EditorViewport::GetDepthStencilView() const noexcept
{
	return depthStencilView_.Get();
}

ID3D11DepthStencilView*
EditorViewport::GetReadOnlyDepthStencilView() const noexcept
{
	return readOnlyDepthStencilView_.Get();
}

bool EditorViewport::CreateResources(Extent2D extent)
{
	if (!device_)
	{
		GILGAMESH_LOG(Core, Error, "EditorViewport::CreateResources called with null device");
		return false;
	}
	if (extent.width == 0 || extent.height == 0)
	{
		GILGAMESH_LOG(Core, Error, "EditorViewport::CreateResources called with an empty extent");
		return false;
	}

	Microsoft::WRL::ComPtr<ID3D11Texture2D>
		newColorTexture;

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>
		newRenderTargetView;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>
		newShaderResourceView;

	Microsoft::WRL::ComPtr<ID3D11Texture2D>
		newDepthTexture;

	Microsoft::WRL::ComPtr<ID3D11DepthStencilView>
		newDepthStencilView;

	Microsoft::WRL::ComPtr<ID3D11DepthStencilView>
		newReadOnlyDepthStencilView;

	// Render target texture and render target view
	D3D11_TEXTURE2D_DESC textureDesc{};
	textureDesc.Width = extent.width;
	textureDesc.Height = extent.height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = device_->CreateTexture2D(&textureDesc, nullptr, newColorTexture.GetAddressOf());
	if (FAILED(hr))
	{
		GILGAMESH_LOG(Core, Error, "Failed to create color texture: HRESULT=0x{:X}", hr);
		return false;
	}

	hr = device_->CreateRenderTargetView(newColorTexture.Get(), nullptr, newRenderTargetView.GetAddressOf());
	if (FAILED(hr))
	{
		GILGAMESH_LOG(Core, Error, "Failed to create render target view: HRESULT=0x{:X}", hr);
		return false;
	}

	hr = device_->CreateShaderResourceView(newColorTexture.Get(), nullptr, newShaderResourceView.GetAddressOf());
	if (FAILED(hr))
	{
		GILGAMESH_LOG(Core, Error, "Failed to create shader resource view: HRESULT=0x{:X}", hr);
		return false;
	}

	// Depth texture and depth stencil view
	D3D11_TEXTURE2D_DESC depthDesc{};
	depthDesc.Width = extent.width;
	depthDesc.Height = extent.height;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	hr = device_->CreateTexture2D(&depthDesc, nullptr, newDepthTexture.GetAddressOf());
	if (FAILED(hr))
	{
		GILGAMESH_LOG(Core, Error, "Failed to create depth texture: HRESULT=0x{:X}", hr);
		return false;
	}

	hr = device_->CreateDepthStencilView(newDepthTexture.Get(), nullptr, newDepthStencilView.GetAddressOf());
	if (FAILED(hr))
	{
		GILGAMESH_LOG(Core, Error, "Failed to create depth stencil view: HRESULT=0x{:X}", hr);
		return false;
	}

	// Overlay passes test against the same depth texture without modifying it.
	D3D11_DEPTH_STENCIL_VIEW_DESC readOnlyDepthViewDesc{};
	readOnlyDepthViewDesc.Format = depthDesc.Format;
	readOnlyDepthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	readOnlyDepthViewDesc.Flags = D3D11_DSV_READ_ONLY_DEPTH;
	hr = device_->CreateDepthStencilView(
		newDepthTexture.Get(), &readOnlyDepthViewDesc, newReadOnlyDepthStencilView.GetAddressOf());
	if (FAILED(hr))
	{
		GILGAMESH_LOG(Core, Error, "Failed to create read-only depth stencil view: HRESULT=0x{:X}", hr);
		return false;
	}

	// Commit only after both depth views and all color resources are ready.
	colorTexture_ =
		std::move(newColorTexture);

	renderTargetView_ =
		std::move(newRenderTargetView);

	shaderResourceView_ =
		std::move(newShaderResourceView);

	depthTexture_ =
		std::move(newDepthTexture);

	depthStencilView_ =
		std::move(newDepthStencilView);

	readOnlyDepthStencilView_ =
		std::move(newReadOnlyDepthStencilView);

	return true;
}

void EditorViewport::ReleaseResources() noexcept
{
	renderTargetView_.Reset();
	shaderResourceView_.Reset();
	depthStencilView_.Reset();
	readOnlyDepthStencilView_.Reset();
	colorTexture_.Reset();
	depthTexture_.Reset();
	extent_ = {};
	pendingExtent_ = {};
	resizePending_ = false;
}
