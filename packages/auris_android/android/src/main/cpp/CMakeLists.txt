# Android-side CMake for libauris.so.
#
# Built into the AAR by gradle's externalNativeBuild. Mirrors the top-level
# libauris CMake target but trims everything not needed on Android (no
# examples, no static lib, no smoke executable). Sources stay at the repo
# root so there's one place to edit; CMake reaches them through AURIS_ROOT.

cmake_minimum_required(VERSION 3.22.1)
project(auris LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Walk up to the repo root. The plugin's android/src/main/cpp/ is 6 levels
# below /Users/.../mockingbird (cpp → main → src → android → auris_android →
# packages → mockingbird).
set(AURIS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../native)

set(AURIS_SOURCES
        ${AURIS_ROOT}/src/api/api_create.cpp
        ${AURIS_ROOT}/src/api/api_defaults.cpp
        ${AURIS_ROOT}/src/api/api_push.cpp
        ${AURIS_ROOT}/src/api/api_queues.cpp
        ${AURIS_ROOT}/src/api/api_run.cpp
        ${AURIS_ROOT}/src/api/api_stats.cpp
        ${AURIS_ROOT}/src/api/api_tuning.cpp
        ${AURIS_ROOT}/src/config_support.cpp
        ${AURIS_ROOT}/src/config_validation.cpp
        ${AURIS_ROOT}/src/engine_events.cpp
        ${AURIS_ROOT}/src/engine_ingest.cpp
        ${AURIS_ROOT}/src/engine_lifetime.cpp
        ${AURIS_ROOT}/src/packet_pool.cpp
        ${AURIS_ROOT}/src/status.cpp
        ${AURIS_ROOT}/src/worker/detector_bank.cpp
        ${AURIS_ROOT}/src/worker/packet_emitter.cpp
        ${AURIS_ROOT}/src/worker/segmenter.cpp
        ${AURIS_ROOT}/src/worker/segmenter_frame.cpp
        ${AURIS_ROOT}/src/worker/tunables.cpp
        ${AURIS_ROOT}/src/worker/worker.cpp
        ${AURIS_ROOT}/src/worker/worker_frame.cpp
        ${AURIS_ROOT}/src/worker/worker_health.cpp
        ${AURIS_ROOT}/src/worker/worker_thread.cpp
        ${AURIS_ROOT}/src/capture/backend_factory.cpp
        ${AURIS_ROOT}/src/capture/miniaudio_backend.cpp
        ${AURIS_ROOT}/src/capture/miniaudio_impl.c
        ${AURIS_ROOT}/src/detect/vad_detector.cpp
        ${AURIS_ROOT}/src/detect/neural_vad_detector.cpp
        ${AURIS_ROOT}/src/detect/neural_vad_inference.cpp
        ${AURIS_ROOT}/src/detect/anomaly_detector.cpp
        ${AURIS_ROOT}/src/detect/anomaly_detector_process.cpp
        ${AURIS_ROOT}/src/detect/yamnet_classifier.cpp
        ${AURIS_ROOT}/src/detect/yamnet_classifier_feed.cpp
        ${AURIS_ROOT}/src/detect/yamnet_classifier_inference.cpp
        ${AURIS_ROOT}/src/encode/factory.cpp
        ${AURIS_ROOT}/src/encode/pcm_encoder.cpp
        ${AURIS_ROOT}/src/encode/opus_encoder.cpp
        ${AURIS_ROOT}/src/preprocess/rnnoise_preprocessor.cpp
        ${AURIS_ROOT}/src/capture/oboe_backend.cpp)

# Vendored RNNoise (BSD-3, Xiph), classic baked-model revision bad0a75.
set(AURIS_RNNOISE_DIR ${AURIS_ROOT}/third_party/rnnoise)
set(AURIS_RNNOISE_SOURCES
        ${AURIS_RNNOISE_DIR}/src/celt_lpc.c
        ${AURIS_RNNOISE_DIR}/src/denoise.c
        ${AURIS_RNNOISE_DIR}/src/kiss_fft.c
        ${AURIS_RNNOISE_DIR}/src/pitch.c
        ${AURIS_RNNOISE_DIR}/src/rnn.c
        ${AURIS_RNNOISE_DIR}/src/nnet.c
        ${AURIS_RNNOISE_DIR}/src/nnet_default.c
        ${AURIS_RNNOISE_DIR}/src/parse_lpcnet_weights.c
        ${AURIS_RNNOISE_DIR}/src/rnnoise_data.c
        ${AURIS_RNNOISE_DIR}/src/rnnoise_tables.c)
list(APPEND AURIS_SOURCES ${AURIS_RNNOISE_SOURCES})

# Vendored libopus 1.6.1 (third_party/opus/VENDORED_FROM.txt).
set(AURIS_OPUS_DIR ${AURIS_ROOT}/third_party/opus)
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
endif()

# Vendored PFFFT.
set(AURIS_PFFFT_SOURCES ${AURIS_ROOT}/third_party/pffft/pffft.c)

# libfvad sources (BSD-3 / MIT).
set(AURIS_LIBFVAD_SOURCES
        ${AURIS_ROOT}/third_party/libfvad/src/fvad.c
        ${AURIS_ROOT}/third_party/libfvad/src/signal_processing/division_operations.c
        ${AURIS_ROOT}/third_party/libfvad/src/signal_processing/energy.c
        ${AURIS_ROOT}/third_party/libfvad/src/signal_processing/get_scaling_square.c
        ${AURIS_ROOT}/third_party/libfvad/src/signal_processing/resample_48khz.c
        ${AURIS_ROOT}/third_party/libfvad/src/signal_processing/resample_by_2_internal.c
        ${AURIS_ROOT}/third_party/libfvad/src/signal_processing/resample_fractional.c
        ${AURIS_ROOT}/third_party/libfvad/src/signal_processing/spl_inl.c
        ${AURIS_ROOT}/third_party/libfvad/src/vad/vad_core.c
        ${AURIS_ROOT}/third_party/libfvad/src/vad/vad_filterbank.c
        ${AURIS_ROOT}/third_party/libfvad/src/vad/vad_gmm.c
        ${AURIS_ROOT}/third_party/libfvad/src/vad/vad_sp.c)

add_library(auris SHARED ${AURIS_SOURCES} ${AURIS_LIBFVAD_SOURCES} ${AURIS_PFFFT_SOURCES})

target_compile_definitions(auris PRIVATE AURIS_BUILDING AURIS_BUILD_SHARED)
# RNNoise is vendored (no external dep), so it's always available on Android.
target_compile_definitions(auris PRIVATE AURIS_HAVE_RNNOISE=1)

target_include_directories(auris PRIVATE
        ${AURIS_ROOT}/include
        ${AURIS_ROOT}/src)
target_include_directories(auris SYSTEM PRIVATE
        ${AURIS_ROOT}/third_party/miniaudio
        ${AURIS_ROOT}/third_party/libfvad/include
        ${AURIS_ROOT}/third_party/libfvad/src
        ${AURIS_ROOT}/third_party/pffft
        ${AURIS_RNNOISE_DIR}/include
        ${AURIS_RNNOISE_DIR}/src)

# Silence warnings inside the vendored RNNoise TUs.
set_source_files_properties(${AURIS_RNNOISE_SOURCES}
        PROPERTIES COMPILE_FLAGS "-w")

# 16KB-aligned shared libraries for Android 15+.
# // VERIFY  Both max-page-size AND common-page-size are needed: lld uses
# common-page-size to lay out segments while max-page-size only controls
# what the loader expects. Without common, segment alignment falls back to
# 4KB and Android's ELF check fails.
target_link_options(auris PRIVATE
        "-Wl,-z,max-page-size=16384"
        "-Wl,-z,common-page-size=16384")

# Android system libs miniaudio reaches for.
find_library(log-lib log)
find_library(opensles-lib OpenSLES)
target_link_libraries(auris PRIVATE ${log-lib} ${opensles-lib} android)

if(AURIS_HAVE_OPUS)
    target_compile_definitions(auris PRIVATE AURIS_HAVE_OPUS=1)
    target_link_libraries(auris PRIVATE opus)
endif()

# Oboe via Prefab. gradle's `implementation 'com.google.oboe:oboe:1.9.0'`
# unpacks the AAR and Prefab makes oboe::oboe discoverable via find_package.
find_package(oboe REQUIRED CONFIG)
target_compile_definitions(auris PRIVATE AURIS_HAVE_OBOE=1)
target_link_libraries(auris PRIVATE oboe::oboe)

# ONNX Runtime — Microsoft's onnxruntime-android AAR does NOT ship Prefab
# metadata, so find_package(onnxruntime CONFIG) cannot work. Instead we
# locate the AAR's transformed cache directory via gradle's standard glob,
# import the .so as a SHARED IMPORTED library, and add its bundled headers
# to the include search path.
#
# // VERIFY  Gradle caches AARs under
#   $HOME/.gradle/caches/<gradle-version>/transforms/<hash>/transformed/
#       jetified-onnxruntime-android-<version>/{headers, jni/<abi>/*.so}
# Resilient to gradle version + hash by globbing both.
file(GLOB ORT_AAR_DIRS
     "$ENV{HOME}/.gradle/caches/*/transforms/*/transformed/jetified-onnxruntime-android-*"
     "$ENV{HOME}/.gradle/caches/transforms-*/*/transformed/jetified-onnxruntime-android-*")
list(SORT ORT_AAR_DIRS)
list(REVERSE ORT_AAR_DIRS)         # latest version first
set(ORT_FOUND FALSE)
foreach(_dir ${ORT_AAR_DIRS})
    if(EXISTS "${_dir}/jni/${ANDROID_ABI}/libonnxruntime.so" AND
       EXISTS "${_dir}/headers/onnxruntime_cxx_api.h")
        set(ORT_LIB     "${_dir}/jni/${ANDROID_ABI}/libonnxruntime.so")
        set(ORT_HEADERS "${_dir}/headers")
        set(ORT_FOUND TRUE)
        break()
    endif()
endforeach()

if(ORT_FOUND)
    add_library(onnxruntime_imp SHARED IMPORTED)
    set_target_properties(onnxruntime_imp PROPERTIES IMPORTED_LOCATION "${ORT_LIB}")
    target_compile_definitions(auris PRIVATE AURIS_HAVE_NEURAL_VAD=1)
    target_compile_definitions(auris PRIVATE AURIS_HAVE_SOUND_CLASS=1)
    target_include_directories(auris SYSTEM PRIVATE "${ORT_HEADERS}")
    target_link_libraries(auris PRIVATE onnxruntime_imp)
    message(STATUS "Auris Android: Neural VAD + YAMNet enabled "
                   "(linked ${ORT_LIB})")
else()
    message(WARNING "Auris Android: onnxruntime AAR not found in gradle "
                    "cache. Neural VAD + YAMNet will be unavailable. Run "
                    "a gradle sync first so the AAR is downloaded.")
endif()

# Warnings.
set(AURIS_WARN_COMMON
        -Wall -Wextra -Wpedantic
        -Wshadow -Wconversion -Wsign-conversion
        -Wcast-align -Wunused -Wnull-dereference
        -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough)
set(AURIS_WARN_CXX_ONLY
        -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual)
target_compile_options(auris PRIVATE
        ${AURIS_WARN_COMMON}
        "$<$<COMPILE_LANGUAGE:CXX>:${AURIS_WARN_CXX_ONLY}>")

# Silence warnings inside vendored third-party TUs.
set_source_files_properties(
        ${AURIS_ROOT}/src/capture/miniaudio_impl.c
        PROPERTIES COMPILE_FLAGS "-w")
set_source_files_properties(${AURIS_LIBFVAD_SOURCES}
        PROPERTIES COMPILE_FLAGS "-w")
set_source_files_properties(${AURIS_PFFFT_SOURCES}
        PROPERTIES COMPILE_FLAGS "-w")
