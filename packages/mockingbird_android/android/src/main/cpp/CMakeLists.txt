# Android-side CMake for libmockingbird.so.
#
# Built into the AAR by Gradle's externalNativeBuild. Mirrors the top-level
# libmockingbird target without the examples, the static library or the smoke
# executable. The engine sources are a copy kept in ../native by
# scripts/sync_native.sh; edit them at the repository root, not here.

cmake_minimum_required(VERSION 3.22.1)
project(mockingbird LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(MOCKINGBIRD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../native)

set(MOCKINGBIRD_SOURCES
        ${MOCKINGBIRD_ROOT}/src/api/api_create.cpp
        ${MOCKINGBIRD_ROOT}/src/api/api_defaults.cpp
        ${MOCKINGBIRD_ROOT}/src/api/api_push.cpp
        ${MOCKINGBIRD_ROOT}/src/api/api_queues.cpp
        ${MOCKINGBIRD_ROOT}/src/api/api_run.cpp
        ${MOCKINGBIRD_ROOT}/src/api/api_devices.cpp
        ${MOCKINGBIRD_ROOT}/src/api/api_keywords.cpp
        ${MOCKINGBIRD_ROOT}/src/api/api_stats.cpp
        ${MOCKINGBIRD_ROOT}/src/api/api_tuning.cpp
        ${MOCKINGBIRD_ROOT}/src/config_support.cpp
        ${MOCKINGBIRD_ROOT}/src/config_validation.cpp
        ${MOCKINGBIRD_ROOT}/src/engine_events.cpp
        ${MOCKINGBIRD_ROOT}/src/engine_ingest.cpp
        ${MOCKINGBIRD_ROOT}/src/engine_lifetime.cpp
        ${MOCKINGBIRD_ROOT}/src/packet_pool.cpp
        ${MOCKINGBIRD_ROOT}/src/status.cpp
        ${MOCKINGBIRD_ROOT}/src/worker/detector_bank.cpp
        ${MOCKINGBIRD_ROOT}/src/worker/detector_bank_keywords.cpp
        ${MOCKINGBIRD_ROOT}/src/worker/packet_emitter.cpp
        ${MOCKINGBIRD_ROOT}/src/worker/segmenter.cpp
        ${MOCKINGBIRD_ROOT}/src/worker/segmenter_frame.cpp
        ${MOCKINGBIRD_ROOT}/src/worker/tunables.cpp
        ${MOCKINGBIRD_ROOT}/src/worker/worker.cpp
        ${MOCKINGBIRD_ROOT}/src/worker/worker_frame.cpp
        ${MOCKINGBIRD_ROOT}/src/worker/worker_health.cpp
        ${MOCKINGBIRD_ROOT}/src/worker/worker_thread.cpp
        ${MOCKINGBIRD_ROOT}/src/capture/backend_factory.cpp
        ${MOCKINGBIRD_ROOT}/src/capture/miniaudio_backend.cpp
        ${MOCKINGBIRD_ROOT}/src/capture/miniaudio_devices.cpp
        ${MOCKINGBIRD_ROOT}/src/capture/miniaudio_ids.cpp
        ${MOCKINGBIRD_ROOT}/src/capture/miniaudio_impl.c
        ${MOCKINGBIRD_ROOT}/src/detect/vad_detector.cpp
        ${MOCKINGBIRD_ROOT}/src/detect/neural_vad_detector.cpp
        ${MOCKINGBIRD_ROOT}/src/detect/neural_vad_inference.cpp
        ${MOCKINGBIRD_ROOT}/src/detect/anomaly_detector.cpp
        ${MOCKINGBIRD_ROOT}/src/detect/anomaly_detector_process.cpp
        ${MOCKINGBIRD_ROOT}/src/detect/yamnet_classifier.cpp
        ${MOCKINGBIRD_ROOT}/src/detect/yamnet_classifier_feed.cpp
        ${MOCKINGBIRD_ROOT}/src/detect/yamnet_classifier_inference.cpp
        ${MOCKINGBIRD_ROOT}/src/detect/oww_keyword_spotter.cpp
        ${MOCKINGBIRD_ROOT}/src/detect/oww_keyword_spotter_feed.cpp
        ${MOCKINGBIRD_ROOT}/src/encode/factory.cpp
        ${MOCKINGBIRD_ROOT}/src/encode/pcm_encoder.cpp
        ${MOCKINGBIRD_ROOT}/src/encode/opus_encoder.cpp
        ${MOCKINGBIRD_ROOT}/src/encode/ogg_opus_encoder.cpp
        ${MOCKINGBIRD_ROOT}/src/encode/ogg_page_writer.cpp
        ${MOCKINGBIRD_ROOT}/src/preprocess/rnnoise_preprocessor.cpp
        ${MOCKINGBIRD_ROOT}/src/capture/oboe_backend.cpp)

# Vendored RNNoise (BSD-3, Xiph), classic baked-model revision bad0a75.
set(MOCKINGBIRD_RNNOISE_DIR ${MOCKINGBIRD_ROOT}/third_party/rnnoise)
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
list(APPEND MOCKINGBIRD_SOURCES ${MOCKINGBIRD_RNNOISE_SOURCES})

# Vendored libopus 1.6.1 (third_party/opus/VENDORED_FROM.txt).
set(MOCKINGBIRD_OPUS_DIR ${MOCKINGBIRD_ROOT}/third_party/opus)
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
endif()

# Vendored PFFFT.
set(MOCKINGBIRD_PFFFT_SOURCES ${MOCKINGBIRD_ROOT}/third_party/pffft/pffft.c)

# libfvad sources (BSD-3 / MIT).
set(MOCKINGBIRD_LIBFVAD_SOURCES
        ${MOCKINGBIRD_ROOT}/third_party/libfvad/src/fvad.c
        ${MOCKINGBIRD_ROOT}/third_party/libfvad/src/signal_processing/division_operations.c
        ${MOCKINGBIRD_ROOT}/third_party/libfvad/src/signal_processing/energy.c
        ${MOCKINGBIRD_ROOT}/third_party/libfvad/src/signal_processing/get_scaling_square.c
        ${MOCKINGBIRD_ROOT}/third_party/libfvad/src/signal_processing/resample_48khz.c
        ${MOCKINGBIRD_ROOT}/third_party/libfvad/src/signal_processing/resample_by_2_internal.c
        ${MOCKINGBIRD_ROOT}/third_party/libfvad/src/signal_processing/resample_fractional.c
        ${MOCKINGBIRD_ROOT}/third_party/libfvad/src/signal_processing/spl_inl.c
        ${MOCKINGBIRD_ROOT}/third_party/libfvad/src/vad/vad_core.c
        ${MOCKINGBIRD_ROOT}/third_party/libfvad/src/vad/vad_filterbank.c
        ${MOCKINGBIRD_ROOT}/third_party/libfvad/src/vad/vad_gmm.c
        ${MOCKINGBIRD_ROOT}/third_party/libfvad/src/vad/vad_sp.c)

add_library(mockingbird SHARED ${MOCKINGBIRD_SOURCES} ${MOCKINGBIRD_LIBFVAD_SOURCES} ${MOCKINGBIRD_PFFFT_SOURCES})

target_compile_definitions(mockingbird PRIVATE MOCKINGBIRD_BUILDING MOCKINGBIRD_BUILD_SHARED)
# RNNoise is vendored (no external dep), so it's always available on Android.
target_compile_definitions(mockingbird PRIVATE MOCKINGBIRD_HAVE_RNNOISE=1)

target_include_directories(mockingbird PRIVATE
        ${MOCKINGBIRD_ROOT}/include
        ${MOCKINGBIRD_ROOT}/src)
target_include_directories(mockingbird SYSTEM PRIVATE
        ${MOCKINGBIRD_ROOT}/third_party/miniaudio
        ${MOCKINGBIRD_ROOT}/third_party/libfvad/include
        ${MOCKINGBIRD_ROOT}/third_party/libfvad/src
        ${MOCKINGBIRD_ROOT}/third_party/pffft
        ${MOCKINGBIRD_RNNOISE_DIR}/include
        ${MOCKINGBIRD_RNNOISE_DIR}/src)

# Silence warnings inside the vendored RNNoise TUs.
set_source_files_properties(${MOCKINGBIRD_RNNOISE_SOURCES}
        PROPERTIES COMPILE_FLAGS "-w")

# 16KB-aligned shared libraries for Android 15+.
# Both max-page-size and common-page-size are needed: lld uses
# common-page-size to lay out segments while max-page-size only controls
# what the loader expects. Without common, segment alignment falls back to
# 4KB and Android's ELF check fails.
target_link_options(mockingbird PRIVATE
        "-Wl,-z,max-page-size=16384"
        "-Wl,-z,common-page-size=16384")

# Android system libs miniaudio reaches for.
find_library(log-lib log)
find_library(opensles-lib OpenSLES)
target_link_libraries(mockingbird PRIVATE ${log-lib} ${opensles-lib} android)

if(MOCKINGBIRD_HAVE_OPUS)
    target_compile_definitions(mockingbird PRIVATE MOCKINGBIRD_HAVE_OPUS=1)
    target_link_libraries(mockingbird PRIVATE opus)
endif()

# Oboe via Prefab. gradle's `implementation 'com.google.oboe:oboe:1.9.0'`
# unpacks the AAR and Prefab makes oboe::oboe discoverable via find_package.
find_package(oboe REQUIRED CONFIG)
target_compile_definitions(mockingbird PRIVATE MOCKINGBIRD_HAVE_OBOE=1)
target_link_libraries(mockingbird PRIVATE oboe::oboe)

# ONNX Runtime: Microsoft's onnxruntime-android AAR does NOT ship Prefab
# metadata, so find_package(onnxruntime CONFIG) cannot work. Instead we
# locate the AAR's transformed cache directory via gradle's standard glob,
# import the .so as a SHARED IMPORTED library, and add its bundled headers
# to the include search path.
#
# Gradle caches AARs under
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
    target_compile_definitions(mockingbird PRIVATE MOCKINGBIRD_HAVE_NEURAL_VAD=1)
    target_compile_definitions(mockingbird PRIVATE MOCKINGBIRD_HAVE_SOUND_CLASS=1)
    target_include_directories(mockingbird SYSTEM PRIVATE "${ORT_HEADERS}")
    target_link_libraries(mockingbird PRIVATE onnxruntime_imp)
    message(STATUS "Mockingbird Android: Neural VAD + YAMNet enabled "
                   "(linked ${ORT_LIB})")
else()
    message(WARNING "Mockingbird Android: onnxruntime AAR not found in gradle "
                    "cache. Neural VAD + YAMNet will be unavailable. Run "
                    "a gradle sync first so the AAR is downloaded.")
endif()

# Warnings.
set(MOCKINGBIRD_WARN_COMMON
        -Wall -Wextra -Wpedantic
        -Wshadow -Wconversion -Wsign-conversion
        -Wcast-align -Wunused -Wnull-dereference
        -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough)
set(MOCKINGBIRD_WARN_CXX_ONLY
        -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual)
target_compile_options(mockingbird PRIVATE
        ${MOCKINGBIRD_WARN_COMMON}
        "$<$<COMPILE_LANGUAGE:CXX>:${MOCKINGBIRD_WARN_CXX_ONLY}>")

# Silence warnings inside vendored third-party TUs.
set_source_files_properties(
        ${MOCKINGBIRD_ROOT}/src/capture/miniaudio_impl.c
        PROPERTIES COMPILE_FLAGS "-w")
set_source_files_properties(${MOCKINGBIRD_LIBFVAD_SOURCES}
        PROPERTIES COMPILE_FLAGS "-w")
set_source_files_properties(${MOCKINGBIRD_PFFFT_SOURCES}
        PROPERTIES COMPILE_FLAGS "-w")
