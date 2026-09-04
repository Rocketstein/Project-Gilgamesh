#include "GraphicsPipeline.h"

#include <cassert>
#include <utility>

HRESULT GraphicsPipeline::Initialize(
	ID3D11Device* device,
	const ShaderManager& shaders,
	const GraphicsPipelineDesc& desc)
{
	if (device == nullptr
		|| !desc.vertexShader.IsValid()
		|| desc.topology == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
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

	D3D11_DEPTH_STENCIL_DESC depthStencilDescription{};
	depthStencilDescription.DepthEnable =
		desc.depthStencil.depthTestEnabled;
	depthStencilDescription.DepthWriteMask =
		desc.depthStencil.depthWriteEnabled
			? D3D11_DEPTH_WRITE_MASK_ALL
			: D3D11_DEPTH_WRITE_MASK_ZERO;
	depthStencilDescription.DepthFunc =
		desc.depthStencil.depthComparison;
	depthStencilDescription.StencilEnable = FALSE;

	const HRESULT depthStencilResult =
		device->CreateDepthStencilState(
			&depthStencilDescription,
			newDepthStencilState.GetAddressOf());

	if (FAILED(depthStencilResult))
	{
		return depthStencilResult;
	}

	// Commit only after every operation succeeds.
	vertexShader_ = std::move(newVertexShader);
	pixelShader_ = std::move(newPixelShader);
	inputLayout_ = std::move(newInputLayout);
	depthStencilState_ = std::move(newDepthStencilState);
	topology_ = desc.topology;
	isInitialized = true;

	return S_OK;
}

void GraphicsPipeline::Bind(ID3D11DeviceContext* context) const noexcept
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
}

bool GraphicsPipeline::IsInitialized() const noexcept
{
	return isInitialized;
}
