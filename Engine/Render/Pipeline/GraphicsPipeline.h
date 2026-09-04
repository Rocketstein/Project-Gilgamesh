#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Render/D3D11/Shader/D3D11ShaderManager.h"
#include "PipelineDesc.h"

class GraphicsPipeline final
{
public:
    GraphicsPipeline() = default;
    ~GraphicsPipeline() = default;

    GraphicsPipeline(const GraphicsPipeline&) = delete;
    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

    GraphicsPipeline(GraphicsPipeline&&) noexcept = default;
    GraphicsPipeline& operator=(GraphicsPipeline&&) noexcept = default;

    [[nodiscard]]
    HRESULT Initialize(
        ID3D11Device* device,
        const ShaderManager& shaders,
        const GraphicsPipelineDesc& desc);

    void Bind(
        ID3D11DeviceContext* context) const noexcept;

    [[nodiscard]]
    bool IsInitialized() const noexcept;

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader>
        vertexShader_;

    Microsoft::WRL::ComPtr<ID3D11PixelShader>
        pixelShader_;

    Microsoft::WRL::ComPtr<ID3D11InputLayout>
        inputLayout_;

    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>
        depthStencilState_;

    D3D11_PRIMITIVE_TOPOLOGY topology_ =
        D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

    bool isInitialized = false;
};
