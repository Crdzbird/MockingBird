# Builds the Mockingbird engine from the sources in native/ and bundles it
# with ONNX Runtime. The ONNX Runtime release is downloaded once per build
# tree and checked against the SHA-256 GitHub publishes for it.
cmake_minimum_required(VERSION 3.24)
project(mockingbird_windows LANGUAGES C CXX)

set(MOCKINGBIRD_ORT_VERSION "1.30.0")
if(CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64")
  set(_ort_arch "arm64")
  set(_ort_sha256 "e53db8a50b23ae35be901cc93428baf997dc8d420333b097b2eae53d3ea9f2d3")
else()
  set(_ort_arch "x64")
  set(_ort_sha256 "c6ba983baf5681af108599675d2a89c2d145512d02de28aed0bff177cd0ba949")
endif()
set(_ort_name "onnxruntime-win-${_ort_arch}-${MOCKINGBIRD_ORT_VERSION}")
set(_ort_root "${CMAKE_CURRENT_BINARY_DIR}/${_ort_name}")

if(NOT EXISTS "${_ort_root}/include/onnxruntime_cxx_api.h")
  set(_ort_archive "${CMAKE_CURRENT_BINARY_DIR}/${_ort_name}.zip")
  file(DOWNLOAD
    "https://github.com/microsoft/onnxruntime/releases/download/v${MOCKINGBIRD_ORT_VERSION}/${_ort_name}.zip"
    "${_ort_archive}"
    EXPECTED_HASH SHA256=${_ort_sha256}
    TLS_VERIFY ON)
  file(ARCHIVE_EXTRACT INPUT "${_ort_archive}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}"
    PATTERNS "${_ort_name}/include/*" "${_ort_name}/lib/onnxruntime.dll"
             "${_ort_name}/lib/onnxruntime.lib" "${_ort_name}/LICENSE")
  file(REMOVE "${_ort_archive}")
endif()

set(ONNXRUNTIME_ROOT "${_ort_root}" CACHE PATH "" FORCE)
set(MOCKINGBIRD_ENABLE_NEURAL_VAD ON CACHE BOOL "" FORCE)
set(MOCKINGBIRD_BUILD_SHARED ON CACHE BOOL "" FORCE)
set(MOCKINGBIRD_BUILD_STATIC OFF CACHE BOOL "" FORCE)
set(MOCKINGBIRD_VERSIONED_SONAME OFF CACHE BOOL "" FORCE)
set(MOCKINGBIRD_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MOCKINGBIRD_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/native" "${CMAKE_CURRENT_BINARY_DIR}/mockingbird")

set(mockingbird_windows_bundled_libraries
  $<TARGET_FILE:mockingbird>
  "${_ort_root}/lib/onnxruntime.dll"
  PARENT_SCOPE
)
