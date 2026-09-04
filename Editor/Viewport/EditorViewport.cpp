#include "EditorViewport.h"

#include "Engine/Core/Color.h"
#include "Engine/Core/Logging/Logger.h"

namespace {
	Color4 clearColor = Color4{ 0.1f, 0.12f, 0.16f };
} // Anonymous Namespace

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
		return false;

	// Revise needed
	if (CreateResources(pendingExtent_))
	{
		extent_ = pendingExtent_;
		resizePending_ = false;
		return true;
	}
	return false;
}

void EditorViewport::Bind(ID3D11DeviceContext* context) const noexcept
{
	if (!context)
	{
		GILGAMESH_LOG(Core, Error, "EditorViewport::Bind called with null context");
		return;
	}
	if (!IsReady())
	{
		GILGAMESH_LOG(Core, Error, "EditorViewport::Bind called when viewport is not ready");
		return;
	}
	context->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), depthStencilView_.Get());
	context->RSSetViewports(1, &viewport_);
}

void EditorViewport::BindAndClear(ID3D11DeviceContext* context) const noexcept
{
	if (!context)
	{
		GILGAMESH_LOG(Core, Error, "EditorViewport::BindAndClear called with null context");
		return;
	}

	if (!IsReady())
	{
		GILGAMESH_LOG(Core, Error, "EditorViewport::BindAndClear called when viewport is not ready");
		return;
	}

	context->ClearRenderTargetView(renderTargetView_.Get(), clearColor.Data());
	context->ClearDepthStencilView(depthStencilView_.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	context->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), depthStencilView_.Get());
	context->RSSetViewports(1, &viewport_);
}

void EditorViewport::Unbind(ID3D11DeviceContext* context) const noexcept
{
	if (context == nullptr)
	{
		GILGAMESH_LOG(Core, Error, "EditorViewport::Unbind called with null context");
		return;
	}

	context->OMSetRenderTargets(0, nullptr, nullptr);
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
		&& depthStencilView_ != nullptr;
}

Extent2D EditorViewport::GetExtent() const noexcept
{
	return extent_;
}

ID3D11ShaderResourceView*
EditorViewport::GetShaderResourceView() const noexcept
{
	return shaderResourceView_.Get();
}

bool EditorViewport::CreateResources(Extent2D extent)
{
	if (!device_)
	{
		GILGAMESH_LOG(Core, Error, "EditorViewport::CreateResources called with null device");
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
	depthDesc.Width = extent_.width;
	depthDesc.Height = extent_.height;
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

	// Viewport
	viewport_.TopLeftX = 0.0f;
	viewport_.TopLeftY = 0.0f;
	viewport_.Width =
		static_cast<float>(extent_.width);
	viewport_.Height =
		static_cast<float>(extent_.height);
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;

	return true;
}

void EditorViewport::ReleaseResources() noexcept
{
	colorTexture_.Reset();
	renderTargetView_.Reset();
	shaderResourceView_.Reset();
	depthTexture_.Reset();
	depthStencilView_.Reset();
}