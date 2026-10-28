import 'dart:js_interop';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/web_assets.dart';
import 'package:web/web.dart' as web;

/// Loads the capture worklet and describes capture failures as events.
final class WebCaptureWorklet {
  /// Creates the helper.
  const factory WebCaptureWorklet() = WebCaptureWorklet._;

  const WebCaptureWorklet._();

  static const _file = 'mockingbird_capture_worklet.js';
  static const _permissionDenied = -8;
  static const _backendError = -4;
  static const _backendErrorType = 7;

  /// Registers the worklet module with [context], trying each asset URL.
  Future<void> add(web.AudioContext context) async {
    Object? failure;
    for (final url in const WebAssets().candidates(_file)) {
      try {
        await context.audioWorklet.addModule(url).toDart;
        return;
      } on Object catch (e) {
        failure = e;
      }
    }
    throw StateError(
      'mockingbird_web: could not load the capture worklet ($failure)',
    );
  }

  /// The backend-error event the platform reports when capture fails.
  MockingbirdNativeEvent errorEvent(Object error) {
    final denied = error.toString().contains('NotAllowed');
    final status = denied ? _permissionDenied : _backendError;
    return MockingbirdNativeEvent(
      type: _backendErrorType,
      segmentId: 0,
      timestampUs: 0,
      reasonMask: status.toUnsigned(32),
      confidence: 0,
    );
  }
}
