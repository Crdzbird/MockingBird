import 'dart:typed_data';

import 'package:mockingbird_platform_interface/src/mockingbird_engine_handle.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_native_event.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_native_packet.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_native_stats.dart';

/// The result of creating an engine: a status code and, on success, a handle.
typedef MockingbirdCreateResult = ({
  int status,
  MockingbirdEngineHandle? handle,
});

/// A status code with the samples a PCM tap read.
typedef MockingbirdPcmResult = ({int status, Int16List samples});

/// A status code with the event polled, if any.
typedef MockingbirdEventResult = ({int status, MockingbirdNativeEvent? event});

/// A status code with the packet drained, if any.
typedef MockingbirdPacketResult = ({
  int status,
  MockingbirdNativePacket? packet,
});

/// A status code with the stats snapshot, if read.
typedef MockingbirdStatsResult = ({int status, MockingbirdNativeStats? stats});
