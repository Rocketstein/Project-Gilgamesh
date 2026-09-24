#include "Engine/Render/Shader/ShaderManager.h"
#include "Engine/Render/Shader/ShaderReflection.h"
#include "Tests/TestFramework.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

namespace
{
    // Finds the shaders compiled beside the test executable.
    std::filesystem::path GetShaderDirectory()
    {
        std::array<wchar_t, MAX_PATH> executablePath{};
        const DWORD length = GetModuleFileNameW(
            nullptr,
            executablePath.data(),
            static_cast<DWORD>(executablePath.size()));
        if (length == 0 || length == executablePath.size()) return {};

        return std::filesystem::path{
            std::wstring_view{ executablePath.data(), length }
        }.parent_path() / L"Shaders";
    }

    // Reads the exact compiled bytes used by the shader manager.
    std::vector<std::byte> ReadShaderBytecode(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) return {};

        const std::streamsize length = file.tellg();
        if (length <= 0) return {};

        std::vector<std::byte> bytecode(static_cast<std::size_t>(length));
        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(bytecode.data()), length)) return {};

        return bytecode;
    }

    // Checks that a failed extraction did not replace any previous metadata.
    bool HasSentinelMetadata(const ShaderReflectionData& data)
    {
        return data.inputs.size() == 1 &&
            data.inputs[0].semanticName == "PreviousInput" &&
            data.inputs[0].semanticIndex == 3 &&
            data.bindings.size() == 1 &&
            data.bindings[0].name == "PreviousBinding" &&
            data.bindings[0].slot == 5 &&
            data.constantBuffers.size() == 1 &&
            data.constantBuffers[0].name == "PreviousBuffer" &&
            data.constantBuffers[0].variables.size() == 1 &&
            data.constantBuffers[0].variables[0].name == "previousValue";
    }
}

GILGAMESH_TEST("Engine.Render.ShaderReflection", "RejectsInvalidBytecodeWithoutChangingOutput")
{
    ShaderReflectionData data;
    data.inputs.push_back({ "PreviousInput", 3, D3D_REGISTER_COMPONENT_FLOAT32, 0x7 });
    data.bindings.push_back({ "PreviousBinding", D3D_SIT_CBUFFER, 5, 1 });
    data.constantBuffers.push_back({ "PreviousBuffer", 16, { { "previousValue", 0, 16 } } });

    // Empty and malformed bytecode must both preserve the caller's data.
    const HRESULT emptyResult = ExtractShaderReflection({}, data);
    GILGAMESH_CHECK_MESSAGE(FAILED(emptyResult),
        "empty shader bytecode must fail reflection");
    GILGAMESH_CHECK_MESSAGE(HasSentinelMetadata(data),
        "empty bytecode must leave previous metadata unchanged");

    const std::array<std::byte, 4> malformedBytecode{};
    const HRESULT malformedResult = ExtractShaderReflection(malformedBytecode, data);
    GILGAMESH_CHECK_MESSAGE(FAILED(malformedResult),
        "malformed shader bytecode must fail reflection");
    GILGAMESH_CHECK_MESSAGE(HasSentinelMetadata(data),
        "malformed bytecode must leave previous metadata unchanged");
}

GILGAMESH_TEST("Engine.Render.ShaderReflection", "ExtractsPrimitiveVertexContract")
{
    const auto bytecode = ReadShaderBytecode(GetShaderDirectory() / L"Primitive.vs.cso");
    GILGAMESH_CHECK_MESSAGE(!bytecode.empty(),
        "the registered Primitive vertex shader must be compiled for tests");
    if (bytecode.empty()) return;

    ShaderReflectionData data;
    const HRESULT result = ExtractShaderReflection(bytecode, data);
    GILGAMESH_CHECK_MESSAGE(SUCCEEDED(result),
        "compiled Primitive vertex bytecode must reflect successfully");
    if (FAILED(result)) return;

    // Match the vertex input contract without assuming reflection order.
    const auto position = std::find_if(data.inputs.begin(), data.inputs.end(),
        [](const auto& input)
        {
            return input.semanticName == "POSITION" && input.semanticIndex == 0;
        });
    const auto color = std::find_if(data.inputs.begin(), data.inputs.end(),
        [](const auto& input)
        {
            return input.semanticName == "COLOR" && input.semanticIndex == 0;
        });
    GILGAMESH_CHECK_MESSAGE(data.inputs.size() == 2 &&
        position != data.inputs.end() && color != data.inputs.end(),
        "Primitive VS must require POSITION0 and COLOR0 inputs");
    if (position != data.inputs.end() && color != data.inputs.end())
    {
        GILGAMESH_CHECK_MESSAGE(
            position->componentType == D3D_REGISTER_COMPONENT_FLOAT32 &&
            position->componentMask == 0x7 &&
            color->componentType == D3D_REGISTER_COMPONENT_FLOAT32 &&
            color->componentMask == 0x7,
            "Primitive VS inputs must both contain three float components");
    }

    // Match the constant buffer slot and its matrix layout.
    const auto binding = std::find_if(data.bindings.begin(), data.bindings.end(),
        [](const auto& resource)
        {
            return resource.name == "ObjectConstants";
        });
    GILGAMESH_CHECK_MESSAGE(binding != data.bindings.end(),
        "Primitive VS must bind ObjectConstants");
    if (binding != data.bindings.end())
    {
        GILGAMESH_CHECK_MESSAGE(binding->type == D3D_SIT_CBUFFER &&
            binding->slot == 2 && binding->count == 1,
            "ObjectConstants must be one constant buffer at b2");
    }

    const auto buffer = std::find_if(data.constantBuffers.begin(),
        data.constantBuffers.end(),
        [](const auto& candidate)
        {
            return candidate.name == "ObjectConstants";
        });
    GILGAMESH_CHECK_MESSAGE(buffer != data.constantBuffers.end(),
        "Primitive VS must expose the ObjectConstants layout");
    if (buffer != data.constantBuffers.end())
    {
        GILGAMESH_CHECK_MESSAGE(buffer->size == 64 &&
            buffer->variables.size() == 1 &&
            buffer->variables[0].name == "modelViewProjection" &&
            buffer->variables[0].offset == 0 &&
            buffer->variables[0].size == 64,
            "ObjectConstants must contain a 64-byte MVP matrix at offset zero");
    }
}

