#pragma once
#include <memory>

#include "Engine/Runtime/Program.h"

class Renderer;
struct Extent2D;

class EditorProgram : public IProgram
{
public:
    EditorProgram();
    ~EditorProgram() override;

    bool Initialize(EngineServices&) override;
    void Update(const FrameContext&) override;
    void Render(RenderContext&) override;
    void Shutdown() override;

private:
    bool InitializePrimitiveTestResources(Renderer& renderer);
#if GILGAMESH_ENABLE_DEVELOPER_TOOLS
    // TODO: Move ImGui dependency to a separate UI directory as UI grows
    void DrawEditorViewportPanel(RenderContext& context);
#endif
    void DrawPrimitive(
        Renderer& renderer,
        Extent2D renderExtent);

private:
    struct Impl;
	std::unique_ptr<Impl> impl_;
};
