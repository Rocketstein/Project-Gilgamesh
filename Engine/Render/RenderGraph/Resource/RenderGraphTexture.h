#pragma once

#include <d3d11.h>
#include <limits>

#include "Engine/Core/Extent2D.h"

struct RenderGraphTextureHandle final
{
    static constexpr std::uint32_t InvalidIndex =
        std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index = InvalidIndex;
    std::uint32_t generation = 0;

    [[nodiscard]]
    explicit operator bool() const noexcept
    {
        return index != InvalidIndex;
    }
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