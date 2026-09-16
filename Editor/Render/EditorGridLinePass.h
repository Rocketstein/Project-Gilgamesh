#pragma once

#include "IRenderPass.h"

class EditorGridLinePass final : IRenderPass
{
public:
    void Execute(
        RenderPassContext& context,
        const RenderFrameData& frame) override;

private:
    EditorGridRenderer gridRenderer_;
};