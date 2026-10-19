#pragma once

#include <memory>
#include <vector>

#include "Engine/Render/Passes/IRenderPass.h"

// Selects the editor's background clear, grid, and scene mesh passes in drawing order.
[[nodiscard]]
std::vector<std::unique_ptr<IRenderPass>> CreateEditorViewportPasses();
