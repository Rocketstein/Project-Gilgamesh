#pragma once

#include "Engine/Render/Pass/IRenderPass.h"

class EditorGridLinePass final : public IRenderPass
{
public:
    void Execute(
        RenderPassContext& context,
        const RenderFrameData& frame) override;
};
