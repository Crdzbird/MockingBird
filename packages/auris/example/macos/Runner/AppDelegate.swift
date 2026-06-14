import Cocoa
import FlutterMacOS
import AVFoundation

@main
class AppDelegate: FlutterAppDelegate {
  override func applicationDidFinishLaunching(_ notification: Notification) {
    super.applicationDidFinishLaunching(notification)
    // Surface the microphone TCC prompt up front. miniaudio's CoreAudio input
    // path does not request authorization itself (it simply receives silence
    // when access is denied), so we ask explicitly. The app carries the
    // com.apple.security.device.audio-input entitlement and the
    // NSMicrophoneUsageDescription string, which this dialog requires.
    if AVCaptureDevice.authorizationStatus(for: .audio) == .notDetermined {
      AVCaptureDevice.requestAccess(for: .audio) { _ in }
    }
  }

  override func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
    return true
  }

  override func applicationSupportsSecureRestorableState(_ app: NSApplication) -> Bool {
    return true
  }
}
