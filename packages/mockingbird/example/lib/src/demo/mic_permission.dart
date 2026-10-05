import 'package:flutter/foundation.dart'
    show TargetPlatform, defaultTargetPlatform, kIsWeb;
import 'package:flutter/services.dart' show MissingPluginException;
import 'package:permission_handler/permission_handler.dart';

/// Asks for the microphone where a plugin can, and lets the platform prompt
/// where it cannot: macOS prompts on first capture, browsers on getUserMedia.
final class MicPermission {
  const MicPermission._();

  /// Null when capture may proceed, otherwise a message for the user.
  static Future<String?> request() async {
    if (kIsWeb || defaultTargetPlatform == TargetPlatform.macOS) return null;
    final PermissionStatus status;
    try {
      status = await Permission.microphone.request();
    } on MissingPluginException {
      return null;
    }
    if (status.isGranted) return null;
    if (status.isPermanentlyDenied || status.isRestricted) {
      await openAppSettings();
      return 'Microphone access is turned off. Opening Settings; enable the '
          'microphone for this app, then press Start again.';
    }
    return 'Microphone permission ${status.name}. Press Start to try again.';
  }
}
