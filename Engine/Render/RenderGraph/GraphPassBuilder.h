#pragma once

#include "Engine/Core/Color.h"
#include "Pass/GraphPass.h"
#include "Resource/RenderGraphTexture.h"

class GraphPassBuilder final
{
public:
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
};