#include "D3D11ShaderManager.h"

#include <fstream>
#include <utility>

namespace
{
    template<ShaderStage>
    inline constexpr bool UnsupportedShaderStage = false;

    template<ShaderStage Stage>
    struct ShaderTraits;

    template<>
    struct ShaderTraits<ShaderStage::Vertex>
    {
        using Resource = VertexShader;

        static constexpr const wchar_t* FileSuffix =
            L".vs.cso";
    };

    template<>
    struct ShaderTraits<ShaderStage::Pixel>
    {
        using Resource = PixelShader;

        static constexpr const wchar_t* FileSuffix =
            L".ps.cso";
    };
}

bool ShaderManager::Initialize(ID3D11Device* device, std::filesystem::path shaderDirectory)
{
    if (device == nullptr || shaderDirectory.empty() || !shaderDirectory.is_absolute())
        return false;

    Shutdown();

    device_ = device;
    shaderDirectory_ = shaderDirectory.lexically_normal();
    return true;
}

void ShaderManager::Shutdown()
{
    pixelShaders_.Clear();
    vertexShaders_.Clear();

    shaderDirectory_.clear();
    device_ = nullptr;
}

bool ShaderManager::IsInitialized() const noexcept
{
    return device_ != nullptr && !shaderDirectory_.empty();
}

std::filesystem::path ShaderManager::NormalizeLogicalName(
    const std::filesystem::path& logicalName)
{
    if (logicalName.empty() ||
        logicalName.is_absolute() ||
        logicalName.has_root_name())
    {
        return {};
    }

    const auto normalized =
        logicalName.lexically_normal();

    if (normalized.empty() ||
        normalized == L"." ||
        normalized.filename().empty())
    {
        return {};
    }

    for (const auto& component : normalized)
    {
        if (component == L"..")
            return {};
    }

    return normalized;
}

std::filesystem::path ShaderManager::MakeShaderPath(
    const std::filesystem::path& logicalName,
    const wchar_t* suffix) const
{
    auto path = shaderDirectory_ / logicalName;
    path += suffix;

    return path;
}

std::wstring ShaderManager::MakeCacheKey(
    const std::filesystem::path& logicalName)
{
    return logicalName.generic_wstring();
}

ShaderManager::BytecodeResult ShaderManager::ReadBytecode(const std::filesystem::path& path)
{
    // Open in binary mode because a .cso is not text.
    //
    // "ate" means the initial file position is at the end.
    // This lets us immediately determine the file size.
    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file)
    {
        return { .error = ShaderLoadError::FileOpenFailed };
    }

    const std::streamsize size = file.tellg();
    if (size <= 0)
    {
        return { .error = ShaderLoadError::FileReadFailed };
    }

    // Move the read position from the end back to the start.
    file.seekg(0, std::ios::beg);

    // Allocate enough memory for the complete file.
    std::vector<std::byte> bytes(
        static_cast<std::size_t>(size));

    // Read the complete file into the vector.
    if (!file.read(
        reinterpret_cast<char*>(bytes.data()),
        size))
    {
        return { .error = ShaderLoadError::FileReadFailed };
    }

    // Transfer ownership of the allocated byte buffer
    // into the returned result.
    return {
        .bytes = std::move(bytes)
    };
}

template<ShaderStage Stage>
auto& ShaderManager::Store()
{
    // Add compute shader branch later
    if constexpr (Stage == ShaderStage::Vertex)
    {
        return vertexShaders_;
    }
    else if constexpr (Stage == ShaderStage::Pixel)
    {
        return pixelShaders_;
    }
    else
    {
        static_assert(
            UnsupportedShaderStage<Stage>,
            "Unsupported shader stage");
    }
}

