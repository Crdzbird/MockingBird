// swift-tools-version: 5.9
//
// Swift Package Manager support for the mockingbird_ios Flutter plugin.
//
// Mockingbird' C/C++ core is shared by every platform and lives at the repository
// root. SwiftPM requires a target's sources to live inside the package, so
// Sources/mockingbird_native contains SYMLINKS to the real tree (include/, src/,
// third_party/*). SwiftPM follows them, and because they are links rather than
// copies the package can never drift from the source of truth. This directory
// is only ever used on macOS, so symlinks are safe here.
//
// The plugin keeps its podspec: Flutter supports both, and CocoaPods remains
// the path that also wires up ONNX Runtime (see "Neural features" below).
//
// FEATURE PARITY: this package pulls in Microsoft's ONNX Runtime Swift package,
// so the SPM build has the same capabilities as the CocoaPods one,
// including Silero neural VAD and YAMNet sound classification. That dependency sets the
// platform floor (iOS 15 / macOS 14), above what the podspecs require.
import PackageDescription

let nonSourceExcludes: [String] = [
    "third_party/libfvad/src/CMakeLists.txt",
    "third_party/libfvad/src/Makefile.am",
    "third_party/opus/celt/meson.build",
    "third_party/opus/silk/meson.build",
    "third_party/opus/src/meson.build",
]

let opusArmExcludes: [String] = [
    "third_party/opus/celt/arm",
    "third_party/opus/silk/arm",
]

let opusExcludes: [String] = [
    "third_party/opus/celt/x86",
    "third_party/opus/celt/mips",
    "third_party/opus/celt/tests",
    "third_party/opus/celt/opus_custom_demo.c",
    "third_party/opus/celt/mini_kfft.c",
    "third_party/opus/silk/x86",
    "third_party/opus/silk/mips",
    "third_party/opus/silk/fixed",
    "third_party/opus/silk/tests",
    "third_party/opus/silk/float/x86",
    "third_party/opus/src/opus_demo.c",
    "third_party/opus/src/opus_compare.c",
    "third_party/opus/src/repacketizer_demo.c",
    "third_party/opus/src/qext_compare.c",
    "third_party/opus/src/opus_multistream.c",
    "third_party/opus/src/opus_multistream_decoder.c",
    "third_party/opus/src/opus_multistream_encoder.c",
    "third_party/opus/src/opus_projection_decoder.c",
    "third_party/opus/src/opus_projection_encoder.c",
]

let package = Package(
    name: "mockingbird_macos",
    platforms: [
        .macOS("14.0"),
    ],
    products: [
        .library(name: "mockingbird-macos", targets: ["mockingbird_macos"]),
    ],
    dependencies: [
        .package(url: "https://github.com/microsoft/onnxruntime-swift-package-manager",
                 exact: "1.24.2"),
    ],
    targets: [
        .target(
            name: "mockingbird_native",
            dependencies: [
                .product(name: "onnxruntime", package: "onnxruntime-swift-package-manager"),
            ],
            path: "Sources/mockingbird_native",
            exclude: [
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
                .headerSearchPath("third_party/opus/include"),
                .headerSearchPath("third_party/opus/celt"),
                .headerSearchPath("third_party/opus/silk"),
                .headerSearchPath("third_party/opus/silk/float"),
                .headerSearchPath("third_party/opus/src"),
                .define("MOCKINGBIRD_BUILDING", to: "1"),
                .define("MOCKINGBIRD_HAVE_OPUS", to: "1"),
                .define("MOCKINGBIRD_HAVE_RNNOISE", to: "1"),
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
                .define("MOCKINGBIRD_BUILDING", to: "1"),
                .define("MOCKINGBIRD_HAVE_OPUS", to: "1"),
                .define("MOCKINGBIRD_HAVE_RNNOISE", to: "1"),
                .define("MOCKINGBIRD_HAVE_NEURAL_VAD", to: "1"),
                .define("MOCKINGBIRD_HAVE_SOUND_CLASS", to: "1"),
            ],
            linkerSettings: [
                .linkedFramework("CoreFoundation"),
                .linkedFramework("CoreAudio"),
                .linkedFramework("AudioToolbox"),
                .linkedFramework("AVFoundation"),
            ]
        ),
        .target(
            name: "mockingbird_macos",
            dependencies: ["mockingbird_native"],
            path: "Sources/mockingbird_macos"
        ),
    ],
    cLanguageStandard: .c11,
    cxxLanguageStandard: .cxx20
)
