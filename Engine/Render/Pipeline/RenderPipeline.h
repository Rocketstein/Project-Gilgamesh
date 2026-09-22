#pragma once

class RenderPipeline final
{
public:
    RenderPipelineBuildResult Initialize(
        ID3D11Device* device,
        ShaderManager& shaders,
        PassRegistrar&& registrar);

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
        D3D11DrawSetup drawSetup;
    };

    std::vector<Entry> entries_;
    bool initialized_ = false;
};