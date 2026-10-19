#include "EditorViewportPasses.h"

#include "EditorGridPass.h"
#include "Engine/Render/Passes/VertexColorMeshPass.h"
#include "Engine/Render/Passes/ViewportClearPass.h"

std::vector<std::unique_ptr<IRenderPass>> CreateEditorViewportPasses()
{
    // The grid is background color; the later mesh pass supplies scene depth and opaque color.
    std::vector<std::unique_ptr<IRenderPass>> passes;
    passes.push_back(std::make_unique<ViewportClearPass>());
    passes.push_back(std::make_unique<EditorGridPass>());
    passes.push_back(std::make_unique<VertexColorMeshPass>());
    return passes;
}
