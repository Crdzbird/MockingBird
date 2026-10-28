/// A model could not be downloaded from any of its sources.
final class MockingbirdDownloadException implements Exception {
  /// Reports that [fileName] failed, the last attempt with [cause].
  const factory MockingbirdDownloadException(String fileName, Object? cause) =
      MockingbirdDownloadException._;

  const MockingbirdDownloadException._(this.fileName, this.cause);

  /// The model that failed.
  final String fileName;

  /// What went wrong on the last attempt.
  final Object? cause;

  @override
  String toString() => 'MockingbirdDownloadException: $fileName ($cause)';
}
