if(POLICY CMP0207)
 cmake_policy(SET CMP0207 NEW)
endif()
# Test-ready Windows assembly; no Python or source restaging. Signed: President.
if(NOT WIN32)
 message(FATAL_ERROR "This first portable target is Windows-only")
endif()
if(NOT EXISTS "${PS_EXE}")
 message(FATAL_ERROR "Build the engine first")
endif()
file(MAKE_DIRECTORY "${PS_OUTPUT}")
file(COPY_FILE "${PS_EXE}" "${PS_OUTPUT}/powersurge.exe" ONLY_IF_DIFFERENT)
# Engine and demonstration assets must match. Upstream startup content is not this preview.
file(COPY "${PS_SOURCE}/labs/content/" DESTINATION "${PS_OUTPUT}")
file(COPY "${PS_SOURCE}/LICENSE" DESTINATION "${PS_OUTPUT}")
file(COPY "${PS_SOURCE}/labs" DESTINATION "${PS_OUTPUT}")
file(WRITE "${PS_OUTPUT}/powersurge-preview.ini" "[preview]\ndefault_lab=terrain\n")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "objdump")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${PS_OBJDUMP}")
file(GET_RUNTIME_DEPENDENCIES EXECUTABLES "${PS_EXE}"
 DIRECTORIES "${PS_DLLDIR}"
 PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
 RESOLVED_DEPENDENCIES_VAR deps UNRESOLVED_DEPENDENCIES_VAR missing)
if(missing)
 message(FATAL_ERROR "Unresolved runtime dependencies: ${missing}")
endif()
foreach(dep IN LISTS deps)
 file(TO_CMAKE_PATH "${dep}" normalized)
 string(TOLOWER "${normalized}" lower)
 if(NOT lower MATCHES "/windows/")
  file(COPY "${dep}" DESTINATION "${PS_OUTPUT}")
 endif()
endforeach()
file(COPY "${PS_SOURCE}/labs/content/licenses/" DESTINATION "${PS_OUTPUT}/licenses")
file(COPY_FILE "${PS_SOURCE}/licenses/runtime-dependency-packages.json" "${PS_OUTPUT}/dependency-packages.json" ONLY_IF_DIFFERENT)
file(COPY "${PS_SOURCE}/docs/LAB_RUNS.md" DESTINATION "${PS_OUTPUT}")
file(SHA256 "${PS_OUTPUT}/powersurge.exe" digest)
set(info [=[{
  "engine":"PowerSurge",
  "status":"development-test-build",
  "binary_sha256":"@digest@",
  "signed":"President"
}
]=])
string(CONFIGURE "${info}" info @ONLY)
file(WRITE "${PS_OUTPUT}/build-info.json" "${info}")
message(STATUS "Portable test build: ${PS_OUTPUT}")
