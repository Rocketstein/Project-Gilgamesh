#pragma once

#include "Engine/Runtime/EngineServices.h"
#include "Engine/Platform/Windows/Window.h"
#include "Engine/Render/Renderer/Renderer.h"
#include "Engine/Time/FrameClock.h"

class IProgram;

class Engine {
public:
	bool Initialize();
	int  Run(IProgram& program);
	void Shutdown();

	EngineServices GetServices();

private:
	bool HandlePresentResult(RenderResult result);

private:
	// TODO: Make release order explicit
	Window	 window_;
	Renderer renderer_;
	FrameClock clock_;

	bool isEngineAlive = false;
};