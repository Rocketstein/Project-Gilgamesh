#pragma once
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "Engine/Render/D3D11/D3D11Shader.h"

// Responsibility Example
// resolves "Primitive"
// reads Primitive.vs.cso
// caches by logical name

// Shader classes
// consumes bytecode
// creates ID3D11VertexShader
// owns the resulting resource

enum class ShaderLoadError
{
    None,
    NotInitialized,
    InvalidName,
    FileOpenFailed,
    FileReadFailed,
    ShaderCreationFailed
};



template<typename Resource>
struct ShaderLoadResult
{
    Resource resource{};
    ShaderLoadError error = ShaderLoadError::None;
    std::filesystem::path path;
    HRESULT nativeResult = S_OK;

    bool Succeeded() const noexcept
    {
        return error == ShaderLoadError::None;
    }

    explicit operator bool() const noexcept
    {
        return Succeeded();
    }
};

class ShaderManager final
{
public:
	ShaderManager() = default;

	HRESULT Initialize(ID3D11Device* device, std::filesystem::path shaderDirectory);

	void Shutdown();

    ShaderLoadResult<VertexShaderHandle>
        LoadVertex(const std::filesystem::path& name);

    ShaderLoadResult<PixelShaderHandle>
        LoadPixel(const std::filesystem::path& name);


private:


};