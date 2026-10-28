# The reduced, static ONNX Runtime the platform packages link, as the ONNX
# Runtime builds workflow publishes it: downloaded once per build tree,
# checked against its SHA-256, and unpacked to <build>/<name>. Setting the
# environment variable MOCKINGBIRD_ORT_DIR to a directory holding unpacked
# archives, as scripts/build_ort_static.sh leaves them, skips the download.
set(MOCKINGBIRD_ORT_RELEASE "onnxruntime-1.30.0-mockingbird.1")

function(mockingbird_onnxruntime target out_root)
  set(_name "onnxruntime-mockingbird-1.30.0-${target}")
  if(EXISTS "$ENV{MOCKINGBIRD_ORT_DIR}/${_name}/include/onnxruntime_cxx_api.h")
    set(${out_root} "$ENV{MOCKINGBIRD_ORT_DIR}/${_name}" PARENT_SCOPE)
    return()
  endif()
  set(_sha_android-arm64-v8a   "c00d84b6c40ea8a5bba5981b6c17a57683dbcd6dfe4527a90b0df7e2750e8e27")
  set(_sha_android-armeabi-v7a "13fe44e8c84574a4f3fe95c64b3b42b25875fc07c8ccb407a8bd59b38510680b")
  set(_sha_android-x86_64      "760812a5d9bd0c5b42b1df361a46d771d407aa02bf0e3846cad503cdab30dc74")
  set(_sha_android-x86         "a887a06bb505a17912dd2cfedcf3b152fad682b0ec4e2556245d4b4b3a639b0f")
  set(_sha_linux-x64           "e598610fc2025773be42686652a4bec73d8fb11003b1f9d4d488ca230b4cc4cb")
  set(_sha_linux-aarch64       "e5c553bd708ac27d2345b19e96aee046706c39bef042ce8214e4589542da70d8")
  set(_sha_windows-x64         "a0b1c77c03cccb97cc0b5581bcca0c3f2f1b77d987b81a81abb31effca0eecba")
  set(_sha_windows-arm64       "414d78060d964c67dafc77db01fd7bd6771c8483e4747e6352449d9201a02db1")
  if(NOT DEFINED _sha_${target})
    message(FATAL_ERROR "Mockingbird: no ONNX Runtime build for ${target}")
  endif()
  set(_ext "tgz")
  if(target MATCHES "^windows")
    set(_ext "zip")
  endif()
  set(_root "${CMAKE_BINARY_DIR}/${_name}")
  if(NOT EXISTS "${_root}/include/onnxruntime_cxx_api.h")
    set(_archive "${CMAKE_BINARY_DIR}/${_name}.${_ext}")
    file(DOWNLOAD
      "https://github.com/Crdzbird/MockingBird/releases/download/${MOCKINGBIRD_ORT_RELEASE}/${_name}.${_ext}"
      "${_archive}"
      EXPECTED_HASH SHA256=${_sha_${target}}
      TLS_VERIFY ON)
    file(ARCHIVE_EXTRACT INPUT "${_archive}" DESTINATION "${CMAKE_BINARY_DIR}")
    file(REMOVE "${_archive}")
  endif()
  set(${out_root} "${_root}" PARENT_SCOPE)
endfunction()
