// swift-tools-version: 5.9
//
// Swift Package Manager support for the auris_ios Flutter plugin.
//
// Auris' C/C++ core is shared by every platform and lives at the repository
// root. SwiftPM requires a target's sources to live inside the package, so
// Sources/auris_native contains SYMLINKS to the real tree (include/, src/,
// third_party/*). SwiftPM follows them, and because they are links rather than
// copies the package can never drift from the source of truth. This directory
// is only ever used on macOS, so symlinks are safe here.
//
// The plugin keeps its podspec: Flutter supports both, and CocoaPods remains
// the path that also wires up ONNX Runtime (see "Neural features" below).
//
// FEATURE PARITY: this package pulls in Microsoft's ONNX Runtime Swift package,
// so the SPM build has the same capabilities as the CocoaPods one — including
// Silero neural VAD and YAMNet sound classification. That dependency sets the
// platform floor (iOS 15 / macOS 14), above what the podspecs require.
import PackageDescription

// Opus ships SIMD variants, a fixed-point tree, tests, and demo programs with
// their own main(). We build the portable float path only — the same trimming
// the podspec's prepare_command does with rm -rf.
// Build-system files that live alongside the sources. SwiftPM treats every file
// under a source directory as input, so anything it cannot compile must be
// named explicitly.
let nonSourceExcludes: [String] = [
    "third_party/libfvad/src/CMakeLists.txt",
    "third_party/libfvad/src/Makefile.am",
    "third_party/opus/celt/meson.build",
    "third_party/opus/silk/meson.build",
    "third_party/opus/src/meson.build",
]

// arm/ holds NEON intrinsics (which we DO build on arm64 — see opusArmSources),
// alongside ARM32 assembly, an Ne10 backend, and the run-time CPU-detection
// shims. Exclude everything except the six NEON files opus' own build selects
// for this architecture.
let opusArmExcludes: [String] = [
    "third_party/opus/celt/arm",
    "third_party/opus/silk/arm",
]

let opusExcludes: [String] = [
    "third_party/opus/celt/x86",
    "third_party/opus/celt/mips",
    "third_party/opus/celt/tests",
    "third_party/opus/celt/dump_modes",
    "third_party/opus/celt/opus_custom_demo.c",
    "third_party/opus/silk/x86",
    "third_party/opus/silk/mips",
    "third_party/opus/silk/fixed",
    "third_party/opus/silk/tests",
    "third_party/opus/silk/float/x86",
    "third_party/opus/src/opus_demo.c",
    "third_party/opus/src/opus_compare.c",
    "third_party/opus/src/repacketizer_demo.c",
    "third_party/opus/src/opus_multistream.c",
    "third_party/opus/src/opus_multistream_decoder.c",
    "third_party/opus/src/opus_multistream_encoder.c",
    "third_party/opus/src/opus_projection_decoder.c",
    "third_party/opus/src/opus_projection_encoder.c",
]

