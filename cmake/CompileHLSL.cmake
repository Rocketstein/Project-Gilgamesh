find_program (FXC_EXECUTABLE fxc
  HINTS "$ENV{WindowsSdkVerBinPath}x64"
  DOC "HLSL compiler (fxc.exe) from the Windows SDK")

if (NOT FXC_EXECUTABLE)
  message (FATAL_ERROR "fxc.exe not found. Configure from a Visual Studio developer environment.")
endif()

# 셰이더 소스 위치와 기본 셰이더 모델. 필요하면 include 이후에 덮어쓸 수 있습니다.
set (HLSL_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Shaders")
set (HLSL_SHADER_MODEL "5_0")

# register_shader 에서 대상을 생략했을 때 사용할 타겟. 루트에서 지정합니다.
set (HLSL_SHADER_TARGET "")

# 자동 감지 대상 스테이지 목록: <출력 접미사>|<엔트리 포인트>
set (HLSL_STAGES
  "vs|VSMain"
  "ps|PSMain"
  "gs|GSMain"
  "hs|HSMain"
  "ds|DSMain"
  "cs|CSMain")

# fxc 는 depfile 을 내보내지 않으므로 인클루드 추적이 불가능합니다.
# 셰이더 디렉터리의 .hlsli 전체를 공통 의존성으로 걸어, 하나라도 바뀌면 다시 컴파일합니다.
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.12)
  file (GLOB HLSL_INCLUDE_FILES CONFIGURE_DEPENDS "${HLSL_SHADER_DIR}/*.hlsli")
else()
  file (GLOB HLSL_INCLUDE_FILES "${HLSL_SHADER_DIR}/*.hlsli")
endif()

# add_hlsl_shader(<target> <source.hlsl> <entry> <profile> <output.cso>)
# 단일 스테이지를 컴파일하는 하위 수준 명령입니다. 보통은 register_shader 를 사용하세요.
function (add_hlsl_shader TARGET SOURCE ENTRY PROFILE OUTPUT_NAME)
  set (out_dir  "${CMAKE_CURRENT_BINARY_DIR}/Shaders")
  set (out_file "${out_dir}/${OUTPUT_NAME}")

  add_custom_command (
    OUTPUT "${out_file}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${out_dir}"
    COMMAND "${FXC_EXECUTABLE}" /nologo /T ${PROFILE} /E ${ENTRY} /WX
            /I "${HLSL_SHADER_DIR}"
            "$<$<CONFIG:Debug>:/Zi;/Od>" "$<$<NOT:$<CONFIG:Debug>>:/O3>"
            /Fo "${out_file}" "${SOURCE}"
    MAIN_DEPENDENCY "${SOURCE}"
    DEPENDS ${HLSL_INCLUDE_FILES}
    COMMENT "fxc ${PROFILE}  ${ENTRY} -> ${OUTPUT_NAME}"
    COMMAND_EXPAND_LISTS
    VERBATIM)

  target_sources (${TARGET} PRIVATE "${out_file}")
  set_source_files_properties ("${out_file}" PROPERTIES HEADER_FILE_ONLY TRUE)
endfunction()

# register_shader([<target>] <name> [STAGES vs ps ...] [MODEL 5_0] [SOURCE <path>])
#
# Shaders/<name>.hlsl 을 읽어 존재하는 엔트리 포인트(VSMain, PSMain, ...)마다
# 컴파일 규칙을 만들고 <name>.<접미사>.cso 로 출력합니다.
#   register_shader (Primitive)                -> Primitive.vs.cso, Primitive.ps.cso
#   register_shader (Blur STAGES cs)           -> 자동 감지 대신 스테이지를 직접 지정
#   register_shader (Sky MODEL 5_1)            -> 셰이더 모델만 다르게
function (register_shader)
  cmake_parse_arguments (ARG "" "MODEL;SOURCE" "STAGES" ${ARGN})

  list (LENGTH ARG_UNPARSED_ARGUMENTS positional_count)
  if (positional_count EQUAL 1)
    set (target "${HLSL_SHADER_TARGET}")
    list (GET ARG_UNPARSED_ARGUMENTS 0 name)
  elseif (positional_count EQUAL 2)
    list (GET ARG_UNPARSED_ARGUMENTS 0 target)
    list (GET ARG_UNPARSED_ARGUMENTS 1 name)
  else()
    message (FATAL_ERROR
      "register_shader: 사용법 - register_shader([<target>] <name> [STAGES ...] [MODEL ...] [SOURCE ...])")
  endif()

  if (NOT target)
    message (FATAL_ERROR
      "register_shader(${name}): 타겟이 없습니다. 대상을 직접 넘기거나 HLSL_SHADER_TARGET 를 설정하세요.")
  endif()

  set (source "${ARG_SOURCE}")
  if (NOT source)
    set (source "${HLSL_SHADER_DIR}/${name}.hlsl")
  elseif (NOT IS_ABSOLUTE "${source}")
    set (source "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
  endif()

  if (NOT EXISTS "${source}")
    message (FATAL_ERROR "register_shader(${name}): 셰이더 소스를 찾을 수 없습니다 - ${source}")
  endif()

  set (model "${ARG_MODEL}")
  if (NOT model)
    set (model "${HLSL_SHADER_MODEL}")
  endif()

  # 엔트리 포인트를 추가/삭제하면 다시 구성해서 감지 결과를 갱신합니다.
  set_property (DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")

  file (READ "${source}" source_text)

  set (registered "")
  foreach (stage IN LISTS HLSL_STAGES)
    string (REPLACE "|" ";" stage_parts "${stage}")
    list (GET stage_parts 0 suffix)
    list (GET stage_parts 1 entry)

    if (ARG_STAGES)
      if (NOT suffix IN_LIST ARG_STAGES)
        continue()
      endif()
    elseif (NOT source_text MATCHES "[^A-Za-z0-9_]${entry}[ \t\r\n]*\\(")
      continue()
    endif()

    add_hlsl_shader (${target} "${source}" ${entry} ${suffix}_${model} "${name}.${suffix}.cso")
    list (APPEND registered ${suffix})
  endforeach()

  if (NOT registered)
    message (FATAL_ERROR
      "register_shader(${name}): '${source}' 에서 엔트리 포인트를 찾지 못했습니다 (VSMain, PSMain, CSMain, ...).")
  endif()

  # 셰이더 원본도 타겟에 넣어 IDE 에서 보이게 합니다. 빌드는 위 custom command 가 담당합니다.
  target_sources (${target} PRIVATE "${source}")
  set_source_files_properties ("${source}" PROPERTIES HEADER_FILE_ONLY TRUE)
  source_group ("Shaders" FILES "${source}")

  string (REPLACE ";" " " registered_text "${registered}")
  message (STATUS "register_shader: ${name} [${registered_text}] sm${model}")
endfunction()