GILGAMESH_TEST("Engine.Render.ShaderReflection", "ExtractsPrimitivePixelContract")
{
    const auto bytecode = ReadShaderBytecode(GetShaderDirectory() / L"Primitive.ps.cso");
    GILGAMESH_CHECK_MESSAGE(!bytecode.empty(),
        "the registered Primitive pixel shader must be compiled for tests");
    if (bytecode.empty()) return;

    ShaderReflectionData data;
    const HRESULT result = ExtractShaderReflection(bytecode, data);
    GILGAMESH_CHECK_MESSAGE(SUCCEEDED(result),
        "compiled Primitive pixel bytecode must reflect successfully");
    if (FAILED(result)) return;

    // The pixel stage consumes interpolated color and no bound resources.
    const auto color = std::find_if(data.inputs.begin(), data.inputs.end(),
        [](const auto& input)
        {
            return input.semanticName == "COLOR" && input.semanticIndex == 0;
        });
    GILGAMESH_CHECK_MESSAGE(color != data.inputs.end() &&
        color->componentType == D3D_REGISTER_COMPONENT_FLOAT32 &&
        color->componentMask == 0x7,
        "Primitive PS must receive a float3 COLOR0 input");
    GILGAMESH_CHECK_MESSAGE(data.bindings.empty() && data.constantBuffers.empty(),
        "Primitive PS must not require resource bindings");
}

GILGAMESH_TEST("Engine.Render.ShaderReflection", "ManagerOwnsReflectedStageMetadata")
{
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    const HRESULT deviceResult = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_WARP,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        device.GetAddressOf(),
        nullptr,
        nullptr);
    GILGAMESH_CHECK_MESSAGE(SUCCEEDED(deviceResult) && device != nullptr,
        "the WARP device must be available for shader manager tests");
    if (FAILED(deviceResult) || !device) return;

    ShaderManager manager;
    GILGAMESH_CHECK_MESSAGE(manager.Initialize(device.Get(), GetShaderDirectory()),
        "the shader manager must initialize with compiled test shaders");
    if (!manager.IsInitialized()) return;

    const auto vertex = manager.LoadVertex(L"Primitive");
    const auto pixel = manager.LoadPixel(L"Primitive");
    GILGAMESH_CHECK_MESSAGE(vertex && pixel,
        "the shader manager must load both Primitive stages");
    if (!vertex || !pixel) return;

    // The manager's typed handles should expose metadata owned by each shader.
    const ShaderReflectionData* vertexData = manager.GetReflection(vertex.resource);
    const ShaderReflectionData* pixelData = manager.GetReflection(pixel.resource);
    GILGAMESH_CHECK_MESSAGE(vertexData != nullptr && pixelData != nullptr,
        "both loaded shader stages must retain reflection data");
    if (vertexData && pixelData)
    {
        GILGAMESH_CHECK_MESSAGE(
            vertexData->inputs.size() == 2 &&
            vertexData->constantBuffers.size() == 1 &&
            pixelData->bindings.empty() &&
            pixelData->constantBuffers.empty(),
            "each stage must retain its own distinct reflection contract");
    }

    GILGAMESH_CHECK_MESSAGE(
        manager.GetReflection(VertexShaderHandle{}) == nullptr &&
        manager.GetReflection(PixelShaderHandle{}) == nullptr,
        "invalid shader handles must not return reflection data");
}
