find_program (FXC_EXECUTABLE fxc
  HINTS "$ENV{WindowsSdkVerBinPath}x64"
  DOC "HLSL compiler (fxc.exe) from the Windows SDK")

if (NOT FXC_EXECUTABLE)
  message (FATAL_ERROR "fxc.exe not found. Configure from a Visual Studio developer environment.")
endif()

# add_hlsl_shader(<target> <source.hlsl> <entry> <profile> <output.cso>)
function (add_hlsl_shader TARGET SOURCE ENTRY PROFILE OUTPUT_NAME)
  set (out_dir  "${CMAKE_CURRENT_BINARY_DIR}/Shaders")
  set (out_file "${out_dir}/${OUTPUT_NAME}")

  add_custom_command (
    OUTPUT "${out_file}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${out_dir}"
    COMMAND "${FXC_EXECUTABLE}" /nologo /T ${PROFILE} /E ${ENTRY} /WX
            $<IF:$<CONFIG:Debug>,/Zi,/O3> $<$<CONFIG:Debug>:/Od>
            /Fo "${out_file}" "${SOURCE}"
    MAIN_DEPENDENCY "${SOURCE}"
    COMMENT "fxc ${PROFILE}  ${ENTRY} -> ${OUTPUT_NAME}"
    VERBATIM)

  target_sources (${TARGET} PRIVATE "${out_file}")
  set_source_files_properties ("${out_file}" PROPERTIES HEADER_FILE_ONLY TRUE)
endfunction()