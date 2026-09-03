#include "Engine.h"
#include "FrameContext.h"
#include "Program.h"
#include "RenderContext.h"

#include "Engine/Core/Logging/Logger.h"

namespace {
    // Temporary code. Move this to somewhere else
    std::filesystem::path ExecutableDir()
    {
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        return std::filesystem::path(buf).parent_path();
    }
}

bool Engine::Initialize()
{
	// Guard against double initialization
    if (isEngineAlive_) return false;

    if (!window_.Create()) return false;
    GILGAMESH_LOG(Core, Info, "Engine Window Initialized Successfully");

    // Temporary code. Move this to somewhere else
    RendererDesc rendererDesc{};
    rendererDesc.outputWindow = window_.GetNativeHandle();
    rendererDesc.extent = window_.GetClientExtent();
    rendererDesc.vsync = true;
    rendererDesc.shaderDirectory = ExecutableDir() / L"Shaders";
    if (!renderer_.Initialize(rendererDesc)) return false;
    GILGAMESH_LOG(Core, Info, "Renderer Initialized Successfully");

    isEngineAlive_ = true;
	return true;
}

void Engine::Shutdown()
{
    if (!isEngineAlive_) return;
    isEngineAlive_ = false;
}

int Engine::Run(IProgram& program)
{
    if (!isEngineAlive_) return 1;
    clock_.Reset();

    while (window_.PumpMessages())
    {
        if (window_.IsMinimized())
        {
            WaitMessage();
            clock_.Reset();
            continue;
        }

        if (window_.ConsumePendingResize())
        {
            if (!renderer_.Resize(window_.GetClientExtent()))
                return 1;
        }

        const FrameContext frame = clock_.BeginFrame();

        program.Update(frame);

        if (!renderer_.BeginFrame())
            return 1;

        RenderContext renderContext{
            renderer_,
            window_.GetClientExtent()
        };

        program.Render(renderContext);

        if (!HandlePresentResult(renderer_.EndFrame()))
            return 1;
    }

    return 0;
}

bool Engine::HandlePresentResult(RenderResult result)
{
    switch (result)
    {
    case (RenderResult::Ok): {
        break;
    }
    case (RenderResult::Occluded): {
        while (window_.PumpMessages() && renderer_.IsOccluded())
            Sleep(16);
        clock_.Reset();
        break;
    }
    case (RenderResult::DeviceLost):
    case (RenderResult::Failed):
        return false;
    }

    return true;
}

EngineServices Engine::GetServices() {
    return { window_, renderer_ };
}