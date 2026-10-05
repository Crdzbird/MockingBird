import 'package:mockingbird_ios/mockingbird_ios.dart';

/// Flutter registers this platform on its own; apps use the `mockingbird`
/// package. Registering by hand is only needed in a plain Dart test harness.
void main() {
  AurisIOS.registerWith();
}
