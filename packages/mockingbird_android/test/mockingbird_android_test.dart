import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird_android/mockingbird_android.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  final messenger =
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger;

  test('player focus goes through the plugin channel', () async {
    final calls = <String>[];
    messenger.setMockMethodCallHandler(
      const MethodChannel('io.mockingbird/playback'),
      (call) async {
        calls.add(call.method);
        return true;
      },
    );
    final platform = MockingbirdAndroid();
    await platform.requestPlaybackFocus();
    await platform.abandonPlaybackFocus();
    expect(calls, ['requestFocus', 'abandonFocus']);
  });

  test('focus changes arrive by name', () async {
    messenger.setMockStreamHandler(
      const EventChannel('io.mockingbird/playback_focus'),
      MockStreamHandler.inline(
        onListen: (arguments, events) {
          events
            ..success('lostTransient')
            ..success('gained')
            ..endOfStream();
        },
      ),
    );
    final changes = await MockingbirdAndroid().playbackFocusChanges.toList();
    expect(changes, [
      MockingbirdPlaybackFocus.lostTransient,
      MockingbirdPlaybackFocus.gained,
    ]);
  });
}
