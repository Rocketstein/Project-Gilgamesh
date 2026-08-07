# CMakeList.txt : 최상위 CMake 프로젝트 파일, 전역 구성을 수행하고
# 여기에 하위 프로젝트를 포함합니다.
#
cmake_minimum_required (VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_program (FXC_EXECUTABLE fxc
  HINTS "$ENV{WindowsSdkVerBinPath}x64"
  DOC "HLSL compiler (fxc.exe) from the Windows SDK")

if (NOT FXC_EXECUTABLE)
  message (FATAL_ERROR "fxc.exe not found. Configure from a Visual Studio developer environment.")
endif()

# 지원되는 경우 MSVC 컴파일러에 대해 핫 다시 로드 사용하도록 설정합니다.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("Project Gilgamesh")

# 하위 프로젝트를 포함합니다.
# add_subdirectory ("Engine")

# 진입점 실행 파일을 정의합니다.
add_executable (Gilgamesh WIN32 "main.cpp")

target_compile_definitions (Gilgamesh PRIVATE UNICODE _UNICODE NOMINMAX
WIN32_LEAN_AND_MEAN)
target_link_libraries (Gilgamesh PRIVATE d3d11 dxgi d3dcompiler)

# target_link_libraries (Gilgamesh PRIVATE Engine)

set (SHADER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Primitive.hlsl")
set (SHADER_OUT "${CMAKE_CURRENT_BINARY_DIR}/Shaders")

add_custom_command (
  OUTPUT "${SHADER_OUT}/Primitive.vs.cso"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUT}"
  COMMAND "${FXC_EXECUTABLE}" /nologo /T vs_5_0 /E VSMain /WX
          $<IF:$<CONFIG:Debug>,/Zi,/O3> $<$<CONFIG:Debug>:/Od>
          /Fo "${SHADER_OUT}/Primitive.vs.cso" "${SHADER_SRC}"
  MAIN_DEPENDENCY "${SHADER_SRC}"
  COMMENT "fxc vs_5_0  Primitive.hlsl -> Primitive.vs.cso"
  VERBATIM)

add_custom_command (
  OUTPUT "${SHADER_OUT}/Primitive.ps.cso"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUT}"
  COMMAND "${FXC_EXECUTABLE}" /nologo /T ps_5_0 /E PSMain /WX
          $<IF:$<CONFIG:Debug>,/Zi,/O3> $<$<CONFIG:Debug>:/Od>
          /Fo "${SHADER_OUT}/Primitive.ps.cso" "${SHADER_SRC}"
  MAIN_DEPENDENCY "${SHADER_SRC}"
  COMMENT "fxc ps_5_0  Primitive.hlsl -> Primitive.ps.cso"
  VERBATIM)