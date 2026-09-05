add_executable(GilgameshCoreMathTests)

target_sources(GilgameshCoreMathTests
  PRIVATE
    Core/Math/MatrixTests.cpp
)

target_link_libraries(GilgameshCoreMathTests
  PRIVATE
    GilgameshEngine
)

add_test(
  NAME Gilgamesh.Core.Math.Matrix
  COMMAND GilgameshCoreMathTests
)
