#pragma once
#include <d3d11.h>
#include <wrl/client.h>

class D3D11Device
{
private:
	HRESULT CreateWithFlags(UINT flags);

public:
	D3D11Device() = default;
	~D3D11Device() = default;

	D3D11Device(const D3D11Device&) = delete;
	D3D11Device& operator=(const D3D11Device&) = delete;

	[[nodiscard]]
	HRESULT Initialize(bool requestDebugLayer);

	[[nodiscard]]
	bool IsInitialized() const { return device_ != nullptr && context_ != nullptr; }

	[[nodiscard]]
	ID3D11Device* GetDevice() const { return device_.Get(); }

	[[nodiscard]]
	ID3D11DeviceContext* GetDeviceContext() const { return context_.Get(); }

	[[nodiscard]]
	D3D_FEATURE_LEVEL GetFeatureLevel() const { return featureLevel_; }

	[[nodiscard]]
	HRESULT DeviceRemovedReason() const;

private:
	Microsoft::WRL::ComPtr<ID3D11Device> device_;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;

	D3D_FEATURE_LEVEL featureLevel_ =
		D3D_FEATURE_LEVEL_11_0;
};