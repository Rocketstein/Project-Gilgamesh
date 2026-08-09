#pragma once

#include "Engine/Render/D3D11/Shader/D3D11ShaderManager.h"
#include "PipelineDesc.h"

// Minimal Prototype Graphics Pipeline

class GraphicsPipeline final
{
public:
	HRESULT Initialize(
		ID3D11Device* device,
		const ShaderManager& shaders,
		const GraphicsPipelineDesc& desc);

	void Bind(
		ID3D11DeviceContext* context,
		const ShaderManager& shaders) const;

private:
	VertexShaderHandle vertexShader_;
	PixelShaderHandle pixelShader_;

	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;

	D3D11_PRIMITIVE_TOPOLOGY topology_ = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
};