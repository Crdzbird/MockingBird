// ONNX Runtime's C++ header lives at a different path per integration:
// Homebrew and the Android AAR, the CocoaPods pod, and the Swift package's
// xcframework. The first match wins.
#pragma once

#if __has_include(<onnxruntime_cxx_api.h>)
#include <onnxruntime_cxx_api.h>
#elif __has_include(<onnxruntime-c/onnxruntime_cxx_api.h>)
#include <onnxruntime-c/onnxruntime_cxx_api.h>
#elif __has_include(<onnxruntime/onnxruntime_cxx_api.h>)
#include <onnxruntime/onnxruntime_cxx_api.h>
#else
#error "ONNX Runtime header not found; check the pod, package or find_package wiring"
#endif
