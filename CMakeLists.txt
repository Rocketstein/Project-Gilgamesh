# CMakeList.txt : 최상위 CMake 프로젝트 파일, 전역 구성을 수행하고
# 여기에 하위 프로젝트를 포함합니다.
#
cmake_minimum_required (VERSION 3.10)

# 지원되는 경우 MSVC 컴파일러에 대해 핫 다시 로드 사용하도록 설정합니다.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("Project Gilgamesh")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CMake 모듈 경로를 추가하고 HLSL 컴파일러를 포함합니다.
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include (CompileHLSL)

# 진입점 실행 파일을 정의합니다.
add_executable (Gilgamesh WIN32 "main.cpp")

target_compile_definitions (Gilgamesh PRIVATE UNICODE _UNICODE NOMINMAX
WIN32_LEAN_AND_MEAN)
target_link_libraries (Gilgamesh PRIVATE d3d11 dxgi)

# hlsli will require an extension
add_hlsl_shader (Gilgamesh "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Primitive.hlsl" VSMain vs_5_0 Primitive.vs.cso)
add_hlsl_shader (Gilgamesh "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Primitive.hlsl" PSMain ps_5_0 Primitive.ps.cso)

# 하위 프로젝트를 포함합니다.
# add_subdirectory ("Engine")

# target_link_libraries (Gilgamesh PRIVATE Engine)

