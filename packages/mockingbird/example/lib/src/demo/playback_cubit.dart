import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird_example/src/demo/capture_replay.dart';
import 'package:mockingbird_example/src/demo/playback_state.dart';

/// Keeps what the engine captured and plays it back on request.
final class PlaybackCubit extends Cubit<PlaybackState> {
  factory PlaybackCubit() = PlaybackCubit._;

  PlaybackCubit._() : super(const PlaybackState());

  final _replay = CaptureReplay();

  /// Keeps one captured packet.
  void keep(MockingbirdPacket packet) {
    if (packet.codec != MockingbirdCodec.opus) return;
    _replay.keep(packet);
    emit(state.copyWith(kept: _replay.kept));
  }

  /// Plays the kept capture through the device output.
  Future<void> play() async {
    if (state.playing) return;
    emit(state.copyWith(playing: true));
    try {
      await _replay.play((s) => emit(state.copyWith(stats: s)));
      emit(state.copyWith(playing: false));
    } on MockingbirdException catch (e) {
      emit(state.copyWith(playing: false, error: '$e'));
    }
  }
}
