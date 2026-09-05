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
)

add_subdirectory(Core/Math)
add_subdirectory(Editor/Camera)

add_test(
  NAME Gilgamesh.Tests
  COMMAND GilgameshTests
)

set_target_properties(GilgameshTests PROPERTIES FOLDER "Tests")
