/// Called as a download proceeds with the bytes received so far and the
/// file's full size.
typedef MockingbirdDownloadProgress = void Function(int received, int total);
