#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "Engine/Render/DrawSetup/DrawSetupDesc.h"

// Declares a pass's logical shader names and fixed draw state.
// Registration stores this metadata; GPU resources are prepared later.
struct PassDescriptor
{
    std::string name;
    std::filesystem::path vertexShader;
    std::optional<std::filesystem::path> pixelShader;

    DrawStateDesc drawState;
};
