#include "D3D11DrawSetup.h"

#include <cassert>
#include <utility>

HRESULT D3D11DrawSetup::Initialize(
	ID3D11Device* device,
	const ShaderManager& shaders,
	const D3D11DrawSetupDesc& desc)
{
	if (device == nullptr
		|| !desc.vertexShader.IsValid()
		|| desc.drawState.topology == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
	{
		return E_INVALIDARG;
	}

	ID3D11VertexShader* nativeVS =
		shaders.Get(desc.vertexShader);

	const std::span<const std::byte> vertexBytecode =
		shaders.GetBytecode(desc.vertexShader);

	if (nativeVS == nullptr || vertexBytecode.empty())
	{
		return E_INVALIDARG;
	}

	Microsoft::WRL::ComPtr<ID3D11VertexShader>
		newVertexShader{ nativeVS };

	Microsoft::WRL::ComPtr<ID3D11PixelShader>
		newPixelShader;

	Microsoft::WRL::ComPtr<ID3D11InputLayout>
		newInputLayout;

	Microsoft::WRL::ComPtr<ID3D11DepthStencilState>
		newDepthStencilState;

	Microsoft::WRL::ComPtr<ID3D11RasterizerState>
		newRasterizerState;

	Microsoft::WRL::ComPtr<ID3D11BlendState>
		newBlendState;

	if (desc.pixelShader.has_value())
	{
		if (!desc.pixelShader->IsValid())
		{
			return E_INVALIDARG;
		}

		ID3D11PixelShader* nativePS =
			shaders.Get(*desc.pixelShader);

		if (nativePS == nullptr)
		{
			return E_INVALIDARG;
		}

		newPixelShader = nativePS;
	}

	if (!desc.inputElements.empty())
	{
		const HRESULT result = device->CreateInputLayout(desc.inputElements.data(),
			static_cast<UINT>(desc.inputElements.size()),
			vertexBytecode.data(),
			vertexBytecode.size(),
			newInputLayout.GetAddressOf());

		if (FAILED(result))
		{
			return result;
		}
	}

	// Configure Rasterizer State
	D3D11_RASTERIZER_DESC rasterizerDescription{};
	rasterizerDescription.CullMode = desc.drawState.cullMode;
	rasterizerDescription.FillMode = desc.drawState.fillMode;
	rasterizerDescription.DepthClipEnable = TRUE;

	HRESULT hr =
		device->CreateRasterizerState(
			&rasterizerDescription,
			newRasterizerState.GetAddressOf());
	if (FAILED(hr))
		return hr;

	// Configure Depth Stencil State
	D3D11_DEPTH_STENCIL_DESC depthStencilDescription{};
	depthStencilDescription.DepthEnable =
		desc.drawState.depthTestEnabled;
	depthStencilDescription.DepthWriteMask =
		desc.drawState.depthWriteEnabled
			? D3D11_DEPTH_WRITE_MASK_ALL
			: D3D11_DEPTH_WRITE_MASK_ZERO;
	depthStencilDescription.DepthFunc =
		desc.drawState.depthComparison;
	depthStencilDescription.StencilEnable = FALSE;

	hr =
		device->CreateDepthStencilState(
			&depthStencilDescription,
			newDepthStencilState.GetAddressOf());
	if (FAILED(hr))
		return hr;

	// Configure Blend State
	D3D11_BLEND_DESC blendDescription{};
	blendDescription.AlphaToCoverageEnable = FALSE;
	blendDescription.IndependentBlendEnable = FALSE;

	D3D11_RENDER_TARGET_BLEND_DESC& rtBlendDescription = blendDescription.RenderTarget[0];
	rtBlendDescription.BlendEnable = desc.drawState.blendEnabled;
	rtBlendDescription.BlendOp = D3D11_BLEND_OP_ADD;
	rtBlendDescription.SrcBlend = D3D11_BLEND_SRC_ALPHA;
	rtBlendDescription.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	rtBlendDescription.SrcBlendAlpha = D3D11_BLEND_ONE;
	rtBlendDescription.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	rtBlendDescription.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	rtBlendDescription.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	hr =
		device->CreateBlendState(
			&blendDescription,
			newBlendState.GetAddressOf());
	if (FAILED(hr))
		return hr;

	// Commit only after every operation succeeds.
	vertexShader_ = std::move(newVertexShader);
	pixelShader_ = std::move(newPixelShader);
	inputLayout_ = std::move(newInputLayout);
	depthStencilState_ = std::move(newDepthStencilState);
	rasterizerState_ = std::move(newRasterizerState);
	blendState_ = std::move(newBlendState);
	topology_ = desc.drawState.topology;

	return S_OK;
}

void D3D11DrawSetup::Apply(ID3D11DeviceContext* context) const noexcept
{
	assert(context != nullptr);
	assert(IsInitialized());

	if (context == nullptr || !IsInitialized())
	{
		return;
	}

	context->IASetInputLayout(inputLayout_.Get());
	context->IASetPrimitiveTopology(topology_);

	context->VSSetShader(
		vertexShader_.Get(),
		nullptr,
		0);

	context->PSSetShader(
		pixelShader_.Get(),
		nullptr,
		0);

	context->OMSetDepthStencilState(
		depthStencilState_.Get(),
		0);

	context->RSSetState(rasterizerState_.Get());

	context->OMSetBlendState(
		blendState_.Get(),
		nullptr,
		0xFFFFFFFFu);
}

bool D3D11DrawSetup::IsInitialized() const noexcept
{
	return vertexShader_ != nullptr
		&& depthStencilState_ != nullptr
		&& rasterizerState_ != nullptr
		&& blendState_ != nullptr
		&& topology_ != D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
}
