#pragma once

struct RenderPassContext;
class RenderFrameData;

class IRenderPass
{
public:
    virtual ~IRenderPass() = default;

    // Executes into the target already bound by the viewport coordinator.
    virtual void Execute(
        RenderPassContext& context,
        const RenderFrameData& frame) = 0;
};
