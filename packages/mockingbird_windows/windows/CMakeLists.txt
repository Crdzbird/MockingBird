# Builds the Mockingbird engine from the sources in native/ with the reduced,
# static ONNX Runtime linked in, downloaded once per build tree and checked
# against its SHA-256 by native/cmake/mockingbird_onnxruntime.cmake.
cmake_minimum_required(VERSION 3.24)
project(mockingbird_windows LANGUAGES C CXX)

if(CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64")
  set(_ort_target "windows-arm64")
else()
  set(_ort_target "windows-x64")
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/native/cmake/mockingbird_onnxruntime.cmake")
mockingbird_onnxruntime(${_ort_target} _ort_root)

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
  PARENT_SCOPE
)
