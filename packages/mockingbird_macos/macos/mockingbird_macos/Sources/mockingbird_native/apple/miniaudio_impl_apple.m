/* Sources/mockingbird_native/apple/miniaudio_impl_apple.m
 *
 * miniaudio's Apple backend imports AVFoundation, so the ONE translation unit
 * that compiles miniaudio's implementation must be built as Objective-C.
 *
 * The shared implementation TU (src/capture/miniaudio_impl.c) is excluded from
 * this target and pulled in here instead, so the defines and miniaudio version
 * stay in exactly one place. The CocoaPods path achieves the same thing by
 * renaming the file to .m during prepare_command.
 */
#include "capture/miniaudio_impl.c"
