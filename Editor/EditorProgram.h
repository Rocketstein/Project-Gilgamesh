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
    void DrawPrimitive(
        Renderer& renderer,
        Extent2D renderExtent);

private:
    struct Impl;
	std::unique_ptr<Impl> impl_;
};
