#pragma once
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include "Engine/Core/Extent2D.h"

class D3D11PresentationSurface
{
private:
	HRESULT CreateBackBuffer(ID3D11Device* device, Extent2D extent);

public:
	D3D11PresentationSurface() = default;
	~D3D11PresentationSurface() = default;
	D3D11PresentationSurface(const D3D11PresentationSurface&) = delete;
	D3D11PresentationSurface& operator=(const D3D11PresentationSurface&) = delete;

	[[nodiscard]]
	HRESULT Initialize(
		ID3D11Device* device,
		HWND outputWindow,
		Extent2D extent,
		bool vsync);

	[[nodiscard]]
	HRESULT Resize(
		ID3D11Device* device,
		ID3D11DeviceContext* context,
		Extent2D extent);

	// Update clearColor to dedicated color struct
	void BeginFrame(
		ID3D11DeviceContext* context,
		const float clearColor[4]) const;

	[[nodiscard]]
	HRESULT Present();

	[[nodiscard]]
	bool IsInitialized() const noexcept;

	[[nodiscard]]
	Extent2D Extent() const noexcept { return extent_; }

	[[nodiscard]]
	IDXGISwapChain1* GetSwapChain() const { return swapChain_.Get(); }
	

private:
	Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferView_;

	D3D11_VIEWPORT viewport_ = {};
	Extent2D extent_ = {};
	bool vsync_ = true;
};