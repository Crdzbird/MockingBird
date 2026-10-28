/// A model file the engine can download: where to get it and how to know it
/// arrived intact.
final class MockingbirdModelSpec {
  /// Describes [fileName], [bytes] long with SHA-256 [sha256], available from
  /// each of [urls] in turn.
  const factory MockingbirdModelSpec({
    required String fileName,
    required int bytes,
    required String sha256,
    required List<Uri> urls,
  }) = MockingbirdModelSpec._;

  const MockingbirdModelSpec._({
    required this.fileName,
    required this.bytes,
    required this.sha256,
    required this.urls,
  });

  /// The file's name, which the engine sees at the end of its path.
  final String fileName;

  /// The exact size of the file.
  final int bytes;

  /// The lowercase hex SHA-256 of the file.
  final String sha256;

  /// Where to fetch it from, tried in order.
  final List<Uri> urls;

  /// The same model with the file also looked for under each of [bases]
  /// first, such as an app's own server.
  MockingbirdModelSpec withMirrors(List<Uri> bases) => MockingbirdModelSpec(
    fileName: fileName,
    bytes: bytes,
    sha256: sha256,
    urls: [for (final base in bases) base.resolve(fileName), ...urls],
  );
}
