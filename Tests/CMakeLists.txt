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
    d3d11
)

add_subdirectory(Core/Math)
add_subdirectory(Engine/Input)
add_subdirectory(Engine/Render/Mesh)

add_test(
  NAME Gilgamesh.Tests
  COMMAND GilgameshTests
)

set_target_properties(GilgameshTests PROPERTIES FOLDER "Tests")
