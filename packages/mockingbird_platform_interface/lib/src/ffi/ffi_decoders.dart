import 'dart:ffi' as ffi;
import 'dart:typed_data';

import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_native_event.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_native_packet.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_native_stats.dart';

/// Copies native records into their plain Dart mirrors.
abstract final class FfiDecoders {
  /// Copies an event record.
  static MockingbirdNativeEvent event(mockingbird_event_t e) =>
      MockingbirdNativeEvent(
        type: e.typeAsInt,
        segmentId: e.segment_id,
        timestampUs: e.timestamp_us,
        reasonMask: e.reason_mask,
        confidence: e.confidence,
      );

  /// Copies a packet record and its payload.
  static MockingbirdNativePacket packet(mockingbird_packet_t p) =>
      MockingbirdNativePacket(
        segmentId: p.segment_id,
        ptsUs: p.pts_us,
        captureTsUs: p.capture_ts_us,
        flags: p.flags,
        codec: p.codecAsInt,
        data: p.size > 0 && p.data != ffi.nullptr
            ? Uint8List.fromList(p.data.asTypedList(p.size))
            : Uint8List(0),
      );

  /// Copies a stats record.
  static MockingbirdNativeStats stats(mockingbird_stats_t s) =>
      MockingbirdNativeStats(
        framesCaptured: s.frames_captured,
        framesDroppedCapture: s.frames_dropped_capture,
        packetsEmitted: s.packets_emitted,
        packetsDropped: s.packets_dropped,
        segmentsOpened: s.segments_opened,
        segmentsClosed: s.segments_closed,
        pcmRingFillFrames: s.pcm_ring_fill_frames,
        packetQueueFill: s.packet_queue_fill,
        inputRmsDbfs: s.input_rms_dbfs,
        inputPeakDbfs: s.input_peak_dbfs,
        tapSamplesDropped: s.tap_samples_dropped,
        workerBusyPpm: s.worker_busy_ppm,
        workerMaxFrameUs: s.worker_max_frame_us,
        workerWakeups: s.worker_wakeups,
        soundClassInferences: s.sound_class_inferences,
        soundClassInferencesSkipped: s.sound_class_inferences_skipped,
        preprocFramesSkipped: s.preproc_frames_skipped,
        degradationLevel: s.degradation_level,
      );
}
