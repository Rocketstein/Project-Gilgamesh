#pragma once

#include <cstdint>
#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Core/Extent2D.h"

class EditorViewport
{
public:
	EditorViewport() = default;
	~EditorViewport() = default;

	EditorViewport(const EditorViewport&) = delete;
	EditorViewport& operator=(const EditorViewport&) = delete;

	void Initialize(ID3D11Device* device);
	void Shutdown() noexcept;

	void RequestResize(Extent2D extent) noexcept;
	bool ApplyPendingResize();

	[[nodiscard]]
	bool IsReady() const noexcept;

	// Accessors
	[[nodiscard]]
	Extent2D GetExtent() const noexcept;

	// Borrowed native pointers remain valid until the next resize or shutdown.
	[[nodiscard]]
	ID3D11Texture2D* GetColorTexture() const noexcept;

	[[nodiscard]]
	ID3D11RenderTargetView* GetColorRenderTargetView() const noexcept;

	[[nodiscard]]
	ID3D11ShaderResourceView* GetColorShaderResourceView() const noexcept;

	[[nodiscard]]
	ID3D11Texture2D* GetDepthTexture() const noexcept;

	[[nodiscard]]
	ID3D11DepthStencilView* GetDepthStencilView() const noexcept;

private:
	bool CreateResources(Extent2D extent);
	void ReleaseResources() noexcept;

private:
	Microsoft::WRL::ComPtr<ID3D11Device>
		device_;

	// Render target texture and render target view for the viewport
	Microsoft::WRL::ComPtr<ID3D11Texture2D> 
		colorTexture_;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> 
		renderTargetView_;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> 
		shaderResourceView_;

	// Depth texture and depth stencil view for the viewport
	Microsoft::WRL::ComPtr<ID3D11Texture2D> 
		depthTexture_;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> 
		depthStencilView_;

	// Dimensions
	Extent2D extent_{};
	Extent2D pendingExtent_{};
	bool resizePending_ = false;
};
