cmake_minimum_required(VERSION 3.24)
project(auris
        VERSION 0.1.0
        DESCRIPTION "Audio capture + VAD + anomaly detection with a C ABI"
        LANGUAGES C CXX)

# ----------------------------------------------------------------------------
# Standards. Clang >= 17 across the board.
# ----------------------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Hide symbols by default; the public C ABI is opt-in via AURIS_API.
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ----------------------------------------------------------------------------
# Platform deployment targets (lowest sensible for v1).
# ----------------------------------------------------------------------------
if(APPLE)
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL "")
        if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
            set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "" FORCE)
        else()
            set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
        endif()
    endif()
endif()

if(ANDROID)
    # // VERIFY: NDK r28+ is required for 16KB-page-aligned shared libraries.
    # Toolchain file should set ANDROID_PLATFORM=android-21 (minSdk) and
    # use a compileSdk >= 35 caller-side to surface the 16KB requirement.
    if(NOT DEFINED ANDROID_PLATFORM)
        set(ANDROID_PLATFORM android-21)
    endif()
    # 16KB page alignment for shared libraries.
    add_link_options("-Wl,-z,max-page-size=16384")
endif()

# ----------------------------------------------------------------------------
# Options.
# ----------------------------------------------------------------------------
option(AURIS_BUILD_SHARED   "Build the shared library (libauris.so/.dylib)" ON)
option(AURIS_ENABLE_NEURAL_VAD "Compile in Silero VAD via ONNX Runtime"   OFF)
option(AURIS_BUILD_STATIC   "Build the static library (libauris.a)"         ON)
option(AURIS_ENABLE_RNNOISE "Compile in RNNoise preprocess (optional dep)"  OFF)
option(AURIS_ENABLE_NEURAL_VAD "Compile in Silero/ONNX neural VAD"          OFF)
option(AURIS_ENABLE_OPUS    "Compile in libopus encoder"                    ON)
option(AURIS_ENABLE_FLAC    "Compile in FLAC archival encoder"              OFF)
option(AURIS_ENABLE_EXAMPLES "Build examples/ targets"                      ON)
option(AURIS_ENABLE_TESTS    "Build tests/ targets"                         OFF)
# Diagnostic logging is OFF by default so release builds are silent. Turn it on
# (-DAURIS_ENABLE_LOG=ON) to trace init / model / backend status to stderr/logcat.
option(AURIS_ENABLE_LOG      "Emit internal diagnostic logging"             OFF)

# ----------------------------------------------------------------------------
# Sources (placeholder; populated as the skeleton lands per build order step 1).
# ----------------------------------------------------------------------------
set(AURIS_PUBLIC_HEADERS
        include/auris/auris.h)

set(AURIS_SOURCES
        src/engine.cpp
        src/packet_pool.cpp
        src/status.cpp
        src/capture/miniaudio_backend.cpp
        src/capture/miniaudio_impl.c
        src/detect/vad_detector.cpp
        src/detect/neural_vad_detector.cpp
        src/detect/anomaly_detector.cpp
        src/detect/sound_classifier.cpp
        src/encode/factory.cpp
        src/encode/pcm_encoder.cpp
        src/encode/opus_encoder.cpp
        src/preprocess/rnnoise_preprocessor.cpp)

# Apple-only native Core Audio backend. The .mm extension makes Apple's
# clang compile this TU as Objective-C++ so AVFoundation / AVAudioSession
# can join in a later step without re-jigging the file list.
if(APPLE)
    list(APPEND AURIS_SOURCES src/capture/coreaudio_backend.mm)
endif()

set(AURIS_INTERNAL_HEADERS
        src/engine.hpp
        src/packet_pool.hpp
        src/spsc_ring.hpp
        src/preroll_ring.hpp
        src/capture/capture_backend.hpp
        src/capture/miniaudio_backend.hpp
        src/detect/detector.hpp
        src/encode/encoder.hpp
        src/preprocess/preprocessor.hpp
        src/preprocess/resampler.hpp)

# Vendored single-header dep. // VERIFY  Pinned to 0.11.25 (downloaded
# 2026-06-08 from github.com/mackron/miniaudio). Public domain / MIT-0.
set(AURIS_MINIAUDIO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio)

