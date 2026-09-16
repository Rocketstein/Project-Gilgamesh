#pragma once

#include "Engine/Render/D3D11/Shader/D3D11ShaderManager.h"

class IRenderPass
{
public:
    virtual ~IRenderPass() = default;
    virtual void Execute(
        RenderPassContext& context,
        const RenderFrameData& frame) = 0;

};