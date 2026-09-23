#pragma once
#include "Pass/IRenderPass.h"
#include "Pass/PassDescriptor.h"
#include "Pass/RenderFrameData.h"
#include "Pass/RenderPassContext.h"
#include "Engine/Render/DrawSetup/DrawSetup.h"

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
    struct Entry
    {
        PassDescriptor descriptor;
        std::unique_ptr<IRenderPass> pass;
        DrawSetup drawSetup;
    };

    std::vector<Entry> entries_;
    bool initialized_ = false;
};