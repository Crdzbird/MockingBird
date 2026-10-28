#
# mockingbird_ios podspec. Builds libmockingbird's C / C++ sources into the iOS app's
# static archive so the Dart side can reach the symbols via
# DynamicLibrary.process().
#
# The C / C++ sources live at the repo root (../../../{include,src,third_party})
# relative to this podspec. CocoaPods does not love "../" globs, so a
# prepare_command mirrors the tree into Sources/ at pod install time. This is
# self-contained and survives `pod install --clean-install`.
#
Pod::Spec.new do |s|
  s.name             = 'mockingbird_ios'
  s.version          = '0.1.0'
  s.summary          = 'iOS bridge for the Mockingbird audio engine.'
  s.description      = <<-DESC
  iOS implementation of the mockingbird plugin. Compiles libmockingbird's C ABI core
  (capture + detection + packetization) inline so the host app can call the
  C symbols directly via dart:ffi.
                       DESC
  s.homepage         = 'https://github.com/mockingbird'
  s.license          = { :type => 'MIT', :file => '../LICENSE' }
  s.author           = { 'Mockingbird' => 'noreply@example.com' }
  s.source           = { :path => '.' }

  # Mirror the repo's C / C++ tree into Sources/ before CocoaPods scans
  # source_files. Idempotent; safe to rerun. Using cp -R instead of symlinks
  # because Xcode's build phases sometimes resolve symlinks unpredictably.
  s.prepare_command = <<-CMD
    set -euo pipefail
    rm -rf Sources
    mkdir -p Sources/include Sources/src Sources/third_party
    cp -R mockingbird_ios/Sources/mockingbird_native/include/mockingbird        Sources/include/
    cp -R mockingbird_ios/Sources/mockingbird_native/src/                  Sources/src/mockingbird/
    cp -R mockingbird_ios/Sources/mockingbird_native/third_party/miniaudio Sources/third_party/
    cp -R mockingbird_ios/Sources/mockingbird_native/third_party/libfvad   Sources/third_party/
    cp -R mockingbird_ios/Sources/mockingbird_native/third_party/pffft     Sources/third_party/
    cp -R mockingbird_ios/Sources/mockingbird_native/third_party/rnnoise   Sources/third_party/
    cp -R mockingbird_ios/Sources/mockingbird_native/third_party/opus      Sources/third_party/
    # Trim opus SIMD / fixed-point / DNN subdirs we explicitly do not use
    # so CocoaPods does not try to compile them.
    rm -rf Sources/third_party/opus/celt/{x86,arm,tests,dump_modes}
    rm -rf Sources/third_party/opus/silk/{x86,arm,mips,fixed,tests}
    rm -rf Sources/third_party/opus/silk/float/x86
    rm -rf Sources/third_party/opus/dnn
    # Drop opus demo / projection / multistream sources we do not link.
    rm -f Sources/third_party/opus/src/{opus_demo,opus_compare,repacketizer_demo,qext_compare}.c
    rm -f Sources/third_party/opus/celt/mini_kfft.c   # 1.6: test-only FFT
    rm -f Sources/third_party/opus/src/opus_multistream*.c
    rm -f Sources/third_party/opus/src/opus_projection*.c
    # celt/opus_custom_demo.c carries its own main() and references the
    # custom-mode API we don't compile (CUSTOM_MODES off); drop it so
    # force_load doesn't drag in unresolved opus_custom_* symbols.
    rm -f Sources/third_party/opus/celt/opus_custom_demo.c
    # On iOS, miniaudio imports AVFoundation (Objective-C). Compile the
    # implementation TU as Objective-C by giving it a .m suffix. The
    # standalone CMake build keeps the .c name on platforms that do not
    # require ObjC.
    mv Sources/src/mockingbird/capture/miniaudio_impl.c \
       Sources/src/mockingbird/capture/miniaudio_impl.m
  CMD

  s.source_files = [
    'mockingbird_ios/Sources/mockingbird_ios/**/*.swift',
    'Sources/include/mockingbird/*.h',
    'Sources/src/mockingbird/**/*.{c,cpp,m,mm,h,hpp}',
    'Sources/third_party/miniaudio/miniaudio.h',
    'Sources/third_party/libfvad/include/*.h',
    'Sources/third_party/libfvad/src/**/*.{c,h}',
    'Sources/third_party/pffft/*.{c,h}',
    'Sources/third_party/rnnoise/include/*.h',
    'Sources/third_party/rnnoise/src/*.{c,h}',
    'Sources/third_party/opus/include/*.h',
    'Sources/third_party/opus/celt/*.{c,h}',
    'Sources/third_party/opus/silk/*.{c,h}',
    'Sources/third_party/opus/silk/float/*.{c,h}',
    'Sources/third_party/opus/src/*.{c,h}',
  ]
  s.public_header_files = 'Sources/include/mockingbird/*.h'

  s.dependency 'Flutter'
  # ONNX Runtime for Silero VAD and YAMNet, from Microsoft's prebuilt iOS
  # framework.
  s.dependency 'onnxruntime-c', '~> 1.30'
  # std::jthread, std::stop_token and std::atomic::wait need iOS 14, and
  # Xcode 27's SDK refuses deployment targets below 15.0, which is also the
  # ONNX Runtime Swift package's floor, so SPM and CocoaPods agree.
  s.platform     = :ios, '15.0'
  s.swift_version = '5.0'

  s.frameworks = 'CoreFoundation', 'CoreAudio', 'AudioToolbox', 'AVFoundation'

  s.pod_target_xcconfig = {
    'DEFINES_MODULE'              => 'YES',
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++20',
    'CLANG_CXX_LIBRARY'           => 'libc++',
    'GCC_C_LANGUAGE_STANDARD'     => 'c11',
    'GCC_PREPROCESSOR_DEFINITIONS' =>
        'MOCKINGBIRD_BUILDING=1 '\
        'MOCKINGBIRD_HAVE_OPUS=1 '\
        'MOCKINGBIRD_HAVE_NEURAL_VAD=1 '\
        'MOCKINGBIRD_HAVE_SOUND_CLASS=1 '\
        'MOCKINGBIRD_HAVE_RNNOISE=1 '\
        'OPUS_BUILD=1 '\
        'HAVE_LRINTF=1 '\
        'HAVE_LRINT=1 '\
        'USE_ALLOCA=1 '\
        'OPUS_DISABLE_INTRINSICS=1 '\
        'FLOATING_POINT=1 ',
    # Hidden visibility by default; MOCKINGBIRD_API on each exported function
    # opts symbols back to default visibility for the linker.
    'OTHER_CFLAGS'         => '-fvisibility=hidden',
    'OTHER_CPLUSPLUSFLAGS' => '-fvisibility=hidden -fvisibility-inlines-hidden',
    'HEADER_SEARCH_PATHS' => [
      '"$(PODS_TARGET_SRCROOT)/Sources/include"',
      '"$(PODS_TARGET_SRCROOT)/Sources/third_party/miniaudio"',
      '"$(PODS_TARGET_SRCROOT)/Sources/third_party/libfvad/include"',
      '"$(PODS_TARGET_SRCROOT)/Sources/third_party/libfvad/src"',
      '"$(PODS_TARGET_SRCROOT)/Sources/third_party/pffft"',
      '"$(PODS_TARGET_SRCROOT)/Sources/third_party/rnnoise/include"',
      # NOTE: rnnoise/src is deliberately NOT here. RNNoise vendors a fork of
      # celt (arch.h, pitch.h, kiss_fft.h, celt_lpc.h) that SHADOWS opus'
      # headers of the same name and silently miscompiles opus. RNNoise's own
      # sources include them relative to themselves, so the path is not needed.
      '"$(PODS_TARGET_SRCROOT)/Sources/third_party/opus/include"',
      '"$(PODS_TARGET_SRCROOT)/Sources/third_party/opus/celt"',
      '"$(PODS_TARGET_SRCROOT)/Sources/third_party/opus/silk"',
      '"$(PODS_TARGET_SRCROOT)/Sources/third_party/opus/silk/float"',
      '"$(PODS_TARGET_SRCROOT)/Sources/third_party/opus/src"',
      '"$(PODS_TARGET_SRCROOT)/Sources/src/mockingbird"',
    ].join(' '),
    # Keep the public C symbols reachable from DynamicLibrary.process().
    # Without this the linker may discard "unreferenced" static-library
    # symbols when the app links the framework.
    'OTHER_LDFLAGS' => '-Wl,-undefined,dynamic_lookup',
  }

  # dart:ffi resolves the C ABI through DynamicLibrary.process(), which searches
  # the HOST EXECUTABLE's dynamic symbol table. libmockingbird is linked statically
  # into that executable, so its (default-visibility) C symbols must be exported
  # into the dynamic table or the lookup fails with
  # "Failed to lookup symbol 'mockingbird_*': symbol not found". -export_dynamic both
  # exports and pins them as GC roots (survives release dead-stripping). Applied
  # to the CONSUMER (app) target, where the final link happens.
  s.user_target_xcconfig = {
    'OTHER_LDFLAGS' => '-Wl,-export_dynamic',
  }
end
