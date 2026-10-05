// Field identifiers the WebAssembly shim exposes so a JavaScript or Dart
// caller never needs the C struct layouts. Order is part of the contract.
#pragma once

namespace mockingbird::web {

enum ConfigField : int {
    kBackend, kSampleRateHz, kFrameMs, kEnableRnnoise, kEnableVad, kVadTier,
    kVadAggressiveness, kVadNeuralThreshold, kVadHangoverMs, kEnableAnomaly,
    kAnomalyRmsZ, kAnomalyFluxZ, kAnomalyBackgroundMs, kEnableSoundClass,
    kSoundClassThreshold, kSoundClassIntervalMs, kPreRollMs, kPostRollMs,
    kSegmentMaxMs, kCodec, kOpusBitrateBps, kOpusVbr, kOpusComplexity,
    kPcmRingFrames, kPacketQueueCapacity, kEnablePcmTap, kPcmTapFrames,
    kSilenceGateDbfs, kEnableAdaptiveDegradation, kDegradeBusyPpm,
    kWorkerBatchFrames, kConfigFieldCount
};

enum EventField : int {
    kEventType, kEventSegmentId, kEventTimestampUs, kEventReasonMask,
    kEventConfidence, kEventFieldCount
};

enum PacketField : int {
    kPacketSegmentId, kPacketPtsUs, kPacketCaptureTsUs, kPacketFlags,
    kPacketCodec, kPacketSize, kPacketData, kPacketFieldCount
};

enum StatsField : int {
    kFramesCaptured, kFramesDroppedCapture, kPacketsEmitted, kPacketsDropped,
    kSegmentsOpened, kSegmentsClosed, kPcmRingFillFrames, kPacketQueueFill,
    kInputRmsDbfs, kInputPeakDbfs, kTapSamplesDropped, kWorkerBusyPpm,
    kWorkerMaxFrameUs, kWorkerWakeups, kSoundClassInferences,
    kSoundClassInferencesSkipped, kPreprocFramesSkipped, kDegradationLevel,
    kStatsFieldCount
};

} // namespace mockingbird::web
