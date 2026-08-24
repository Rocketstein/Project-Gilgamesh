#pragma once

#include "Engine/Render/D3D11/Shader/D3D11ShaderManager.h"
#include "PipelineDesc.h"

// Minimal Prototype Graphics Pipeline

class GraphicsPipeline final
{
public:
    GraphicsPipeline() = default;
    ~GraphicsPipeline() = default;

    GraphicsPipeline(const GraphicsPipeline&) = delete;
    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

    GraphicsPipeline(GraphicsPipeline&&) noexcept = default;
    GraphicsPipeline& operator=(GraphicsPipeline&&) noexcept = default;

    HRESULT Initialize(
        ID3D11Device* device,
        const ShaderManager& shaders,
        const GraphicsPipelineDesc& desc);

    void Bind(ID3D11DeviceContext* context) const noexcept;

    [[nodiscard]]
    bool IsInitialized() const noexcept;

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;

    D3D11_PRIMITIVE_TOPOLOGY topology_ =
        D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
};