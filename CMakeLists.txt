cmake_minimum_required(VERSION 3.24)
project(mockingbird
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

# Hide symbols by default; the public C ABI is opt-in via MOCKINGBIRD_API.
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
option(MOCKINGBIRD_BUILD_SHARED   "Build the shared library (libmockingbird.so/.dylib)" ON)
option(MOCKINGBIRD_ENABLE_NEURAL_VAD "Compile in Silero VAD via ONNX Runtime"   OFF)
option(MOCKINGBIRD_BUILD_STATIC   "Build the static library (libmockingbird.a)"         ON)
option(MOCKINGBIRD_ENABLE_RNNOISE "Compile in RNNoise preprocess (optional dep)"  OFF)
option(MOCKINGBIRD_ENABLE_NEURAL_VAD "Compile in Silero/ONNX neural VAD"          OFF)
option(MOCKINGBIRD_ENABLE_OPUS    "Compile in libopus encoder"                    ON)
option(MOCKINGBIRD_ENABLE_FLAC    "Compile in FLAC archival encoder"              OFF)
option(MOCKINGBIRD_ENABLE_EXAMPLES "Build examples/ targets"                      ON)
option(MOCKINGBIRD_ENABLE_TESTS    "Build tests/ targets"                         OFF)
# Diagnostic logging is OFF by default so release builds are silent. Turn it on
# (-DMOCKINGBIRD_ENABLE_LOG=ON) to trace init / model / backend status to stderr/logcat.
option(MOCKINGBIRD_ENABLE_LOG      "Emit internal diagnostic logging"             OFF)

# ----------------------------------------------------------------------------
# Sources
# ----------------------------------------------------------------------------
set(MOCKINGBIRD_PUBLIC_HEADERS
        include/mockingbird/mockingbird.h)

set(MOCKINGBIRD_SOURCES
        src/api/api_create.cpp
        src/api/api_defaults.cpp
        src/api/api_push.cpp
        src/api/api_queues.cpp
        src/api/api_run.cpp
        src/api/api_stats.cpp
        src/api/api_tuning.cpp
        src/config_support.cpp
        src/config_validation.cpp
        src/engine_events.cpp
        src/engine_ingest.cpp
        src/engine_lifetime.cpp
        src/packet_pool.cpp
        src/status.cpp
        src/worker/detector_bank.cpp
        src/worker/packet_emitter.cpp
        src/worker/segmenter.cpp
        src/worker/segmenter_frame.cpp
        src/worker/tunables.cpp
        src/worker/worker.cpp
        src/worker/worker_frame.cpp
        src/worker/worker_health.cpp
        src/worker/worker_thread.cpp
        src/capture/backend_factory.cpp
        src/capture/miniaudio_backend.cpp
        src/capture/miniaudio_impl.c
        src/detect/vad_detector.cpp
        src/detect/neural_vad_detector.cpp
        src/detect/neural_vad_inference.cpp
        src/detect/anomaly_detector.cpp
        src/detect/anomaly_detector_process.cpp
        src/detect/yamnet_classifier.cpp
        src/detect/yamnet_classifier_feed.cpp
        src/detect/yamnet_classifier_inference.cpp
        src/encode/factory.cpp
        src/encode/pcm_encoder.cpp
        src/encode/opus_encoder.cpp
        src/preprocess/rnnoise_preprocessor.cpp)

if(APPLE)
    list(APPEND MOCKINGBIRD_SOURCES
            src/capture/coreaudio_backend.mm
            src/capture/coreaudio_backend_init.mm
            src/capture/coreaudio_render.mm
            src/capture/coreaudio_unit.mm)
endif()

if(EMSCRIPTEN)
    list(REMOVE_ITEM MOCKINGBIRD_SOURCES
            src/worker/worker_thread.cpp
            src/capture/backend_factory.cpp
            src/capture/miniaudio_backend.cpp
            src/capture/miniaudio_impl.c)
    list(APPEND MOCKINGBIRD_SOURCES
            src/worker/worker_inline.cpp
            src/capture/backend_factory_host_only.cpp
            src/web/web_config.cpp
            src/web/web_event.cpp
            src/web/web_packet.cpp
            src/web/web_push.cpp
            src/web/web_stats.cpp)
    set(MOCKINGBIRD_BUILD_SHARED OFF)
    set(MOCKINGBIRD_BUILD_STATIC OFF)
    set(MOCKINGBIRD_ENABLE_EXAMPLES OFF)
endif()

set(MOCKINGBIRD_INTERNAL_HEADERS
        src/mockingbird_log.hpp
        src/capture/capture_backend.hpp
        src/capture/chunk_timestamp.hpp
        src/capture/coreaudio_backend.hpp
        src/capture/coreaudio_impl.hpp
        src/capture/mach_timebase.hpp
        src/capture/miniaudio_backend.hpp
        src/capture/oboe_backend.hpp
        src/clock.hpp
        src/config_validation.hpp
        src/degrade.hpp
        src/detect/anomaly_detector.hpp
        src/detect/classification_result.hpp
        src/detect/detection_result.hpp
        src/detect/detector.hpp
        src/detect/feed_hints.hpp
        src/detect/model_path.hpp
        src/detect/neural_vad_detector.hpp
        src/detect/ort_env.hpp
        src/detect/ort_headers.hpp
        src/detect/rolling_stats.hpp
        src/detect/sound_classifier.hpp
        src/detect/spectral_metrics.hpp
        src/detect/yamnet_classifier.hpp
        src/encode/encoder.hpp
        src/engine.hpp
        src/engine_events.hpp
        src/engine_ingest.hpp
        src/frame_clock.hpp
        src/packet_pool.hpp
        src/preprocess/decimator.hpp
        src/preprocess/interpolator.hpp
        src/preprocess/lowpass_design.hpp
        src/preprocess/preprocessor.hpp
        src/preprocess/resampler.hpp
        src/preroll_ring.hpp
        src/spsc_ring.hpp
        src/web/web_fields.hpp
        src/worker/detection_pair.hpp
        src/worker/detector_bank.hpp
        src/worker/level.hpp
        src/worker/level_meter.hpp
        src/worker/packet_emitter.hpp
        src/worker/segmenter.hpp
        src/worker/tunables.hpp
        src/worker/worker.hpp
        src/worker/worker_driver.hpp
        src/worker/worker_health.hpp)

# Vendored single-header dep. // VERIFY  Pinned to 0.11.25 (downloaded
# 2026-06-08 from github.com/mackron/miniaudio). Public domain / MIT-0.
set(MOCKINGBIRD_MINIAUDIO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio)

# Vendored VAD dep. BSD-3-Clause (WebRTC origin) + MIT (Pirch fork).
# // VERIFY  Pinned to commit 532ab66 of github.com/dpirch/libfvad
# downloaded 2026-06-08.
set(MOCKINGBIRD_LIBFVAD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libfvad)
set(MOCKINGBIRD_LIBFVAD_SOURCES
        ${MOCKINGBIRD_LIBFVAD_DIR}/src/fvad.c
        ${MOCKINGBIRD_LIBFVAD_DIR}/src/signal_processing/division_operations.c
        ${MOCKINGBIRD_LIBFVAD_DIR}/src/signal_processing/energy.c
        ${MOCKINGBIRD_LIBFVAD_DIR}/src/signal_processing/get_scaling_square.c
        ${MOCKINGBIRD_LIBFVAD_DIR}/src/signal_processing/resample_48khz.c
        ${MOCKINGBIRD_LIBFVAD_DIR}/src/signal_processing/resample_by_2_internal.c
        ${MOCKINGBIRD_LIBFVAD_DIR}/src/signal_processing/resample_fractional.c
        ${MOCKINGBIRD_LIBFVAD_DIR}/src/signal_processing/spl_inl.c
        ${MOCKINGBIRD_LIBFVAD_DIR}/src/vad/vad_core.c
        ${MOCKINGBIRD_LIBFVAD_DIR}/src/vad/vad_filterbank.c
        ${MOCKINGBIRD_LIBFVAD_DIR}/src/vad/vad_gmm.c
        ${MOCKINGBIRD_LIBFVAD_DIR}/src/vad/vad_sp.c)
list(APPEND MOCKINGBIRD_SOURCES ${MOCKINGBIRD_LIBFVAD_SOURCES})

# Vendored FFT dep. BSD-like license, FFTPACK-derived.
# // VERIFY  Pommier's original PFFFT, downloaded 2026-06-08 from
# bitbucket.org/jpommier/pffft (master).
set(MOCKINGBIRD_PFFFT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pffft)
set(MOCKINGBIRD_PFFFT_SOURCES ${MOCKINGBIRD_PFFFT_DIR}/pffft.c)
list(APPEND MOCKINGBIRD_SOURCES ${MOCKINGBIRD_PFFFT_SOURCES})

# Vendored RNNoise (recurrent noise suppression). BSD-3-Clause (Xiph).
# rnnoise 0.2 with the full 0b50c45 model compiled in (see third_party/rnnoise/
# VENDORED_FROM.txt for the size/CPU trade-off and the os_support.h shim).
set(MOCKINGBIRD_RNNOISE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rnnoise)
set(MOCKINGBIRD_RNNOISE_SOURCES
        ${MOCKINGBIRD_RNNOISE_DIR}/src/celt_lpc.c
        ${MOCKINGBIRD_RNNOISE_DIR}/src/denoise.c
        ${MOCKINGBIRD_RNNOISE_DIR}/src/kiss_fft.c
        ${MOCKINGBIRD_RNNOISE_DIR}/src/pitch.c
        ${MOCKINGBIRD_RNNOISE_DIR}/src/rnn.c
        ${MOCKINGBIRD_RNNOISE_DIR}/src/nnet.c
        ${MOCKINGBIRD_RNNOISE_DIR}/src/nnet_default.c
        ${MOCKINGBIRD_RNNOISE_DIR}/src/parse_lpcnet_weights.c
        ${MOCKINGBIRD_RNNOISE_DIR}/src/rnnoise_data.c
        ${MOCKINGBIRD_RNNOISE_DIR}/src/rnnoise_tables.c)
if(EXISTS ${MOCKINGBIRD_RNNOISE_DIR}/include/rnnoise.h)
    list(APPEND MOCKINGBIRD_SOURCES ${MOCKINGBIRD_RNNOISE_SOURCES})
    set(MOCKINGBIRD_HAVE_RNNOISE ON)
else()
    set(MOCKINGBIRD_HAVE_RNNOISE OFF)
endif()

# ONNX Runtime for the Silero VAD neural detector. v1 looks for a Homebrew
# install on macOS; iOS / Android pods will use their own dep mechanisms in
# follow-up turns. // VERIFY  Set MOCKINGBIRD_ENABLE_NEURAL_VAD=ON to compile in.
if(MOCKINGBIRD_ENABLE_NEURAL_VAD)
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
        set(MOCKINGBIRD_HAVE_NEURAL_VAD ON)
        message(STATUS "Mockingbird: Neural VAD enabled (ONNX Runtime at ${ONNXRUNTIME_LIBRARY})")
    else()
        message(WARNING "Mockingbird: MOCKINGBIRD_ENABLE_NEURAL_VAD=ON but ONNX Runtime "
                        "not found; install with `brew install onnxruntime`. "
                        "Neural VAD will be unavailable.")
    endif()
endif()

# Vendored libopus 1.5.2. BSD. add_subdirectory uses its own CMake build.
# Trim everything we do not need so the link surface stays small and the
# build doesn't pull in opus tests / demos.
set(MOCKINGBIRD_OPUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/opus)
if(EXISTS ${MOCKINGBIRD_OPUS_DIR}/CMakeLists.txt)
    set(OPUS_BUILD_PROGRAMS              OFF CACHE BOOL "" FORCE)
    set(OPUS_BUILD_TESTING               OFF CACHE BOOL "" FORCE)
    set(OPUS_INSTALL_PKG_CONFIG_MODULE   OFF CACHE BOOL "" FORCE)
    set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
    set(OPUS_BUILD_SHARED_LIBRARY        OFF CACHE BOOL "" FORCE)
    set(OPUS_ENABLE_FLOAT_API            ON  CACHE BOOL "" FORCE)
    set(OPUS_FIXED_POINT                 OFF CACHE BOOL "" FORCE)
    add_subdirectory(${MOCKINGBIRD_OPUS_DIR} ${CMAKE_BINARY_DIR}/_opus EXCLUDE_FROM_ALL)
    set(MOCKINGBIRD_HAVE_OPUS ON)
else()
    set(MOCKINGBIRD_HAVE_OPUS OFF)
endif()

# Header-only stub until first sources land; lets the include path validate.
add_library(mockingbird_headers INTERFACE)
target_include_directories(mockingbird_headers INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

# Per-target wiring shared by shared + static libmockingbird.
function(_mockingbird_configure_target tgt)
    target_compile_definitions(${tgt} PRIVATE MOCKINGBIRD_BUILDING)
    target_include_directories(${tgt}
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src)
    # Vendored third-party header: SYSTEM suppresses warnings from inside it
    # while keeping our own code under strict warnings.
    target_include_directories(${tgt} SYSTEM PRIVATE
            ${MOCKINGBIRD_MINIAUDIO_DIR}
            ${MOCKINGBIRD_LIBFVAD_DIR}/include
            ${MOCKINGBIRD_LIBFVAD_DIR}/src
            ${MOCKINGBIRD_PFFFT_DIR})
    if(MOCKINGBIRD_HAVE_RNNOISE)
        target_include_directories(${tgt} SYSTEM PRIVATE
                ${MOCKINGBIRD_RNNOISE_DIR}/include
                ${MOCKINGBIRD_RNNOISE_DIR}/src)
    endif()
    target_link_libraries(${tgt} PUBLIC mockingbird_headers)
    if(NOT EMSCRIPTEN)
        find_package(Threads REQUIRED)
        target_link_libraries(${tgt} PRIVATE Threads::Threads)
    endif()

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
    elseif(EMSCRIPTEN)
    elseif(UNIX)
        target_link_libraries(${tgt} PRIVATE pthread dl m)
    endif()

    if(MOCKINGBIRD_HAVE_OPUS)
        target_compile_definitions(${tgt} PRIVATE MOCKINGBIRD_HAVE_OPUS=1)
        target_link_libraries(${tgt} PRIVATE opus)
    endif()

    if(MOCKINGBIRD_HAVE_NEURAL_VAD)
        target_compile_definitions(${tgt} PRIVATE MOCKINGBIRD_HAVE_NEURAL_VAD=1)
        target_include_directories(${tgt} SYSTEM PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
        target_link_libraries(${tgt} PRIVATE ${ONNXRUNTIME_LIBRARY})
        # YAMNet sound classifier reuses the same ORT integration.
        target_compile_definitions(${tgt} PRIVATE MOCKINGBIRD_HAVE_SOUND_CLASS=1)
    endif()

    if(MOCKINGBIRD_HAVE_RNNOISE)
        target_compile_definitions(${tgt} PRIVATE MOCKINGBIRD_HAVE_RNNOISE=1)
    endif()

    if(MOCKINGBIRD_ENABLE_LOG)
        target_compile_definitions(${tgt} PRIVATE MOCKINGBIRD_ENABLE_LOG=1)
    endif()

    # Suppress warnings inside vendored third-party TUs. We do not want to
    # paper over warnings in OUR code, just the third-party sources.
    set_source_files_properties(src/capture/miniaudio_impl.c
            TARGET_DIRECTORY ${tgt}
            PROPERTIES COMPILE_FLAGS "-w")
    set_source_files_properties(${MOCKINGBIRD_LIBFVAD_SOURCES}
            TARGET_DIRECTORY ${tgt}
            PROPERTIES COMPILE_FLAGS "-w")
    set_source_files_properties(${MOCKINGBIRD_PFFFT_SOURCES}
            TARGET_DIRECTORY ${tgt}
            PROPERTIES COMPILE_FLAGS "-w")
    if(MOCKINGBIRD_HAVE_RNNOISE)
        set_source_files_properties(${MOCKINGBIRD_RNNOISE_SOURCES}
                TARGET_DIRECTORY ${tgt}
                PROPERTIES COMPILE_FLAGS "-w")
    endif()
endfunction()

if(MOCKINGBIRD_SOURCES)
    set(_mockingbird_all_sources ${MOCKINGBIRD_SOURCES} ${MOCKINGBIRD_INTERNAL_HEADERS} ${MOCKINGBIRD_PUBLIC_HEADERS})

    if(MOCKINGBIRD_BUILD_SHARED)
        add_library(mockingbird SHARED ${_mockingbird_all_sources})
        target_compile_definitions(mockingbird PRIVATE MOCKINGBIRD_BUILD_SHARED)
        _mockingbird_configure_target(mockingbird)
        set_target_properties(mockingbird PROPERTIES
                VERSION   ${PROJECT_VERSION}
                SOVERSION ${PROJECT_VERSION_MAJOR}
                OUTPUT_NAME mockingbird)
    endif()

    if(MOCKINGBIRD_BUILD_STATIC)
        add_library(mockingbird_static STATIC ${_mockingbird_all_sources})
        _mockingbird_configure_target(mockingbird_static)
        set_target_properties(mockingbird_static PROPERTIES OUTPUT_NAME mockingbird)
    endif()

    if(EMSCRIPTEN)
        add_executable(mockingbird_wasm ${_mockingbird_all_sources})
        _mockingbird_configure_target(mockingbird_wasm)
        set_target_properties(mockingbird_wasm PROPERTIES OUTPUT_NAME mockingbird SUFFIX ".js")
        file(READ ${CMAKE_CURRENT_SOURCE_DIR}/scripts/wasm_exports.txt _mockingbird_wasm_exports)
        string(STRIP "${_mockingbird_wasm_exports}" _mockingbird_wasm_exports)
        target_link_options(mockingbird_wasm PRIVATE
                -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createMockingbirdModule
                -sENVIRONMENT=web,worker,node -sALLOW_MEMORY_GROWTH=1
                -sEXPORTED_FUNCTIONS=${_mockingbird_wasm_exports}
                -sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAP8,HEAP16,HEAPU16,HEAP32,HEAPU32,HEAPF32,HEAPF64,UTF8ToString
                -sINITIAL_MEMORY=32MB -sSTACK_SIZE=1MB)
    endif()
endif()

# ----------------------------------------------------------------------------
# Warnings (Clang-focused; Mockingbird does not target MSVC in v1).
# ----------------------------------------------------------------------------
# Common warnings for every source. C++-only warnings are guarded so the
# C miniaudio TU does not get spurious "applied to C" notes.
set(MOCKINGBIRD_WARN_COMMON
        -Wall -Wextra -Wpedantic
        -Wshadow -Wconversion -Wsign-conversion
        -Wcast-align -Wunused -Wnull-dereference
        -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough)
set(MOCKINGBIRD_WARN_CXX_ONLY
        -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual)
set(MOCKINGBIRD_WARN_FLAGS
        ${MOCKINGBIRD_WARN_COMMON}
        "$<$<COMPILE_LANGUAGE:CXX>:${MOCKINGBIRD_WARN_CXX_ONLY}>")

if(MOCKINGBIRD_SOURCES)
    foreach(_tgt mockingbird mockingbird_static)
        if(TARGET ${_tgt})
            target_compile_options(${_tgt} PRIVATE ${MOCKINGBIRD_WARN_FLAGS})
        endif()
    endforeach()
endif()

# ----------------------------------------------------------------------------
# Examples (placeholder).
# ----------------------------------------------------------------------------
if(MOCKINGBIRD_ENABLE_EXAMPLES AND TARGET mockingbird)
    add_executable(smoke examples/smoke.cpp)
    target_link_libraries(smoke PRIVATE mockingbird)
    target_compile_options(smoke PRIVATE ${MOCKINGBIRD_WARN_FLAGS})
endif()

# ----------------------------------------------------------------------------
# Tests (placeholder).
# ----------------------------------------------------------------------------
if(MOCKINGBIRD_ENABLE_TESTS AND TARGET mockingbird_static)
    enable_testing()

    # Vendored doctest. // VERIFY  Pinned to v2.4.12 single header
    # (downloaded 2026-06-08 from github.com/doctest/doctest, MIT).
    set(MOCKINGBIRD_DOCTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/doctest)

    add_executable(test_mockingbird
            tests/main.cpp
            tests/test_spsc_ring.cpp
            tests/test_packet_pool.cpp
            tests/test_preroll_ring.cpp
            tests/test_abi_validation.cpp
            tests/test_lifecycle.cpp
            tests/test_update_config.cpp
            tests/test_signal_tap.cpp
            tests/test_degrade_policy.cpp
            tests/test_silence_gate.cpp
            tests/test_wake_batching.cpp
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
    target_compile_definitions(test_mockingbird PRIVATE
            MOCKINGBIRD_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    target_link_libraries(test_mockingbird PRIVATE mockingbird_static)
    target_include_directories(test_mockingbird PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_include_directories(test_mockingbird SYSTEM PRIVATE
            ${MOCKINGBIRD_DOCTEST_DIR})
    target_compile_options(test_mockingbird PRIVATE ${MOCKINGBIRD_WARN_FLAGS})

    if(MOCKINGBIRD_HAVE_OPUS)
        target_compile_definitions(test_mockingbird PRIVATE MOCKINGBIRD_HAVE_OPUS=1)
    endif()
    if(MOCKINGBIRD_HAVE_NEURAL_VAD)
        target_compile_definitions(test_mockingbird PRIVATE MOCKINGBIRD_HAVE_NEURAL_VAD=1)
        target_compile_definitions(test_mockingbird PRIVATE MOCKINGBIRD_HAVE_SOUND_CLASS=1)
    endif()
    if(MOCKINGBIRD_HAVE_RNNOISE)
        target_compile_definitions(test_mockingbird PRIVATE MOCKINGBIRD_HAVE_RNNOISE=1)
    endif()

    add_test(NAME mockingbird_unit COMMAND test_mockingbird)
endif()

# ---------------------------------------------------------------------------
# Throughput benchmark (bench/throughput.cpp): CPU cost per second of audio for
# each feature combination. Built with the tests; run ./bench_mockingbird [seconds].
if(MOCKINGBIRD_ENABLE_TESTS AND TARGET mockingbird_static)
    add_executable(bench_mockingbird bench/throughput.cpp)
    target_compile_definitions(bench_mockingbird PRIVATE
            MOCKINGBIRD_BENCH_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    target_link_libraries(bench_mockingbird PRIVATE mockingbird_static)
    target_compile_options(bench_mockingbird PRIVATE ${MOCKINGBIRD_WARN_FLAGS})
    if(MOCKINGBIRD_HAVE_OPUS)
        target_compile_definitions(bench_mockingbird PRIVATE MOCKINGBIRD_HAVE_OPUS=1)
    endif()
    if(MOCKINGBIRD_HAVE_NEURAL_VAD)
        target_compile_definitions(bench_mockingbird PRIVATE MOCKINGBIRD_HAVE_NEURAL_VAD=1 MOCKINGBIRD_HAVE_SOUND_CLASS=1)
    endif()
    if(MOCKINGBIRD_HAVE_RNNOISE)
        target_compile_definitions(bench_mockingbird PRIVATE MOCKINGBIRD_HAVE_RNNOISE=1)
    endif()
endif()

# Note: main.cpp (CLion boilerplate) is intentionally not referenced.
# It will be replaced by examples/smoke.cpp in step 1.
