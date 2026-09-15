#pragma once
#include <memory>

#include "Engine/Runtime/Program.h"

class Renderer;
struct RenderView;
struct StaticMeshDrawItem;

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
    bool InitializeCubeResources(Renderer& renderer);
    void DrawStaticMesh(
        Renderer& renderer,
        const RenderView& renderView,
        const StaticMeshDrawItem& drawItem);

private:
    struct Impl;
	std::unique_ptr<Impl> impl_;
};
