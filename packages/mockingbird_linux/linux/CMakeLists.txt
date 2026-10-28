# Builds the Mockingbird engine from the sources in native/ and bundles it
# with ONNX Runtime. The ONNX Runtime release is downloaded once per build
# tree and checked against the SHA-256 GitHub publishes for it.
cmake_minimum_required(VERSION 3.24)
project(mockingbird_linux LANGUAGES C CXX)

set(MOCKINGBIRD_ORT_VERSION "1.30.0")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
  set(_ort_arch "aarch64")
  set(_ort_sha256 "e16a27a8ed330bbc698df7330b0cf56e722f354e3bcc92118682c74ef3c3e3da")
else()
  set(_ort_arch "x64")
  set(_ort_sha256 "a5ed5a3cac51fbb2e90da632ae43d19212faaa20e76484e62bcb7c23ddb3b3fd")
endif()
set(_ort_name "onnxruntime-linux-${_ort_arch}-${MOCKINGBIRD_ORT_VERSION}")
set(_ort_root "${CMAKE_CURRENT_BINARY_DIR}/${_ort_name}")

if(NOT EXISTS "${_ort_root}/include/onnxruntime_cxx_api.h")
  set(_ort_archive "${CMAKE_CURRENT_BINARY_DIR}/${_ort_name}.tgz")
  file(DOWNLOAD
    "https://github.com/microsoft/onnxruntime/releases/download/v${MOCKINGBIRD_ORT_VERSION}/${_ort_name}.tgz"
    "${_ort_archive}"
    EXPECTED_HASH SHA256=${_ort_sha256}
    TLS_VERIFY ON)
  file(ARCHIVE_EXTRACT INPUT "${_ort_archive}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
  file(REMOVE "${_ort_archive}")
endif()

set(_ort_bundle "${CMAKE_CURRENT_BINARY_DIR}/ort_bundle/libonnxruntime.so.1")
configure_file("${_ort_root}/lib/libonnxruntime.so.1" "${_ort_bundle}" COPYONLY)

set(ONNXRUNTIME_ROOT "${_ort_root}" CACHE PATH "" FORCE)
set(MOCKINGBIRD_ENABLE_NEURAL_VAD ON CACHE BOOL "" FORCE)
set(MOCKINGBIRD_BUILD_SHARED ON CACHE BOOL "" FORCE)
set(MOCKINGBIRD_BUILD_STATIC OFF CACHE BOOL "" FORCE)
set(MOCKINGBIRD_VERSIONED_SONAME OFF CACHE BOOL "" FORCE)
set(MOCKINGBIRD_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MOCKINGBIRD_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/native" "${CMAKE_CURRENT_BINARY_DIR}/mockingbird")

set(mockingbird_linux_bundled_libraries
  $<TARGET_FILE:mockingbird>
  "${_ort_bundle}"
  PARENT_SCOPE
)
