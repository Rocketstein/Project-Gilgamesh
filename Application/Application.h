#pragma once

#include <memory>
#include "Engine/Platform/Windows/Window.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/Program.h"

// Prototype entry point for Gilgamesh Engine
class Application
{
public:
	bool Initialize();
	void Shutdown();
	int Run();

private:
	bool InitializeEngine();
	std::unique_ptr<IProgram> CreateProgram();

private:
	Engine engine_;
	std::unique_ptr<IProgram> program_;
};