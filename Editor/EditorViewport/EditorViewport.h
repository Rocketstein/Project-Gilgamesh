#pragma once

#include <cstdint>
#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Core/Extent2D.h"

// Owns viewport attachments and publishes complete replacements after a resize.
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

	// Shares the depth texture while preventing depth writes in overlay passes.
	[[nodiscard]]
	ID3D11DepthStencilView* GetReadOnlyDepthStencilView() const noexcept;

private:
	// Builds all replacement resources before changing the current attachments.
	bool CreateResources(Extent2D extent);

	// Releases views before their underlying viewport textures.
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

	// One depth texture with writable and read-only attachment views.
	Microsoft::WRL::ComPtr<ID3D11Texture2D> 
		depthTexture_;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> 
		depthStencilView_;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView>
		readOnlyDepthStencilView_;

	// Dimensions
	Extent2D extent_{};
	Extent2D pendingExtent_{};
	bool resizePending_ = false;
};