let package = Package(
    name: "auris_macos",
    platforms: [
        // Matches the podspec (osx.deployment_target = 11.0) and the
        // CMake build's CMAKE_OSX_DEPLOYMENT_TARGET.
        .macOS("14.0"),   // raised by the ONNX Runtime Swift package
    ],
    products: [
        .library(name: "auris-macos", targets: ["auris_macos"]),
    ],
    dependencies: [
        // Microsoft's official ONNX Runtime Swift package. Its `onnxruntime`
        // binary target wraps pod-archive-onnxruntime-c — the SAME artifact the
        // podspec's `onnxruntime-c` pod uses — so the C/C++ API is available.
        .package(url: "https://github.com/microsoft/onnxruntime-swift-package-manager",
                 exact: "1.24.2"),
    ],
    // No Flutter dependency is declared here: flutter_tools generates a wrapper
    // package (FlutterGeneratedPluginSwiftPackage) that vends the Flutter
    // framework and depends on this one, exactly as shipping plugins do.
    targets: [
        // The native engine: Auris' C/C++ plus its vendored, permissively
        // licensed dependencies. Compiled into the app so the Dart side can
        // reach the C ABI through DynamicLibrary.process().
        .target(
            name: "auris_native",
            dependencies: [
                .product(name: "onnxruntime", package: "onnxruntime-swift-package-manager"),
            ],
            path: "Sources/auris_native",
            exclude: [
                // Compiled via the Objective-C shim below (miniaudio's Apple
                // backend imports AVFoundation, so its single implementation
                // translation unit must be ObjC).
                "src/capture/miniaudio_impl.c",
            ] + opusExcludes + opusArmExcludes + nonSourceExcludes,
            sources: [
                "src",
                "apple",
                "third_party/libfvad/src",
                "third_party/pffft/pffft.c",
                "third_party/rnnoise/src",
                "third_party/opus/celt",
                "third_party/opus/silk",
                "third_party/opus/src",
            ],
            publicHeadersPath: "include",
            cSettings: [
                .headerSearchPath("src"),
                .headerSearchPath("third_party/miniaudio"),
                .headerSearchPath("third_party/libfvad/include"),
                .headerSearchPath("third_party/libfvad/src"),
                .headerSearchPath("third_party/pffft"),
                .headerSearchPath("third_party/rnnoise/include"),
                // NOTE: third_party/rnnoise/src is deliberately NOT on the
                // include path. RNNoise vendors its own fork of celt (arch.h,
                // pitch.h, kiss_fft.h, celt_lpc.h, _kiss_fft_guts.h) which
                // SHADOWS opus' headers of the same name and silently breaks
                // opus (mismatched celt_inner_prod signatures, missing
                // celt_sig_assert/opus_fft). RNNoise's own .c files include
                // those headers relative to themselves, so it is not needed.
                .headerSearchPath("third_party/opus/include"),
                .headerSearchPath("third_party/opus/celt"),
                .headerSearchPath("third_party/opus/silk"),
                .headerSearchPath("third_party/opus/silk/float"),
                .headerSearchPath("third_party/opus/src"),
                .define("AURIS_BUILDING", to: "1"),
                .define("AURIS_HAVE_OPUS", to: "1"),
                .define("AURIS_HAVE_RNNOISE", to: "1"),
                .define("OPUS_BUILD", to: "1"),
                .define("OPUS_DISABLE_INTRINSICS", to: "1"),
                .define("VAR_ARRAYS", to: "1"),
                .define("HAVE_ALLOCA_H", to: "1"),
                .define("DISABLE_DEBUG_FLOAT", to: "1"),
                .define("HAVE_LRINTF", to: "1"),
                .define("HAVE_LRINT", to: "1"),
                .define("FLOATING_POINT", to: "1"),
            ],
            cxxSettings: [
                .headerSearchPath("src"),
                .headerSearchPath("third_party/miniaudio"),
                .headerSearchPath("third_party/libfvad/include"),
                .headerSearchPath("third_party/pffft"),
                .headerSearchPath("third_party/rnnoise/include"),
                .headerSearchPath("third_party/opus/include"),
                .define("AURIS_BUILDING", to: "1"),
                .define("AURIS_HAVE_OPUS", to: "1"),
                .define("AURIS_HAVE_RNNOISE", to: "1"),
                .define("AURIS_HAVE_NEURAL_VAD", to: "1"),
                .define("AURIS_HAVE_SOUND_CLASS", to: "1"),
            ],
            linkerSettings: [
                .linkedFramework("CoreFoundation"),
                .linkedFramework("CoreAudio"),
                .linkedFramework("AudioToolbox"),
                .linkedFramework("AVFoundation"),
            ]
        ),
        // The Flutter plugin class. Its only job is registration plus the
        // symbol anchor that keeps the C ABI reachable from dart:ffi.
        .target(
            name: "auris_macos",
            dependencies: ["auris_native"],
            path: "Sources/auris_macos"
        ),
    ],
    cLanguageStandard: .c11,
    cxxLanguageStandard: .cxx20
)
