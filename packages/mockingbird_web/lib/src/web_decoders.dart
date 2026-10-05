import 'dart:js_interop';
import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/auris_module.dart';
import 'package:mockingbird_web/src/web_record_field.dart';

/// Reads native records through the shim's field getters.
final class WebDecoders {
  const WebDecoders(this._m);

  final AurisModule _m;

  double _e(int p, WebEventField f) => _m.auris_web_event_get(p, f.index);
  double _p(int p, WebPacketField f) => _m.auris_web_packet_get(p, f.index);
  double _s(int p, WebStatsField f) => _m.auris_web_stats_get(p, f.index);

  AurisNativeEvent event(int p) => AurisNativeEvent(
        type: _e(p, WebEventField.type).toInt(),
        segmentId: _e(p, WebEventField.segmentId).toInt(),
        timestampUs: _e(p, WebEventField.timestampUs).toInt(),
        reasonMask: _e(p, WebEventField.reasonMask).toInt(),
        confidence: _e(p, WebEventField.confidence),
      );

  AurisNativePacket packet(int p) {
    final size = _p(p, WebPacketField.size).toInt();
    final data = _p(p, WebPacketField.data).toInt();
    final bytes = size > 0 && data != 0
        ? Uint8List.fromList(_m.HEAPU8.toDart.sublist(data, data + size))
        : Uint8List(0);
    return AurisNativePacket(
      segmentId: _p(p, WebPacketField.segmentId).toInt(),
      ptsUs: _p(p, WebPacketField.ptsUs).toInt(),
      captureTsUs: _p(p, WebPacketField.captureTsUs).toInt(),
      flags: _p(p, WebPacketField.flags).toInt(),
      codec: _p(p, WebPacketField.codec).toInt(),
      data: bytes,
    );
  }

  AurisNativeStats stats(int p) => AurisNativeStats(
        framesCaptured: _s(p, WebStatsField.framesCaptured).toInt(),
        framesDroppedCapture: _s(p, WebStatsField.framesDroppedCapture).toInt(),
        packetsEmitted: _s(p, WebStatsField.packetsEmitted).toInt(),
        packetsDropped: _s(p, WebStatsField.packetsDropped).toInt(),
        segmentsOpened: _s(p, WebStatsField.segmentsOpened).toInt(),
        segmentsClosed: _s(p, WebStatsField.segmentsClosed).toInt(),
        pcmRingFillFrames: _s(p, WebStatsField.pcmRingFillFrames).toInt(),
        packetQueueFill: _s(p, WebStatsField.packetQueueFill).toInt(),
        inputRmsDbfs: _s(p, WebStatsField.inputRmsDbfs),
        inputPeakDbfs: _s(p, WebStatsField.inputPeakDbfs),
        tapSamplesDropped: _s(p, WebStatsField.tapSamplesDropped).toInt(),
        workerBusyPpm: _s(p, WebStatsField.workerBusyPpm).toInt(),
        workerMaxFrameUs: _s(p, WebStatsField.workerMaxFrameUs).toInt(),
        workerWakeups: _s(p, WebStatsField.workerWakeups).toInt(),
        soundClassInferences:
            _s(p, WebStatsField.soundClassInferences).toInt(),
        soundClassInferencesSkipped:
            _s(p, WebStatsField.soundClassInferencesSkipped).toInt(),
        preprocFramesSkipped: _s(p, WebStatsField.preprocFramesSkipped).toInt(),
        degradationLevel: _s(p, WebStatsField.degradationLevel).toInt(),
      );
}
