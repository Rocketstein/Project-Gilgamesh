#pragma once
#include <d3d11.h>
#include <span>

#include "Engine/Render/D3D11/Shader/ShaderTypes.h"

struct GraphicsPipelineDesc
{
    VertexShaderHandle vertexShader;
    PixelShaderHandle pixelShader;

    std::span<const D3D11_INPUT_ELEMENT_DESC>
        inputElements;

    D3D11_PRIMITIVE_TOPOLOGY topology =
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};