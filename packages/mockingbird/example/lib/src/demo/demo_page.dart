import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:mockingbird_example/src/demo/demo_cubit.dart';
import 'package:mockingbird_example/src/demo/demo_view.dart';

/// Provides the [DemoCubit] to the screen.
final class DemoPage extends StatelessWidget {
  const factory DemoPage({Key? key}) = DemoPage._;

  const DemoPage._({super.key});

  @override
  Widget build(BuildContext context) =>
      BlocProvider(create: (_) => DemoCubit(), child: const DemoView());
}
