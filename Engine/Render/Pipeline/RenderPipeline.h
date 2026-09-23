#pragma once
#include "Pass/IRenderPass.h"
#include "Pass/PassDescriptor.h"
#include "Pass/RenderFrameData.h"
#include "Pass/RenderPassContext.h"
#include "Engine/Render/Buffers/ConstantBuffer.h"
#include "Engine/Render/DrawSetup/DrawSetup.h"
#include "Engine/Render/Pipeline/Resource/RenderConstants.h"

class RenderPipeline final
{
public:
    template<class Pass, class... Args>
    Pass& AddPass(Args&&... args)
    {
        PassDescriptor descriptor = Pass::Describe();
        auto pass = std::make_unique<Pass>(std::forward<Args>(args)...);
        Pass& registered = *pass;
        entries_.push_back({ std::move(descriptor), std::move(pass) });
        return registered;
    }

    bool Initialize(
        ID3D11Device* device,
        ShaderManager& shaders);

    void Execute(
        RenderPassContext& context,
        const RenderFrameData& frame);

    void Reset() noexcept;

    [[nodiscard]]
    bool IsInitialized() const noexcept;

private:
    // Runs before every other rendering pass. Uploads global constants extracted from the scene to b0.
    // TODO: Revise function parameters after implementing scene and ECS.
    void BuildFrame(RenderPassContext& context);

private:
    struct Entry
    {
        PassDescriptor descriptor;
        std::unique_ptr<IRenderPass> pass;
        DrawSetup drawSetup;
    };

    ConstantBuffer<FrameConstants> frameCB_;
    std::vector<Entry> entries_;
    bool initialized_ = false;
};