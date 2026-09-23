#include "RenderPipeline.h"

#include <cstdint>
#include <optional>
#include <utility>

#include "Engine/Core/Logging/Logger.h"
#include "Engine/Render/Shader/ShaderManager.h"
#include "Engine/Render/Pipeline/Pass/RenderPassContext.h"

bool RenderPipeline::Initialize(
    ID3D11Device* device,
    ShaderManager& shaders)
{
    if (initialized_)
    {
        GILGAMESH_LOG(
            Renderer,
            Error,
            "Cannot initialize an already initialized render pipeline");
        return false;
    }

    if (device == nullptr)
    {
        GILGAMESH_LOG(
            Renderer,
            Error,
            "Cannot initialize the render pipeline with a null device");
        return false;
    }

    if (!shaders.IsInitialized())
    {
        GILGAMESH_LOG(
            Renderer,
            Error,
            "Cannot initialize the render pipeline with an uninitialized shader manager");
        return false;
    }

    // Stage every setup so a failed entry cannot publish a partial pipeline.
    std::vector<DrawSetup> preparedSetups;
    preparedSetups.reserve(entries_.size());

    for (std::size_t index = 0; index < entries_.size(); ++index)
    {
        const PassDescriptor& descriptor = entries_[index].descriptor;

        const auto vertexShader =
            shaders.LoadVertex(descriptor.vertexShader);
        if (!vertexShader)
        {
            GILGAMESH_LOG(
                Renderer,
                Error,
                "Failed to load vertex shader '{}' for render pass '{}' "
                "at index {} (error {}, HRESULT {:#010x})",
                descriptor.vertexShader,
                descriptor.name,
                index,
                static_cast<int>(vertexShader.error),
                static_cast<std::uint32_t>(vertexShader.nativeResult));
            return false;
        }

        std::optional<PixelShaderHandle> pixelShader;
        if (descriptor.pixelShader.has_value())
        {
            const auto loadedPixelShader =
                shaders.LoadPixel(*descriptor.pixelShader);
            if (!loadedPixelShader)
            {
                GILGAMESH_LOG(
                    Renderer,
                    Error,
                    "Failed to load pixel shader '{}' for render pass '{}' "
                    "at index {} (error {}, HRESULT {:#010x})",
                    *descriptor.pixelShader,
                    descriptor.name,
                    index,
                    static_cast<int>(loadedPixelShader.error),
                    static_cast<std::uint32_t>(loadedPixelShader.nativeResult));
                return false;
            }

            pixelShader = loadedPixelShader.resource;
        }

        const DrawSetupDesc drawSetupDesc{
            .vertexShader = vertexShader.resource,
            .pixelShader = pixelShader,
            .inputElements = {},
            .drawState = descriptor.drawState
        };

        DrawSetup drawSetup;
        const HRESULT result = drawSetup.Initialize(
            device,
            shaders,
            drawSetupDesc);
        if (FAILED(result))
        {
            GILGAMESH_LOG(
                Renderer,
                Error,
                "Failed to prepare the draw setup for render pass '{}' "
                "at index {} (HRESULT {:#010x})",
                descriptor.name,
                index,
                static_cast<std::uint32_t>(result));
            return false;
        }

        preparedSetups.push_back(std::move(drawSetup));
    }

    // Commit only after every entry has been prepared successfully.
    for (std::size_t index = 0; index < entries_.size(); ++index)
    {
        entries_[index].drawSetup =
            std::move(preparedSetups[index]);
    }

    initialized_ = true;
    return true;
}

void RenderPipeline::Execute(RenderPassContext& context, const RenderFrameData& frame)
{
    if (!initialized_) return;


    for (auto& entry : entries_)
    {
        // Bind this pass's shaders and fixed graphics state.
        entry.drawSetup.Apply(&context.deviceContext);

        // Let the pass bind dynamic resources and issue its draw calls.
        entry.pass->Execute(context, frame);
    }
}

void RenderPipeline::Reset()
{
    if (!initialized_) return;

    entries_.clear();
    initialized_ = false;
}

bool RenderPipeline::IsInitialized() const noexcept
{
    return initialized_;
}
