#pragma once

#include "Engine/Platform/Windows/Window.h"
#include "Engine/Render/Renderer/Renderer.h"

class InputSystem;

struct EngineServices
{
	Window& mainWindow;
	Renderer& renderer;
	const InputSystem& input;
};
