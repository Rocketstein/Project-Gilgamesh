#include "EditorProgram.h"

#include "Engine/Render/Pipeline/GraphicsPipeline.h"
#include "Engine/Render/Renderer/Renderer.h"
#include "Engine/Render/VertexTypes/VertexTypes.h"
#include "Engine/Runtime/EngineServices.h"
#include "Engine/Runtime/FrameContext.h"
#include "Engine/Runtime/RenderContext.h"

#if GILGAMESH_ENABLE_DEVELOPER_TOOLS
#include "DeveloperTools/Console/Commands/BuiltInCommands.h"
#include "DeveloperTools/Console/ConsoleBuffer.h"
#include "DeveloperTools/Console/ConsoleCommandOutput.h"
#include "DeveloperTools/Console/ConsoleConfiguration.h"
#include "DeveloperTools/Console/ConsoleLogSink.h"
#include "DeveloperTools/Console/OutputLogPanel.h"
#include "DeveloperTools/Runtime/ImGuiIntegration.h"
#include "DeveloperTools/Workspace/DeveloperToolsWorkspace.h"
#endif

bool EditorProgram::Initialize(EngineServices& serives)
{


	isProgramAlive = true;
	return true;
}

void EditorProgram::Update(const FrameContext& frame)
{

}

void EditorProgram::Render(RenderContext& context)
{

}

void EditorProgram::Shutdown()
{
	if (!isProgramAlive) return;

}