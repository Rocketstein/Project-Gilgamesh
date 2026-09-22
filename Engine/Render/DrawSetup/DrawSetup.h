#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Render/Shader/ShaderManager.h"
#include "DrawSetupDesc.h"

class DrawSetup final
{
public:
    DrawSetup() = default;
    ~DrawSetup() = default;

    DrawSetup(const DrawSetup&) = delete;
    DrawSetup& operator=(const DrawSetup&) = delete;

    DrawSetup(DrawSetup&&) noexcept = default;
    DrawSetup& operator=(DrawSetup&&) noexcept = default;

    [[nodiscard]]
    HRESULT Initialize(
        ID3D11Device* device,
        const ShaderManager& shaders,
        const DrawSetupDesc& desc);

    void Apply(
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

    Microsoft::WRL::ComPtr<ID3D11RasterizerState>
        rasterizerState_;

    Microsoft::WRL::ComPtr<ID3D11BlendState>
        blendState_;

    D3D11_PRIMITIVE_TOPOLOGY topology_ =
        D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
};
