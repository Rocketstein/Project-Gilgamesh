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

	void Bind(ID3D11DeviceContext* context) const noexcept;

	void BindAndClear(ID3D11DeviceContext* context) const noexcept;

	void Unbind(
		ID3D11DeviceContext* context) const noexcept;

	[[nodiscard]]
	bool IsReady() const noexcept;

	// Accessors
	[[nodiscard]]
	Extent2D GetExtent() const noexcept;

	[[nodiscard]]
	ID3D11ShaderResourceView*
		GetShaderResourceView() const noexcept;


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

	D3D11_VIEWPORT viewport_{};

	// Dimensions
	Extent2D extent_{};
	Extent2D pendingExtent_{};
	bool resizePending_ = false;
};