template<ShaderStage Stage>
ShaderLoadResult<ShaderHandle<Stage>>
ShaderManager::LoadInternal(
    const std::filesystem::path& logicalName)
{
    // Select the compile-time metadata for this shader stage.
    //
    // For Stage == Vertex:
    //   Traits::Resource   == VertexShader
    //   Traits::FileSuffix == L".vs.cso"
    //
    // For Stage == Pixel:
    //   Traits::Resource   == PixelShader
    //   Traits::FileSuffix == L".ps.cso"
    using Traits = ShaderTraits<Stage>;

    // The concrete GPU resource class for this stage.
    using Resource = typename Traits::Resource;

    // The typed handle returned by this load operation.
    //
    // Vertex stage -> VertexShaderHandle
    // Pixel stage  -> PixelShaderHandle
    using Handle = ShaderHandle<Stage>;

    if (!IsInitialized()) 
    {
        return { .error = ShaderLoadError::NotInitialized };
    }

    const auto normalized = NormalizeLogicalName(logicalName);

    if (normalized.empty())
    {
        return { .error = ShaderLoadError::InvalidName };
    }

    // Select the correct stage-specific store.
    auto& store = Store<Stage>();

    const std::wstring key = MakeCacheKey(normalized);

    const auto path = MakeShaderPath(
        normalized,
        Traits::FileSuffix
    );

    // Check whether this exact shader stage has already been loaded and cached.
    if (const auto found = store.cache.find(key);
        found != store.cache.end())
    {
        return {
            // Reuse the existing typed handle.
            .resource = found->second,
            .path = path
        };
    }

    // Read the compiled .cso file into a byte vector.
    auto bytecode = ReadBytecode(path);

    if (bytecode.error != ShaderLoadError::None)
    {
        return {
            .error = bytecode.error,
            .path = path
        };
    }

    Resource shader;

    const HRESULT result = shader.Initialize(
        device_,
        std::move(bytecode.bytes));

    if (FAILED(result))
    {
        return {
            .error =
                ShaderLoadError::ShaderCreationFailed,
            .path = path,
            .nativeResult = result
        };
    }

    const Handle handle{
        static_cast<std::uint32_t>(
            store.resources.size())
    };

    // Associate the logical shader name with its new typed handle.
    store.resources.push_back(std::move(shader));
    store.cache.emplace(key, handle);

    // Return the handle to the caller.
    //
    // The caller can later pass it to:
    //   shaderManager.Get(handle)
    // or, for a vertex shader:
    //   shaderManager.GetBytecode(handle)
    return {
        .resource = handle,
        .path = path
    };
}


ShaderLoadResult<VertexShaderHandle>
ShaderManager::LoadVertex(
    const std::filesystem::path& logicalName)
{
    return LoadInternal<ShaderStage::Vertex>(logicalName);
}

ShaderLoadResult<PixelShaderHandle>
ShaderManager::LoadPixel(
    const std::filesystem::path& logicalName)
{
    return LoadInternal<ShaderStage::Pixel>(logicalName);
}


ID3D11VertexShader* ShaderManager::Get(
    VertexShaderHandle handle) const noexcept
{
    if (!handle.IsValid() ||
        handle.index_ >= vertexShaders_.resources.size())
    {
        return nullptr;
    }

    return vertexShaders_
        .resources[handle.index_]
        .GetNativeHandle();
}

ID3D11PixelShader* ShaderManager::Get(
    PixelShaderHandle handle) const noexcept
{
    if (!handle.IsValid() ||
        handle.index_ >= pixelShaders_.resources.size())
    {
        return nullptr;
    }

    return pixelShaders_
        .resources[handle.index_]
        .GetNativeHandle();
}

std::span<const std::byte>
ShaderManager::GetBytecode(
    VertexShaderHandle handle) const noexcept
{
    if (!handle.IsValid() ||
        handle.index_ >= vertexShaders_.resources.size())
    {
        return {};
    }

    return vertexShaders_
        .resources[handle.index_]
        .GetBytecode();
}

std::span<const std::byte>
ShaderManager::GetBytecode(
    PixelShaderHandle handle) const noexcept
{
    if (!handle.IsValid() ||
        handle.index_ >= pixelShaders_.resources.size())
    {
        return {};
    }

    return pixelShaders_
        .resources[handle.index_]
        .GetBytecode();
}