#pragma once

#include "Engine/Render/Pass/IRenderPass.h"
#include "Engine/Render/Pass/PassDescriptor.h"

class EditorGridLinePass final : public IRenderPass
{
public:
    [[nodiscard]] static PassDescriptor Describe();

    void Execute(
        RenderPassContext& context,
        const RenderFrameData& frame) override;
};
