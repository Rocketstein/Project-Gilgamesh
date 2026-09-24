#pragma once

#include <cstdint>
#include <d3d11.h>
#include <limits>

#include "Engine/Core/Extent2D.h"

class RenderGraph;

// Identifies a texture declaration owned by one RenderGraph generation.
class RenderGraphTextureHandle final
{
public:
    constexpr RenderGraphTextureHandle() noexcept = default;

    // Reports only whether this handle is populated; RenderGraph checks lifetime.
    constexpr explicit operator bool() const noexcept
    {
        return index_ != InvalidIndex && ownerId_ != 0;
    }

    friend constexpr bool operator==(
        RenderGraphTextureHandle,
        RenderGraphTextureHandle) = default;

private:
    static constexpr std::uint32_t InvalidIndex =
        std::numeric_limits<std::uint32_t>::max();

    constexpr RenderGraphTextureHandle(
        std::uint32_t index,
        std::uint64_t generation,
        std::uint64_t ownerId) noexcept
        : index_(index), generation_(generation), ownerId_(ownerId)
    {
    }

    std::uint32_t index_ = InvalidIndex;
    std::uint64_t generation_ = 0;
    std::uint64_t ownerId_ = 0;

    friend class RenderGraph;
};

struct RenderGraphTextureDesc
{
    Extent2D extent{};
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

    std::uint32_t mipLevels = 1;
    std::uint32_t sampleCount = 1;
};

struct ImportedRenderGraphTexture
{
    ID3D11Texture2D* texture = nullptr;

    ID3D11RenderTargetView* renderTargetView = nullptr;
    ID3D11DepthStencilView* depthStencilView = nullptr;
    ID3D11ShaderResourceView* shaderResourceView = nullptr;
};

enum class RenderGraphLoadOp
{
    Load,
    Clear
};

enum class RenderGraphDepthAccess
{
    ReadOnly,
    ReadWrite
};
