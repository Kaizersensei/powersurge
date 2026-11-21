# Production selection of the accepted adapter. Portable relative source layout. Signed: Mech.
if(BSL_AUDIO_BACKEND STREQUAL "SOLOUD")
 set(BSL_ROOT "${CMAKE_SOURCE_DIR}/bsl")
 set(SOLOUD_SOURCE "${BSL_ROOT}/third_party/soloud")
 set(SIGNALSMITH_INCLUDE "${BSL_ROOT}/third_party")
 add_subdirectory("${BSL_ROOT}/foundation/audio" "${CMAKE_BINARY_DIR}/bsl-audio" EXCLUDE_FROM_ALL)
 target_include_directories(${GAME_UNIXNAME} PRIVATE
   "${BSL_ROOT}/foundation/audio" "${CMAKE_SOURCE_DIR}/src/foundation")
 target_link_libraries(${GAME_UNIXNAME} bsl_audio_adapter)
endif()
