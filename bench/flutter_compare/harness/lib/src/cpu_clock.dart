import 'dart:ffi';

import 'package:ffi/ffi.dart';

final class _Timespec extends Struct {
  @Int64()
  external int sec;

  @Int64()
  external int nsec;
}

typedef _ClockGetTimeC = Int32 Function(Int32, Pointer<_Timespec>);
typedef _ClockGetTimeDart = int Function(int, Pointer<_Timespec>);

class CpuClock {
  CpuClock()
      : _clockGetTime = DynamicLibrary.process()
            .lookupFunction<_ClockGetTimeC, _ClockGetTimeDart>('clock_gettime');

  static const int _processCpuTimeId = 12;

  final _ClockGetTimeDart _clockGetTime;

  double nowSeconds() {
    final ts = calloc<_Timespec>();
    try {
      _clockGetTime(_processCpuTimeId, ts);
      return ts.ref.sec + ts.ref.nsec / 1e9;
    } finally {
      calloc.free(ts);
    }
  }
}
