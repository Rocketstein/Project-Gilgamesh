#pragma once
#include <d3d11.h>
#include <optional>
#include <span>

#include "Engine/Render/D3D11/Shader/ShaderTypes.h"

// Describes fixed draw state independently of how shaders are identified.
struct DrawStateDesc
{
    D3D11_PRIMITIVE_TOPOLOGY topology =
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    D3D11_CULL_MODE cullMode = D3D11_CULL_BACK;
    D3D11_FILL_MODE fillMode = D3D11_FILL_SOLID;

    bool depthTestEnabled = false;
    bool depthWriteEnabled = false;
    D3D11_COMPARISON_FUNC depthComparison =
        D3D11_COMPARISON_LESS;

    bool blendEnabled = false;
};

struct D3D11DrawSetupDesc
{
    VertexShaderHandle vertexShader;
    std::optional<PixelShaderHandle> pixelShader;

    std::span<const D3D11_INPUT_ELEMENT_DESC>
        inputElements;

    DrawStateDesc drawState;
};
