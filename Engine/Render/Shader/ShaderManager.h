#pragma once
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#include "Engine/Render/D3D11/D3D11Shader.h"
#include "Engine/Render/Shader/ShaderTypes.h"

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

	bool Initialize(ID3D11Device* device, std::filesystem::path shaderDirectory);
	void Shutdown();

    [[nodiscard]]
    bool IsInitialized() const noexcept;

    ShaderLoadResult<VertexShaderHandle>
        LoadVertex(const std::filesystem::path& name);

    ShaderLoadResult<PixelShaderHandle>
        LoadPixel(const std::filesystem::path& name);

    [[nodiscard]]
    ID3D11VertexShader* Get(
        VertexShaderHandle handle) const noexcept;

    [[nodiscard]]
    ID3D11PixelShader* Get(
        PixelShaderHandle handle) const noexcept;

    [[nodiscard]]
    std::span<const std::byte> GetBytecode(
        VertexShaderHandle handle) const noexcept;

    [[nodiscard]]
    std::span<const std::byte> GetBytecode(
        PixelShaderHandle handle) const noexcept;

private:
    template<typename Resource, ShaderStage Stage>
    struct ShaderStore
    {
        using Handle = ShaderHandle<Stage>;

        // Owns the actual GPU shader wrappers.
        std::vector<Resource> resources;

        // Finds an existing handle by logical name.
        std::unordered_map<std::wstring, Handle> cache;

        void Clear()
        {
            cache.clear();
            resources.clear();
        }
    };

    struct BytecodeResult
    {
        std::vector<std::byte> bytes;
        ShaderLoadError error = ShaderLoadError::None;
    };

    template<ShaderStage Stage>
    ShaderLoadResult<ShaderHandle<Stage>>
        LoadInternal(const std::filesystem::path& logicalName);

    template<ShaderStage Stage>
    auto& Store();

    [[nodiscard]]
    static std::filesystem::path NormalizeLogicalName(
        const std::filesystem::path& logicalName);

    [[nodiscard]]
    std::filesystem::path MakeShaderPath(
        const std::filesystem::path& logicalName,
        const wchar_t* suffix) const;

    [[nodiscard]]
    static std::wstring MakeCacheKey(
        const std::filesystem::path& logicalName);

    [[nodiscard]]
    static BytecodeResult ReadBytecode(
        const std::filesystem::path& path);

    ID3D11Device* device_ = nullptr;
    std::filesystem::path shaderDirectory_;

    ShaderStore<VertexShader, ShaderStage::Vertex>
        vertexShaders_;

    ShaderStore<PixelShader, ShaderStage::Pixel>
        pixelShaders_;

};