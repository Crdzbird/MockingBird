/// Event field ids, in the order of `src/web/web_fields.hpp`.
enum WebEventField { type, segmentId, timestampUs, reasonMask, confidence }

/// Packet field ids, in the order of `src/web/web_fields.hpp`.
enum WebPacketField {
  segmentId,
  ptsUs,
  captureTsUs,
  flags,
  codec,
  size,
  data,
}

/// Stats field ids, in the order of `src/web/web_fields.hpp`.
enum WebStatsField {
  framesCaptured,
  framesDroppedCapture,
  packetsEmitted,
  packetsDropped,
  segmentsOpened,
  segmentsClosed,
  pcmRingFillFrames,
  packetQueueFill,
  inputRmsDbfs,
  inputPeakDbfs,
  tapSamplesDropped,
  workerBusyPpm,
  workerMaxFrameUs,
  workerWakeups,
  soundClassInferences,
  soundClassInferencesSkipped,
  preprocFramesSkipped,
  degradationLevel,
}
