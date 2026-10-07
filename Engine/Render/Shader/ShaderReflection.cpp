#include "ShaderReflection.h"

#include <d3dcompiler.h>
#include <utility>
#include <wrl/client.h>

HRESULT ExtractShaderReflection(
    std::span<const std::byte> bytecode,
    ShaderReflectionData& output)
{
    if (bytecode.empty())
    {
        return E_INVALIDARG;
    }

    // Reflect the compiled shader once; copy all data before releasing COM.
    Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflection;
    HRESULT result = D3DReflect(
        bytecode.data(),
        bytecode.size(),
        IID_PPV_ARGS(reflection.GetAddressOf()));
    if (FAILED(result))
    {
        return result;
    }

    D3D11_SHADER_DESC shaderDescription{};
    result = reflection->GetDesc(&shaderDescription);
    if (FAILED(result))
    {
        return result;
    }

    ShaderReflectionData extracted;
    extracted.inputs.reserve(shaderDescription.InputParameters);
    extracted.bindings.reserve(shaderDescription.BoundResources);
    extracted.constantBuffers.reserve(shaderDescription.ConstantBuffers);

    // Record the signature of this compiled shader stage.
    for (UINT index = 0; index < shaderDescription.InputParameters; ++index)
    {
        D3D11_SIGNATURE_PARAMETER_DESC description{};
        result = reflection->GetInputParameterDesc(index, &description);
        if (FAILED(result))
        {
            return result;
        }

        extracted.inputs.push_back({
            description.SemanticName ? description.SemanticName : "",
            description.SemanticIndex,
            description.ComponentType,
            description.Mask
        });
    }

    // Resource bindings contain the stage's actual register assignments.
    for (UINT index = 0; index < shaderDescription.BoundResources; ++index)
    {
        D3D11_SHADER_INPUT_BIND_DESC description{};
        result = reflection->GetResourceBindingDesc(index, &description);
        if (FAILED(result))
        {
            return result;
        }

        extracted.bindings.push_back({
            description.Name ? description.Name : "",
            description.Type,
            description.BindPoint,
            description.BindCount
        });
    }

    // Copy constant-buffer sizes and member offsets for CPU layout checks.
    for (UINT bufferIndex = 0;
        bufferIndex < shaderDescription.ConstantBuffers;
        ++bufferIndex)
    {
        ID3D11ShaderReflectionConstantBuffer* buffer =
            reflection->GetConstantBufferByIndex(bufferIndex);
        if (!buffer)
        {
            return E_FAIL;
        }

        D3D11_SHADER_BUFFER_DESC bufferDescription{};
        result = buffer->GetDesc(&bufferDescription);
        if (FAILED(result))
        {
            return result;
        }

        ShaderReflectionData::ConstantBuffer constantBuffer;
        constantBuffer.name = bufferDescription.Name
            ? bufferDescription.Name
            : "";
        constantBuffer.size = bufferDescription.Size;
        constantBuffer.variables.reserve(bufferDescription.Variables);

        for (UINT variableIndex = 0;
            variableIndex < bufferDescription.Variables;
            ++variableIndex)
        {
            ID3D11ShaderReflectionVariable* variable =
                buffer->GetVariableByIndex(variableIndex);
            if (!variable)
            {
                return E_FAIL;
            }

            D3D11_SHADER_VARIABLE_DESC variableDescription{};
            result = variable->GetDesc(&variableDescription);
            if (FAILED(result))
            {
                return result;
            }

            constantBuffer.variables.push_back({
                variableDescription.Name ? variableDescription.Name : "",
                variableDescription.StartOffset,
                variableDescription.Size
            });
        }

        extracted.constantBuffers.push_back(std::move(constantBuffer));
    }

    output = std::move(extracted);
    return S_OK;
}
