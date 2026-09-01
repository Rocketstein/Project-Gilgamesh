#include "Application.h"
#include "Engine/Core/Logging/Logger.h"
#include "Engine/Runtime/Engine.h"

#if GILGAMESH_BUILD_EDITOR
	#include "Editor/EditorProgram.h"
#else

#endif

bool Application::Initialize()
{
	if (!InitializeEngine()) return false;
	GILGAMESH_LOG(Core, Info, "Engine Initialized Successfully");

	return true;
}

bool Application::InitializeEngine()
{
	return engine_.Initialize();
}

void Application::Shutdown()
{
	if (program_)
	{
		program_->Shutdown();
		program_.reset();
	}

	engine_.Shutdown();
}

int Application::Run()
{
	if (!Initialize())
	{
		Shutdown();
		return 1;
	}

	program_ = CreateProgram();
	EngineServices services = engine_.GetServices();
	if (!program_ ||
		!program_->Initialize(services))
	{
		Shutdown();
		return 1;
	}
#if GILGAMESH_BUILD_EDITOR
	GILGAMESH_LOG(Core, Info, "Editor Initialized Successfully");
#else
	GILGAMESH_LOG(Core, Info, "Game Initialized Successfully");
#endif

	const int result = engine_.Run(*program_);
	Shutdown();

	return result;
}

std::unique_ptr<IProgram> Application::CreateProgram()
{
#if GILGAMESH_BUILD_EDITOR
	return std::make_unique<EditorProgram>();
#else
	return std::make_unique<GameProgram>();
#endif
}