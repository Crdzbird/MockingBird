import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird_example/src/demo/demo_models.dart';
import 'package:mockingbird_example/src/demo/demo_state.dart';
import 'package:mockingbird_example/src/demo/detection_summary.dart';
import 'package:mockingbird_example/src/demo/engine_session.dart';
import 'package:mockingbird_example/src/demo/mic_permission.dart';

/// Drives one engine session from the screen's controls.
final class DemoCubit extends Cubit<DemoState> {
  factory DemoCubit([void Function(MockingbirdPacket)? onPacket]) = DemoCubit._;

  DemoCubit._([this._onPacket]) : super(const DemoState());

  final void Function(MockingbirdPacket)? _onPacket;

  EngineSession? _session;
  DemoModels _models = const DemoModels();

  Future<void> start() async {
    emit(state.copyWith(error: () => null, packetCount: 0, packetBytes: 0));
    if (await MicPermission.request() case final denied?) {
      emit(state.copyWith(error: () => denied));
      return;
    }
    _models = await DemoModels.load();
    try {
      _session = await EngineSession.open(
        _config(state.sensitivity),
        onEvent: (e) => emit(state.withEvent(e, _models.formatter)),
        onStats: (s) => emit(state.copyWith(stats: s)),
        onPacket: _packet,
      );
      emit(state.copyWith(running: true));
    } on MockingbirdException catch (e) {
      emit(state.copyWith(error: () => '$e'));
    }
  }

  void _packet(MockingbirdPacket p) {
    _onPacket?.call(p);
    emit(state.withPacket(p));
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
    } on MockingbirdException catch (e) {
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

  MockingbirdConfig _config(double sensitivity) => state.preset.config(
    sensitivity: sensitivity,
    denoise: state.denoise,
    neuralVadModelPath: _models.neuralVadModelPath,
    soundClassModelPath: _models.soundClassModelPath,
  );

  Future<void> _restartIfRunning() async {
    if (!state.running) return;
    await stop();
    await start();
  }
}
