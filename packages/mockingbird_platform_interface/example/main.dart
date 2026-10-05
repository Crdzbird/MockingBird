import 'dart:io';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// Reads the engine version through whichever platform registered itself.
/// Apps use the `mockingbird` package instead of this one.
void main() {
  final platform = AurisPlatform.instance;
  stdout
    ..writeln('Auris ${platform.version()}')
    ..writeln(platform.statusName(AurisStatusCode.queueEmpty));
}
