#include "GraphicsPipeline.h"

HRESULT GraphicsPipeline::Initialize(
	ID3D11Device* device,
	const ShaderManager& shaders,
	const GraphicsPipelineDesc& desc)
{

}

void GraphicsPipeline::Bind(ID3D11DeviceContext* context) const
{
	context->IASetInputLayout(inputLayout_.Get());
	context->IASetPrimitiveTopology(topology_);
	context->VSSetShader(vertexShader_.Get(), nullptr, 0);
	context->PSSetShader(pixelShader_.Get(), nullptr, 0);
}

bool GraphicsPipeline::IsInitialized() const
{

	return false;
}