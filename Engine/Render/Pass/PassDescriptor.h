#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "Engine/Render/Pipeline/PipelineDesc.h"

// Declares a pass's logical shader names and fixed pipeline state.
// Registration stores this metadata; GPU resources are prepared later.
struct PassDescriptor
{
    std::string name;
    std::filesystem::path vertexShader;
    std::optional<std::filesystem::path> pixelShader;

    DrawStateDesc drawState;
};
