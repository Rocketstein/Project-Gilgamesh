# CMakeList.txt : 최상위 CMake 프로젝트 파일, 전역 구성을 수행하고
# 여기에 하위 프로젝트를 포함합니다.
#
cmake_minimum_required (VERSION 3.12)

# 지원되는 경우 MSVC 컴파일러에 대해 핫 다시 로드 사용하도록 설정합니다.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("Project Gilgamesh")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(
  GILGAMESH_ENABLE_DEVELOPER_TOOLS
  "Build ImGui-based in-process developer tools"
  ON
)

if (GILGAMESH_ENABLE_DEVELOPER_TOOLS)
  # Dear ImGui is vendored from the docking branch. Build only the core
  # sources and the platform/renderer backends used by Gilgamesh tools.
  set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/imgui")

  add_library(DearImGui STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_dx11.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
  )

  target_include_directories(DearImGui
    PUBLIC
      "${IMGUI_DIR}"
      "${IMGUI_DIR}/backends"
  )

  target_link_libraries(DearImGui
    PRIVATE
      d3d11
  )

  set_target_properties(DearImGui PROPERTIES FOLDER "ThirdParty")
endif()

# CMake 모듈 경로를 추가하고 HLSL 컴파일러를 포함합니다.
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include (CompileHLSL)

# Define the editor executable and its process-level composition sources.
add_executable (Gilgamesh WIN32)

target_sources (Gilgamesh
  PRIVATE
    main.cpp
    Application/Application.cpp
    Application/Application.h
    Editor/EditorViewport/EditorViewport.cpp
    Editor/EditorViewport/EditorViewport.h
    Editor/EditorCamera/EditorCamera.cpp
    Editor/EditorCamera/EditorCamera.h
    Editor/EditorProgram.cpp
    Editor/EditorProgram.h
)

get_target_property (GILGAMESH_APPLICATION_SOURCES Gilgamesh SOURCES)
source_group (
  TREE ${CMAKE_CURRENT_SOURCE_DIR}
  FILES ${GILGAMESH_APPLICATION_SOURCES}
)

# UNICODE / NOMINMAX / WIN32_LEAN_AND_MEAN 는 GilgameshEngine 이 PUBLIC 으로
# 전파합니다. Windows.h 를 공개 헤더에서 포함하므로 한 곳에서만 정의합니다.

# 셰이더를 등록합니다. Shaders/<이름>.hlsl 에서 엔트리 포인트를 찾아
# <이름>.vs.cso / <이름>.ps.cso 등을 생성합니다.
set (HLSL_SHADER_TARGET Gilgamesh)

register_shader (Primitive)

# Engine Library
add_subdirectory ("Engine")

target_link_libraries (Gilgamesh
	PRIVATE
		GilgameshEngine
)

if (GILGAMESH_ENABLE_DEVELOPER_TOOLS)
  # In-process diagnostics depend on the engine, while the engine remains
  # independent from Dear ImGui and developer-facing panels.
  add_subdirectory ("DeveloperTools")

  target_link_libraries (Gilgamesh
    PRIVATE
      GilgameshDeveloperTools
      DearImGui
  )
endif()

target_compile_definitions (Gilgamesh
  PRIVATE
    GILGAMESH_BUILD_EDITOR=1
    GILGAMESH_ENABLE_DEVELOPER_TOOLS=$<BOOL:${GILGAMESH_ENABLE_DEVELOPER_TOOLS}>
)

include(CTest)

if (BUILD_TESTING)
  add_subdirectory(Tests)
endif()

