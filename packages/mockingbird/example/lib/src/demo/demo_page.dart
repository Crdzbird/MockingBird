import 'package:mockingbird_example/src/demo/demo_cubit.dart';
import 'package:mockingbird_example/src/demo/demo_view.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';

/// Provides the [DemoCubit] to the screen.
class DemoPage extends StatelessWidget {
  const DemoPage({super.key});

  @override
  Widget build(BuildContext context) =>
      BlocProvider(create: (_) => DemoCubit(), child: const DemoView());
}
