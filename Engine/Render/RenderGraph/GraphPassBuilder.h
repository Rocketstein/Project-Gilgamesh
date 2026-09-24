#pragma once

#include <cstdint>
#include <vector>

#include "Engine/Core/Color.h"
#include "Engine/Render/RenderGraph/Resource/RenderGraphTexture.h"

class RenderGraph;

// Records texture access declarations while RenderGraph::AddPass is running.
class GraphPassBuilder final
{
public:
    GraphPassBuilder(const GraphPassBuilder&) = delete;
    GraphPassBuilder& operator=(const GraphPassBuilder&) = delete;
    GraphPassBuilder(GraphPassBuilder&&) = delete;
    GraphPassBuilder& operator=(GraphPassBuilder&&) = delete;

    void SetColorTarget(
        std::uint32_t slot,
        RenderGraphTextureHandle texture,
        RenderGraphLoadOp loadOp,
        const Color4& clearColor = {});

    void SetDepthTarget(
        RenderGraphTextureHandle texture,
        RenderGraphDepthAccess access,
        RenderGraphLoadOp loadOp,
        float clearDepth = 1.0f);

    void ReadVertexTexture(
        RenderGraphTextureHandle texture,
        std::uint32_t slot);

    void ReadPixelTexture(
        RenderGraphTextureHandle texture,
        std::uint32_t slot);

private:
    // A single normalized record represents attachments and shader reads.
    struct TextureAccess
    {
        enum class Type
        {
            ColorTarget,
            DepthTarget,
            VertexShaderRead,
            PixelShaderRead
        };

        RenderGraphTextureHandle texture;
        Type type = Type::ColorTarget;
        std::uint32_t slot = 0;
        RenderGraphLoadOp loadOp = RenderGraphLoadOp::Load;
        RenderGraphDepthAccess depthAccess =
            RenderGraphDepthAccess::ReadOnly;
        Color4 clearColor{};
        float clearDepth = 1.0f;
    };

    GraphPassBuilder() = default;

    std::vector<TextureAccess> textureAccesses_;

    friend class RenderGraph;
};
