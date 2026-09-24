#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <string>
#include <vector>
#include <d3d11shader.h>

struct ShaderReflectionData
{
    struct InputParameter
    {
        std::string semanticName;
        std::uint32_t semanticIndex = 0;
        D3D_REGISTER_COMPONENT_TYPE componentType{};
        std::uint8_t componentMask = 0;
    };

    struct ResourceBinding
    {
        std::string name;
        D3D_SHADER_INPUT_TYPE type{};
        std::uint32_t slot = 0;
        std::uint32_t count = 0;
    };

    struct ConstantVariable
    {
        std::string name;
        std::uint32_t offset = 0;
        std::uint32_t size = 0;
    };

    struct ConstantBuffer
    {
        std::string name;
        std::uint32_t size = 0;
        std::vector<ConstantVariable> variables;
    };

    std::vector<InputParameter> inputs;
    std::vector<ResourceBinding> bindings;
    std::vector<ConstantBuffer> constantBuffers;
};

// Copies the compiled shader's input and resource contract into owned data.
// The output is unchanged if reflection fails.
[[nodiscard]]
HRESULT ExtractShaderReflection(
    std::span<const std::byte> bytecode,
    ShaderReflectionData& output);
