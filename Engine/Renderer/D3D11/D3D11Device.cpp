#include "D3D11Device.h"

#include <array>

HRESULT D3D11Device::CreateWithFlags(UINT flags)
{
	constexpr std::array featureLevels {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	device_.Reset();
	context_.Reset();

	HRESULT result = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		flags,
		featureLevels.data(),
		static_cast<UINT>(featureLevels.size()),
		D3D11_SDK_VERSION,
		device_.ReleaseAndGetAddressOf(),
		&featureLevel_,
		context_.ReleaseAndGetAddressOf());
	
	if (result == E_INVALIDARG)
	{
		device_.Reset();
		context_.Reset();

		result = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			flags,
			featureLevels.data() + 1,
			static_cast<UINT>(featureLevels.size() - 1),
			D3D11_SDK_VERSION,
			device_.ReleaseAndGetAddressOf(),
			&featureLevel_,
			context_.ReleaseAndGetAddressOf());
	}

	if (FAILED(result))
	{
		device_.Reset();
		context_.Reset();
	}

	return result;
}

HRESULT D3D11Device::Initialize(bool requestDebugLayer)
{
	if (IsInitialized()) return S_FALSE;

	UINT flags = 0;

	if (requestDebugLayer) flags |= D3D11_CREATE_DEVICE_DEBUG;

	HRESULT result = CreateWithFlags(flags);

	if (result == DXGI_ERROR_SDK_COMPONENT_MISSING &&
		requestDebugLayer)
	{
		result = CreateWithFlags(
			flags & ~D3D11_CREATE_DEVICE_DEBUG);
	}

	return result;
}

HRESULT D3D11Device::DeviceRemovedReason() const
{
	if (device_ == nullptr) return E_POINTER;

	return device_->GetDeviceRemovedReason();
}