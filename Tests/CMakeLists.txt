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

# Give renderer integration tests their own compiled copy of the fixed shaders.
register_shader(GilgameshTests Primitive)

add_subdirectory(Core/Math)
add_subdirectory(Editor/EditorScene)
add_subdirectory(Engine/Input)
add_subdirectory(Engine/Render/Mesh)
add_subdirectory(Engine/Render/Techniques)

add_test(
  NAME Gilgamesh.Tests
  COMMAND GilgameshTests
)

set_target_properties(GilgameshTests PROPERTIES FOLDER "Tests")
