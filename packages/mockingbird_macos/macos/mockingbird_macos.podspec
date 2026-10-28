#
# mockingbird_macos podspec. Mirror of mockingbird_ios; same prepare_command rsync of
# libmockingbird's C/C++ tree, same source globs. Differences:
#   - dependency FlutterMacOS, not Flutter
#   - platform osx, not ios
#   - no AVFoundation (macOS Core Audio is plain C; AVAudioSession is iOS-only)
#   - .m rename for miniaudio_impl still applies (miniaudio uses CoreAudio
#     APIs that are clean C, but the compile-as-ObjC trick is harmless and
#     keeps the source tree symmetric with iOS).
#
Pod::Spec.new do |s|
  s.name             = 'mockingbird_macos'
  s.version          = '0.1.0'
  s.summary          = 'macOS bridge for the Mockingbird audio engine.'
  s.description      = <<-DESC
  macOS implementation of the mockingbird plugin. Compiles libmockingbird's C ABI core
  (capture + detection + packetization) inline so the host app can call the
  C symbols directly via dart:ffi.
                       DESC
  s.homepage         = 'https://github.com/mockingbird'
  s.license          = { :type => 'MIT', :file => '../LICENSE' }
  s.author           = { 'Mockingbird' => 'noreply@example.com' }
  s.source           = { :path => '.' }

  s.prepare_command = <<-CMD
    set -euo pipefail
    rm -rf Sources
    mkdir -p Sources/include Sources/src Sources/third_party
    cp -R mockingbird_macos/Sources/mockingbird_native/include/mockingbird        Sources/include/
    cp -R mockingbird_macos/Sources/mockingbird_native/src/                  Sources/src/mockingbird/
    cp -R mockingbird_macos/Sources/mockingbird_native/third_party/miniaudio Sources/third_party/
    cp -R mockingbird_macos/Sources/mockingbird_native/third_party/libfvad   Sources/third_party/
    cp -R mockingbird_macos/Sources/mockingbird_native/third_party/pffft     Sources/third_party/
    cp -R mockingbird_macos/Sources/mockingbird_native/third_party/rnnoise   Sources/third_party/
    cp -R mockingbird_macos/Sources/mockingbird_native/third_party/opus      Sources/third_party/
    rm -rf Sources/third_party/opus/celt/{x86,arm,tests,dump_modes}
    rm -rf Sources/third_party/opus/silk/{x86,arm,mips,fixed,tests}
    rm -rf Sources/third_party/opus/silk/float/x86
    rm -rf Sources/third_party/opus/dnn
    rm -f Sources/third_party/opus/src/{opus_demo,opus_compare,repacketizer_demo,qext_compare}.c
    rm -f Sources/third_party/opus/celt/mini_kfft.c   # 1.6: test-only FFT
    rm -f Sources/third_party/opus/src/opus_multistream*.c
    rm -f Sources/third_party/opus/src/opus_projection*.c
    # celt/opus_custom_demo.c carries its own main() and references the
    # custom-mode API we don't compile (CUSTOM_MODES off); drop it so
    # force_load doesn't drag in unresolved opus_custom_* symbols.
    rm -f Sources/third_party/opus/celt/opus_custom_demo.c
    # miniaudio uses Core Audio (plain C) on macOS, so the .m rename is not
    # strictly required; it keeps the build the same as on iOS.
    mv Sources/src/mockingbird/capture/miniaudio_impl.c \
       Sources/src/mockingbird/capture/miniaudio_impl.m
  CMD

  s.source_files = [
    'mockingbird_macos/Sources/mockingbird_macos/**/*.swift',
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

  s.dependency 'FlutterMacOS'
  s.dependency 'onnxruntime-c', '~> 1.20'
  s.platform     = :osx
  # macOS 11 (Big Sur) is the first universal-binary baseline and is the
  # iOS-14-equivalent floor for std::atomic::wait/notify.
  # Xcode 27's SDK refuses deployment targets below 12.0.
  s.osx.deployment_target = '12.0'
  s.swift_version = '5.0'

  s.frameworks = 'CoreFoundation', 'CoreAudio', 'AudioToolbox'

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
    'OTHER_LDFLAGS' => '-Wl,-undefined,dynamic_lookup',
  }

  # dart:ffi resolves the C ABI through DynamicLibrary.process(), which searches
  # the HOST EXECUTABLE's dynamic symbol table. The C ABI is referenced only at
  # runtime (from Dart), so the linker would otherwise keep none of it and the
  # lookup fails with "Failed to lookup symbol 'mockingbird_*': symbol not found".
  # MockingbirdPlugin.register() (always called by Flutter) takes the address of the
  # ABI entry points, which keeps their whole transitive closure in the binary;
  # -export_dynamic then publishes those default-visibility symbols into the
  # dynamic symbol table. Applied to the CONSUMER (app) target.
  s.user_target_xcconfig = {
    'OTHER_LDFLAGS' => '-Wl,-export_dynamic',
  }
end
