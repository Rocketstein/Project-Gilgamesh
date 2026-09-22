#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Render/D3D11/Shader/D3D11ShaderManager.h"
#include "PipelineDesc.h"

class D3D11DrawSetup final
{
public:
    D3D11DrawSetup() = default;
    ~D3D11DrawSetup() = default;

    D3D11DrawSetup(const D3D11DrawSetup&) = delete;
    D3D11DrawSetup& operator=(const D3D11DrawSetup&) = delete;

    D3D11DrawSetup(D3D11DrawSetup&&) noexcept = default;
    D3D11DrawSetup& operator=(D3D11DrawSetup&&) noexcept = default;

    [[nodiscard]]
    HRESULT Initialize(
        ID3D11Device* device,
        const ShaderManager& shaders,
        const D3D11DrawSetupDesc& desc);

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
