// swift-tools-version: 5.9
//
// Swift Package Manager support for the mockingbird_ios Flutter plugin.
//
// Sources/mockingbird_native holds a copy of the engine's sources, which
// scripts/sync_native.sh keeps identical to the repository root. ONNX Runtime
// comes as a binary target: the reduced, static xcframework the ONNX Runtime
// builds workflow publishes, checked against its checksum, which links into
// the plugin and loads the bundled .ort models. The plugin keeps its podspec
// too; the CocoaPods build uses Microsoft's onnxruntime-c pod instead.
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
    name: "mockingbird_ios",
    platforms: [
        .iOS("15.0"),
    ],
    products: [
        .library(name: "mockingbird-ios", targets: ["mockingbird_ios"]),
    ],
    targets: [
        .binaryTarget(
            name: "onnxruntime",
            url: "https://github.com/Crdzbird/MockingBird/releases/download/onnxruntime-1.30.0-mockingbird.1/onnxruntime-mockingbird-1.30.0-apple.xcframework.zip",
            checksum: "ae47f9335def0756ebec5687ac662b5535ea859aca8a82892151bc0216b78d7c"
        ),
        .target(
            name: "mockingbird_native",
            dependencies: ["onnxruntime"],
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
            name: "mockingbird_ios",
            dependencies: ["mockingbird_native"],
            path: "Sources/mockingbird_ios"
        ),
    ],
    cLanguageStandard: .c11,
    cxxLanguageStandard: .cxx20
)
