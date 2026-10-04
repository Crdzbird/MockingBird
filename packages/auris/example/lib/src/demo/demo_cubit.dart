import 'package:auris/auris.dart';
import 'package:auris_example/src/demo/demo_state.dart';
import 'package:auris_example/src/demo/detection_summary.dart';
import 'package:auris_example/src/demo/engine_session.dart';
import 'package:auris_example/src/demo/event_formatter.dart';
import 'package:auris_example/src/demo/mic_permission.dart';
import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:flutter_bloc/flutter_bloc.dart';

/// Drives one engine session from the screen's controls.
final class DemoCubit extends Cubit<DemoState> {
  DemoCubit() : super(const DemoState());

  EngineSession? _session;
  EventFormatter _formatter = const EventFormatter([]);
  String _neuralVadModelPath = '';
  String _soundClassModelPath = '';

  Future<void> start() async {
    emit(state.copyWith(error: () => null, packetCount: 0, packetBytes: 0));
    if (await MicPermission.request() case final denied?) {
      emit(state.copyWith(error: () => denied));
      return;
    }
    if (!kIsWeb) await _loadModels();
    try {
      _session = await EngineSession.open(
        _config(state.sensitivity),
        onEvent: (e) => emit(state.withEvent(e, _formatter)),
        onStats: (s) => emit(state.copyWith(stats: s)),
        onPacket: (p) => emit(state.withPacket(p)),
      );
      emit(state.copyWith(running: true));
    } on AurisException catch (e) {
      emit(state.copyWith(error: () => '$e'));
    }
  }

  Future<void> stop() async {
    final session = _session;
    if (session == null) return;
    _session = null;
    await session.close();
    emit(state.copyWith(running: false, detection: const DetectionSummary()));
  }

  void previewSensitivity(double value) =>
      emit(state.copyWith(sensitivity: value));

  /// Retunes the running engine in place: no teardown, no model reload.
  void setSensitivity(double value) {
    previewSensitivity(value);
    final engine = _session?.engine;
    if (engine == null) return;
    try {
      engine.updateConfig(_config(value));
    } on AurisException catch (e) {
      emit(state.copyWith(error: () => 'retune failed: $e'));
    }
  }

  Future<void> selectPreset(int index) async {
    emit(state.copyWith(presetIndex: index));
    emit(state.copyWith(sensitivity: state.preset.sensitivity));
    await _restartIfRunning();
  }

  Future<void> setDenoise({required bool enabled}) async {
    emit(state.copyWith(denoise: enabled));
    await _restartIfRunning();
  }

  @override
  Future<void> close() async {
    await stop();
    await super.close();
  }

  Future<void> _loadModels() async {
    _formatter = EventFormatter(await AurisModels.yamnetLabels());
    _neuralVadModelPath = await AurisModels.neuralVadPath();
    _soundClassModelPath = await AurisModels.soundClassPath();
  }

  AurisConfig _config(double sensitivity) => state.preset.config(
    sensitivity: sensitivity,
    denoise: state.denoise,
    neuralVadModelPath: _neuralVadModelPath,
    soundClassModelPath: _soundClassModelPath,
  );

  Future<void> _restartIfRunning() async {
    if (!state.running) return;
    await stop();
    await start();
  }
}
