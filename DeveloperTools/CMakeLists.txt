add_library (GilgameshDeveloperTools STATIC)

target_sources (GilgameshDeveloperTools
	PRIVATE
		Console/Commands/BuiltInCommands.cpp
		Console/Commands/BuiltInCommands.h
		Console/Commands/CommandInvocation.h
		Console/Commands/CommandLineTokenizer.cpp
		Console/Commands/CommandLineTokenizer.h
		Console/Commands/CommandRegistry.cpp
		Console/Commands/CommandRegistry.h
		Console/Commands/CommandTypes.cpp
		Console/Commands/CommandTypes.h
		Console/ConsoleBuffer.cpp
		Console/ConsoleBuffer.h
		Console/ConsoleConfiguration.cpp
		Console/ConsoleConfiguration.h
		Console/ConsoleCommandOutput.cpp
		Console/ConsoleCommandOutput.h
		Console/ConsoleLogSink.cpp
		Console/ConsoleLogSink.h
		Console/OutputLogPanel.cpp
		Console/OutputLogPanel.h

		Runtime/ImGuiIntegration.cpp
		Runtime/ImGuiIntegration.h

		Workspace/DeveloperToolsWorkspace.cpp
		Workspace/DeveloperToolsWorkspace.h
)

get_target_property (
	GILGAMESH_DEVELOPER_TOOLS_SOURCES
	GilgameshDeveloperTools
	SOURCES)

source_group (
	TREE ${CMAKE_CURRENT_SOURCE_DIR}
	FILES ${GILGAMESH_DEVELOPER_TOOLS_SOURCES})

target_include_directories (GilgameshDeveloperTools
	PUBLIC
		${CMAKE_SOURCE_DIR}
)

target_compile_features (GilgameshDeveloperTools
	PUBLIC
		cxx_std_20
)

target_link_libraries (GilgameshDeveloperTools
	PUBLIC
		GilgameshEngine
	PRIVATE
		DearImGui
)
