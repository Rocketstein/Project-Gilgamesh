#pragma once

#include "Engine/Runtime/EngineServices.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Platform/Windows/Window.h"
#include "Engine/Platform/Windows/Win32InputBackend.h"
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
	Window			  window_;
	InputSystem		  input_;
	Win32InputBackend inputBackend_;
	Renderer		  renderer_;
	FrameClock		  clock_;

	bool isEngineAlive_ = false;
};