# Vendored VAD dep. BSD-3-Clause (WebRTC origin) + MIT (Pirch fork).
# // VERIFY  Pinned to commit 532ab66 of github.com/dpirch/libfvad
# downloaded 2026-06-08.
set(AURIS_LIBFVAD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libfvad)
set(AURIS_LIBFVAD_SOURCES
        ${AURIS_LIBFVAD_DIR}/src/fvad.c
        ${AURIS_LIBFVAD_DIR}/src/signal_processing/division_operations.c
        ${AURIS_LIBFVAD_DIR}/src/signal_processing/energy.c
        ${AURIS_LIBFVAD_DIR}/src/signal_processing/get_scaling_square.c
        ${AURIS_LIBFVAD_DIR}/src/signal_processing/resample_48khz.c
        ${AURIS_LIBFVAD_DIR}/src/signal_processing/resample_by_2_internal.c
        ${AURIS_LIBFVAD_DIR}/src/signal_processing/resample_fractional.c
        ${AURIS_LIBFVAD_DIR}/src/signal_processing/spl_inl.c
        ${AURIS_LIBFVAD_DIR}/src/vad/vad_core.c
        ${AURIS_LIBFVAD_DIR}/src/vad/vad_filterbank.c
        ${AURIS_LIBFVAD_DIR}/src/vad/vad_gmm.c
        ${AURIS_LIBFVAD_DIR}/src/vad/vad_sp.c)
list(APPEND AURIS_SOURCES ${AURIS_LIBFVAD_SOURCES})

# Vendored FFT dep. BSD-like license, FFTPACK-derived.
# // VERIFY  Pommier's original PFFFT, downloaded 2026-06-08 from
# bitbucket.org/jpommier/pffft (master).
set(AURIS_PFFFT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pffft)
set(AURIS_PFFFT_SOURCES ${AURIS_PFFFT_DIR}/pffft.c)
list(APPEND AURIS_SOURCES ${AURIS_PFFFT_SOURCES})

# Vendored RNNoise (recurrent noise suppression). BSD-3-Clause (Xiph).
# // VERIFY  Pinned to commit bad0a75 of github.com/xiph/rnnoise — the last
# classic revision with the model baked into rnn_data.c (no model download).
set(AURIS_RNNOISE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rnnoise)
set(AURIS_RNNOISE_SOURCES
        ${AURIS_RNNOISE_DIR}/src/celt_lpc.c
        ${AURIS_RNNOISE_DIR}/src/denoise.c
        ${AURIS_RNNOISE_DIR}/src/kiss_fft.c
        ${AURIS_RNNOISE_DIR}/src/pitch.c
        ${AURIS_RNNOISE_DIR}/src/rnn.c
        ${AURIS_RNNOISE_DIR}/src/rnn_data.c
        ${AURIS_RNNOISE_DIR}/src/rnn_reader.c)
if(EXISTS ${AURIS_RNNOISE_DIR}/include/rnnoise.h)
    list(APPEND AURIS_SOURCES ${AURIS_RNNOISE_SOURCES})
    set(AURIS_HAVE_RNNOISE ON)
else()
    set(AURIS_HAVE_RNNOISE OFF)
endif()

# ONNX Runtime for the Silero VAD neural detector. v1 looks for a Homebrew
# install on macOS; iOS / Android pods will use their own dep mechanisms in
# follow-up turns. // VERIFY  Set AURIS_ENABLE_NEURAL_VAD=ON to compile in.
if(AURIS_ENABLE_NEURAL_VAD)
    find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
              PATHS /opt/homebrew/opt/onnxruntime/include/onnxruntime
                    /usr/local/opt/onnxruntime/include/onnxruntime
                    /opt/homebrew/include/onnxruntime
                    /usr/local/include/onnxruntime)
    find_library(ONNXRUNTIME_LIBRARY onnxruntime
                 PATHS /opt/homebrew/opt/onnxruntime/lib
                       /usr/local/opt/onnxruntime/lib
                       /opt/homebrew/lib
                       /usr/local/lib)
    if(ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIBRARY)
        set(AURIS_HAVE_NEURAL_VAD ON)
        message(STATUS "Auris: Neural VAD enabled (ONNX Runtime at ${ONNXRUNTIME_LIBRARY})")
    else()
        message(WARNING "Auris: AURIS_ENABLE_NEURAL_VAD=ON but ONNX Runtime "
                        "not found; install with `brew install onnxruntime`. "
                        "Neural VAD will be unavailable.")
    endif()
endif()

