add_library (GilgameshEditor STATIC)

target_sources (GilgameshEditor
  PRIVATE
    EditorProgram.cpp
    EditorProgram.h

    EditorCamera/EditorCamera.cpp
    EditorCamera/EditorCamera.h

    EditorInput/EditorCameraIntent.h
    EditorInput/EditorController.cpp
    EditorInput/EditorController.h

    EditorViewport/EditorViewport.cpp
    EditorViewport/EditorViewport.h
    EditorViewport/EditorViewportClient.cpp
    EditorViewport/EditorViewportClient.h

    EditorUI/Panels/EditorViewportPanel.cpp
    EditorUI/Panels/EditorViewportPanel.h

    EditorUI/Runtime/ImGuiIntegration.cpp
    EditorUI/Runtime/ImGuiIntegration.h

    EditorUI/Workspace/EditorWorkspace.cpp
    EditorUI/Workspace/EditorWorkspace.h
)

if (GILGAMESH_ENABLE_EDITOR_DIAGNOSTICS)
  target_sources (GilgameshEditor
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
      Console/ConsoleCommandOutput.cpp
      Console/ConsoleCommandOutput.h
      Console/ConsoleConfiguration.cpp
      Console/ConsoleConfiguration.h
      Console/ConsoleLogSink.cpp
      Console/ConsoleLogSink.h

      EditorUI/Panels/OutputLogPanel.cpp
      EditorUI/Panels/OutputLogPanel.h
  )
endif()

get_target_property (
  GILGAMESH_EDITOR_SOURCES
  GilgameshEditor
  SOURCES
)

source_group (
  TREE ${CMAKE_CURRENT_SOURCE_DIR}
  FILES ${GILGAMESH_EDITOR_SOURCES}
)

target_include_directories (GilgameshEditor
  PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_compile_features (GilgameshEditor
  PUBLIC
    cxx_std_20
)

target_compile_definitions (GilgameshEditor
  PRIVATE
    GILGAMESH_ENABLE_EDITOR_DIAGNOSTICS=$<BOOL:${GILGAMESH_ENABLE_EDITOR_DIAGNOSTICS}>
)

target_link_libraries (GilgameshEditor
  PUBLIC
    GilgameshEngine
  PRIVATE
    DearImGui
)
