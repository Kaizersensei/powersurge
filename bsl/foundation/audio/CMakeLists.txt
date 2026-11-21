cmake_minimum_required(VERSION 3.20)
project(BSLAudioBackend LANGUAGES C CXX)
# Standalone compatibility slice; no Surge/Allegro device is linked. Signed: Mech.
if(NOT DEFINED SOLOUD_SOURCE OR NOT DEFINED BSL_ROOT OR NOT DEFINED SIGNALSMITH_INCLUDE)
 message(FATAL_ERROR "Explicit pinned dependency/project paths required")
endif()
file(GLOB SOLOUD_CORE CONFIGURE_DEPENDS "${SOLOUD_SOURCE}/src/core/*.cpp")
add_library(bsl_soloud STATIC ${SOLOUD_CORE}
 "${SOLOUD_SOURCE}/src/audiosource/wav/soloud_wav.cpp"
 "${SOLOUD_SOURCE}/src/audiosource/wav/soloud_wavstream.cpp"
 "${SOLOUD_SOURCE}/src/audiosource/wav/dr_impl.cpp"
 "${SOLOUD_SOURCE}/src/audiosource/wav/stb_vorbis.c"
 "${SOLOUD_SOURCE}/src/backend/miniaudio/soloud_miniaudio.cpp"
 "${SOLOUD_SOURCE}/src/backend/null/soloud_null.cpp")
target_include_directories(bsl_soloud PUBLIC "${SOLOUD_SOURCE}/include")
target_compile_definitions(bsl_soloud PRIVATE WITH_MINIAUDIO WITH_NULL)
target_compile_options(bsl_soloud PRIVATE -O2)
if(WIN32)
 target_link_libraries(bsl_soloud PUBLIC winmm ole32 uuid)
endif()
add_library(bsl_audio_adapter STATIC soloud_backend.cpp)
target_compile_features(bsl_audio_adapter PUBLIC cxx_std_17)
target_compile_options(bsl_audio_adapter PRIVATE -O2 -Wall -Wextra -Werror)
target_include_directories(bsl_audio_adapter PUBLIC "${BSL_ROOT}/foundation/include" "${SIGNALSMITH_INCLUDE}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(bsl_audio_adapter PUBLIC bsl_soloud)
add_executable(audio_backend_check backend_check.cpp)
target_compile_options(audio_backend_check PRIVATE -O2 -Wall -Wextra -Werror)
target_link_libraries(audio_backend_check PRIVATE bsl_audio_adapter)