# Vendored libopus 1.5.2. BSD. add_subdirectory uses its own CMake build.
# Trim everything we do not need so the link surface stays small and the
# build doesn't pull in opus tests / demos.
set(AURIS_OPUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/opus)
if(EXISTS ${AURIS_OPUS_DIR}/CMakeLists.txt)
    set(OPUS_BUILD_PROGRAMS              OFF CACHE BOOL "" FORCE)
    set(OPUS_BUILD_TESTING               OFF CACHE BOOL "" FORCE)
    set(OPUS_INSTALL_PKG_CONFIG_MODULE   OFF CACHE BOOL "" FORCE)
    set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
    set(OPUS_BUILD_SHARED_LIBRARY        OFF CACHE BOOL "" FORCE)
    set(OPUS_ENABLE_FLOAT_API            ON  CACHE BOOL "" FORCE)
    set(OPUS_FIXED_POINT                 OFF CACHE BOOL "" FORCE)
    add_subdirectory(${AURIS_OPUS_DIR} ${CMAKE_BINARY_DIR}/_opus EXCLUDE_FROM_ALL)
    set(AURIS_HAVE_OPUS ON)
else()
    set(AURIS_HAVE_OPUS OFF)
endif()

# Header-only stub until first sources land; lets the include path validate.
add_library(auris_headers INTERFACE)
target_include_directories(auris_headers INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

# Per-target wiring shared by shared + static libauris.
function(_auris_configure_target tgt)
    target_compile_definitions(${tgt} PRIVATE AURIS_BUILDING)
    target_include_directories(${tgt}
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src)
    # Vendored third-party header: SYSTEM suppresses warnings from inside it
    # while keeping our own code under strict warnings.
    target_include_directories(${tgt} SYSTEM PRIVATE
            ${AURIS_MINIAUDIO_DIR}
            ${AURIS_LIBFVAD_DIR}/include
            ${AURIS_LIBFVAD_DIR}/src
            ${AURIS_PFFFT_DIR})
    if(AURIS_HAVE_RNNOISE)
        target_include_directories(${tgt} SYSTEM PRIVATE
                ${AURIS_RNNOISE_DIR}/include
                ${AURIS_RNNOISE_DIR}/src)
    endif()
    target_link_libraries(${tgt} PUBLIC auris_headers)
    find_package(Threads REQUIRED)
    target_link_libraries(${tgt} PRIVATE Threads::Threads)

    # Platform link surface for miniaudio.
    # // VERIFY  Confirm each framework / lib against the host toolchain when
    # the first device-target build runs.
    if(APPLE)
        target_link_libraries(${tgt} PRIVATE
                "-framework CoreFoundation"
                "-framework CoreAudio"
                "-framework AudioToolbox")
        if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
            target_link_libraries(${tgt} PRIVATE
                    "-framework AVFoundation")
        endif()
    elseif(ANDROID)
        target_link_libraries(${tgt} PRIVATE log OpenSLES)
    elseif(UNIX)
        target_link_libraries(${tgt} PRIVATE pthread dl m)
    endif()

    if(AURIS_HAVE_OPUS)
        target_compile_definitions(${tgt} PRIVATE AURIS_HAVE_OPUS=1)
        target_link_libraries(${tgt} PRIVATE opus)
    endif()

    if(AURIS_HAVE_NEURAL_VAD)
        target_compile_definitions(${tgt} PRIVATE AURIS_HAVE_NEURAL_VAD=1)
        target_include_directories(${tgt} SYSTEM PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
        target_link_libraries(${tgt} PRIVATE ${ONNXRUNTIME_LIBRARY})
        # YAMNet sound classifier reuses the same ORT integration.
        target_compile_definitions(${tgt} PRIVATE AURIS_HAVE_SOUND_CLASS=1)
    endif()

    if(AURIS_HAVE_RNNOISE)
        target_compile_definitions(${tgt} PRIVATE AURIS_HAVE_RNNOISE=1)
    endif()

    if(AURIS_ENABLE_LOG)
        target_compile_definitions(${tgt} PRIVATE AURIS_ENABLE_LOG=1)
    endif()

    # Suppress warnings inside vendored third-party TUs. We do not want to
    # paper over warnings in OUR code, just the third-party sources.
    set_source_files_properties(src/capture/miniaudio_impl.c
            TARGET_DIRECTORY ${tgt}
            PROPERTIES COMPILE_FLAGS "-w")
    set_source_files_properties(${AURIS_LIBFVAD_SOURCES}
            TARGET_DIRECTORY ${tgt}
            PROPERTIES COMPILE_FLAGS "-w")
    set_source_files_properties(${AURIS_PFFFT_SOURCES}
            TARGET_DIRECTORY ${tgt}
            PROPERTIES COMPILE_FLAGS "-w")
    if(AURIS_HAVE_RNNOISE)
        set_source_files_properties(${AURIS_RNNOISE_SOURCES}
                TARGET_DIRECTORY ${tgt}
                PROPERTIES COMPILE_FLAGS "-w")
    endif()
endfunction()

if(AURIS_SOURCES)
    set(_auris_all_sources ${AURIS_SOURCES} ${AURIS_INTERNAL_HEADERS} ${AURIS_PUBLIC_HEADERS})

    if(AURIS_BUILD_SHARED)
        add_library(auris SHARED ${_auris_all_sources})
        target_compile_definitions(auris PRIVATE AURIS_BUILD_SHARED)
        _auris_configure_target(auris)
        set_target_properties(auris PROPERTIES
                VERSION   ${PROJECT_VERSION}
                SOVERSION ${PROJECT_VERSION_MAJOR}
                OUTPUT_NAME auris)
    endif()

    if(AURIS_BUILD_STATIC)
        add_library(auris_static STATIC ${_auris_all_sources})
        _auris_configure_target(auris_static)
        set_target_properties(auris_static PROPERTIES OUTPUT_NAME auris)
    endif()
endif()

# ----------------------------------------------------------------------------
# Warnings (Clang-focused; Auris does not target MSVC in v1).
# ----------------------------------------------------------------------------
# Common warnings for every source. C++-only warnings are guarded so the
# C miniaudio TU does not get spurious "applied to C" notes.
set(AURIS_WARN_COMMON
        -Wall -Wextra -Wpedantic
        -Wshadow -Wconversion -Wsign-conversion
        -Wcast-align -Wunused -Wnull-dereference
        -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough)
set(AURIS_WARN_CXX_ONLY
        -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual)
set(AURIS_WARN_FLAGS
        ${AURIS_WARN_COMMON}
        "$<$<COMPILE_LANGUAGE:CXX>:${AURIS_WARN_CXX_ONLY}>")

if(AURIS_SOURCES)
    foreach(_tgt auris auris_static)
        if(TARGET ${_tgt})
            target_compile_options(${_tgt} PRIVATE ${AURIS_WARN_FLAGS})
        endif()
    endforeach()
endif()

# ----------------------------------------------------------------------------
# Examples (placeholder).
# ----------------------------------------------------------------------------
if(AURIS_ENABLE_EXAMPLES AND TARGET auris)
    add_executable(smoke examples/smoke.cpp)
    target_link_libraries(smoke PRIVATE auris)
    target_compile_options(smoke PRIVATE ${AURIS_WARN_FLAGS})
endif()

# ----------------------------------------------------------------------------
# Tests (placeholder).
# ----------------------------------------------------------------------------
if(AURIS_ENABLE_TESTS AND TARGET auris_static)
    enable_testing()

    # Vendored doctest. // VERIFY  Pinned to v2.4.12 single header
    # (downloaded 2026-06-08 from github.com/doctest/doctest, MIT).
    set(AURIS_DOCTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/doctest)

    add_executable(test_auris
            tests/main.cpp
            tests/test_spsc_ring.cpp
            tests/test_packet_pool.cpp
            tests/test_preroll_ring.cpp
            tests/test_abi_validation.cpp
            tests/test_lifecycle.cpp
            tests/test_update_config.cpp
            tests/test_host_pcm.cpp
            tests/test_opus_encoder.cpp
            tests/test_detectors_golden.cpp
            tests/test_anomaly_reason.cpp
            tests/test_real_voice.cpp
            tests/test_neural_vad.cpp
            tests/test_sound_classifier.cpp
            tests/test_preprocess_rnnoise.cpp
            tests/test_abi_fuzz.cpp
            tests/test_stress.cpp
            tests/test_fault_injection.cpp
            tests/test_backend_faults.cpp)
    target_compile_definitions(test_auris PRIVATE
            AURIS_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    target_link_libraries(test_auris PRIVATE auris_static)
    target_include_directories(test_auris PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_include_directories(test_auris SYSTEM PRIVATE
            ${AURIS_DOCTEST_DIR})
    target_compile_options(test_auris PRIVATE ${AURIS_WARN_FLAGS})

    if(AURIS_HAVE_OPUS)
        target_compile_definitions(test_auris PRIVATE AURIS_HAVE_OPUS=1)
    endif()
    if(AURIS_HAVE_NEURAL_VAD)
        target_compile_definitions(test_auris PRIVATE AURIS_HAVE_NEURAL_VAD=1)
        target_compile_definitions(test_auris PRIVATE AURIS_HAVE_SOUND_CLASS=1)
    endif()
    if(AURIS_HAVE_RNNOISE)
        target_compile_definitions(test_auris PRIVATE AURIS_HAVE_RNNOISE=1)
    endif()

    add_test(NAME auris_unit COMMAND test_auris)
endif()

# Note: main.cpp (CLion boilerplate) is intentionally not referenced.
# It will be replaced by examples/smoke.cpp in step 1.
