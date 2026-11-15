import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// A stats snapshot with every counter at zero.
const zeroStats = MockingbirdNativeStats(
  framesCaptured: 0,
  framesDroppedCapture: 0,
  packetsEmitted: 0,
  packetsDropped: 0,
  segmentsOpened: 0,
  segmentsClosed: 0,
  pcmRingFillFrames: 0,
  packetQueueFill: 0,
  inputRmsDbfs: 0,
  inputPeakDbfs: 0,
  tapSamplesDropped: 0,
  worker: MockingbirdNativeWorkerStats(
    busyPpm: 0,
    maxFrameUs: 0,
    wakeups: 0,
    soundClassInferences: 0,
    soundClassInferencesSkipped: 0,
    preprocFramesSkipped: 0,
    degradationLevel: 0,
  ),
  voiceProcessingActive: 0,
  captureRestarts: 0,
);
