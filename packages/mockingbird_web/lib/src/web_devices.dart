import 'dart:js_interop';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/web_engine_handle.dart';
import 'package:web/web.dart' as web;

/// Microphones through `enumerateDevices`. Browsers only name them after the
/// page has been granted the microphone once.
final class WebDevices {
  /// Creates the helper.
  const factory WebDevices() = WebDevices._;

  const WebDevices._();

  /// The page's audio inputs.
  Future<MockingbirdDevicesResult> list() async {
    final all = await web.window.navigator.mediaDevices
        .enumerateDevices()
        .toDart;
    return (
      status: MockingbirdStatusCode.ok,
      devices: [
        for (final d in all.toDart)
          if (d.kind == 'audioinput')
            MockingbirdNativeDevice(
              id: d.deviceId,
              name: d.label,
              isDefault: d.deviceId == 'default',
            ),
      ],
    );
  }

  /// Chooses the microphone [h] opens next; only while it is stopped.
  int select(WebEngineHandle h, String? id) {
    if (h.capture != null) return MockingbirdStatusCode.invalidState;
    h.deviceId = (id == null || id.isEmpty) ? null : id;
    return MockingbirdStatusCode.ok;
  }
}
