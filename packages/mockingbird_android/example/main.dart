import 'package:mockingbird_android/mockingbird_android.dart';

/// Flutter registers this platform on its own; apps use the `mockingbird`
/// package. Registering by hand is only needed in a plain Dart test harness.
void main() {
  AurisAndroid.registerWith();
}
