import 'package:flutter/material.dart';

/// The last error, in the error colours.
final class ErrorBanner extends StatelessWidget {
  const factory ErrorBanner({required String message, Key? key}) =
      ErrorBanner._;

  const ErrorBanner._({required this.message, super.key});

  final String message;

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    return Card(
      color: cs.errorContainer,
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Text(message, style: TextStyle(color: cs.onErrorContainer)),
      ),
    );
  }
}
