import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:mockingbird_example/src/demo/demo_cubit.dart';
import 'package:mockingbird_example/src/demo/demo_view.dart';
import 'package:mockingbird_example/src/demo/playback_cubit.dart';

/// Provides the [DemoCubit] and the [PlaybackCubit] it feeds to the screen.
final class DemoPage extends StatelessWidget {
  const factory DemoPage({Key? key}) = DemoPage._;

  const DemoPage._({super.key});

  @override
  Widget build(BuildContext context) => BlocProvider(
    create: (_) => PlaybackCubit(),
    child: BlocProvider(
      create: (context) => DemoCubit(context.read<PlaybackCubit>().keep),
      child: const DemoView(),
    ),
  );
}
