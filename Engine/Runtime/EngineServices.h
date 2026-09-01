#pragma once

#include "Engine/Platform/Windows/Window.h"
#include "Engine/Render/Renderer/Renderer.h"

struct EngineServices
{
	Window& mainWindow;
	Renderer& renderer;
};