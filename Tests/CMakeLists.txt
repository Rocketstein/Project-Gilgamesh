add_executable(GilgameshTests)

target_sources(GilgameshTests
  PRIVATE
    TestFramework.cpp
    TestFramework.h
    TestMain.cpp
)

target_link_libraries(GilgameshTests
  PRIVATE
    GilgameshEngine
    GilgameshEditor
    d3d11
)

# Give renderer integration tests their own compiled copy of the primitive shader.
register_shader(GilgameshTests Primitive)
register_shader(GilgameshTests EditorGrid
  SOURCE "${CMAKE_SOURCE_DIR}/Shaders/Editor/EditorGrid.hlsl")

add_subdirectory(Core/Math)
add_subdirectory(Engine/ECS)
add_subdirectory(Engine/Input)
add_subdirectory(Engine/Render/Mesh)
add_subdirectory(Engine/Render/RenderGraph)
add_subdirectory(Engine/Render/Shader)
add_subdirectory(Engine/Render/DrawSetup)
add_subdirectory(Engine/Render/Passes)
add_subdirectory(Editor/Render)

add_test(
  NAME Gilgamesh.Tests
  COMMAND GilgameshTests
)

set_target_properties(GilgameshTests PROPERTIES FOLDER "Tests")
