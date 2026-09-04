#pragma once
#include <d3d11.h>
#include <optional>
#include <span>

#include "Engine/Render/D3D11/Shader/ShaderTypes.h"

struct DepthStencilPipelineDesc
{
    bool depthTestEnabled = false;
    bool depthWriteEnabled = false;
    D3D11_COMPARISON_FUNC depthComparison =
        D3D11_COMPARISON_LESS;
};

struct GraphicsPipelineDesc
{
    VertexShaderHandle vertexShader;
    std::optional<PixelShaderHandle> pixelShader;

    std::span<const D3D11_INPUT_ELEMENT_DESC>
        inputElements;

    D3D11_PRIMITIVE_TOPOLOGY topology =
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    DepthStencilPipelineDesc depthStencil;
};
