/// Event field ids, in the order of `src/web/web_fields.hpp`.
enum WebEventField {
  /// `type`.
  type,

  /// `segment_id`.
  segmentId,

  /// `timestamp_us`.
  timestampUs,

  /// `reason_mask`.
  reasonMask,

  /// `confidence`.
  confidence,
}

/// Packet field ids, in the order of `src/web/web_fields.hpp`.
enum WebPacketField {
  /// `segment_id`.
  segmentId,

  /// `pts_us`.
  ptsUs,

  /// `capture_ts_us`.
  captureTsUs,

  /// `flags`.
  flags,

  /// `codec`.
  codec,

  /// `size`.
  size,

  /// `data`.
  data,
}

/// Stats field ids, in the order of `src/web/web_fields.hpp`.
enum WebStatsField {
  /// `frames_captured`.
  framesCaptured,

  /// `frames_dropped_capture`.
  framesDroppedCapture,

  /// `packets_emitted`.
  packetsEmitted,

  /// `packets_dropped`.
  packetsDropped,

  /// `segments_opened`.
  segmentsOpened,

  /// `segments_closed`.
  segmentsClosed,

  /// `pcm_ring_fill_frames`.
  pcmRingFillFrames,

  /// `packet_queue_fill`.
  packetQueueFill,

  /// `input_rms_dbfs`.
  inputRmsDbfs,

  /// `input_peak_dbfs`.
  inputPeakDbfs,

  /// `tap_samples_dropped`.
  tapSamplesDropped,

  /// `worker_busy_ppm`.
  workerBusyPpm,

  /// `worker_max_frame_us`.
  workerMaxFrameUs,

  /// `worker_wakeups`.
  workerWakeups,

  /// `sound_class_inferences`.
  soundClassInferences,

  /// `sound_class_inferences_skipped`.
  soundClassInferencesSkipped,

  /// `preproc_frames_skipped`.
  preprocFramesSkipped,

  /// `degradation_level`.
  degradationLevel,
}
