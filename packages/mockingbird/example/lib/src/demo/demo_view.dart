import 'package:mockingbird_example/src/demo/demo_cubit.dart';
import 'package:mockingbird_example/src/demo/demo_state.dart';
import 'package:mockingbird_example/src/widgets/denoise_switch.dart';
import 'package:mockingbird_example/src/widgets/error_banner.dart';
import 'package:mockingbird_example/src/widgets/event_log.dart';
import 'package:mockingbird_example/src/widgets/last_sound_card.dart';
import 'package:mockingbird_example/src/widgets/level_meter.dart';
import 'package:mockingbird_example/src/widgets/preset_chips.dart';
import 'package:mockingbird_example/src/widgets/sensitivity_slider.dart';
import 'package:mockingbird_example/src/widgets/start_stop_bar.dart';
import 'package:mockingbird_example/src/widgets/stats_card.dart';
import 'package:mockingbird_example/src/widgets/voice_indicator.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';

/// The demo screen. A scrolling list, so nothing overflows on small phones.
class DemoView extends StatelessWidget {
  const DemoView({super.key});

  @override
  Widget build(BuildContext context) {
    final cubit = context.read<DemoCubit>();
    final cs = Theme.of(context).colorScheme;
    return Scaffold(
      appBar: AppBar(
        title: const Text('Auris demo'),
        backgroundColor: cs.surfaceContainerHigh,
      ),
      body: SafeArea(
        child: BlocBuilder<DemoCubit, DemoState>(
          builder: (context, state) => ListView(
            padding: const EdgeInsets.all(16),
            children: [
              StartStopBar(
                running: state.running,
                onStart: cubit.start,
                onStop: cubit.stop,
              ),
              const SizedBox(height: 12),
              LevelMeter(
                rmsDbfs: state.stats?.inputRmsDbfs ?? 0,
                peakDbfs: state.stats?.inputPeakDbfs ?? 0,
                running: state.running,
              ),
              const SizedBox(height: 12),
              PresetChips(
                selected: state.presetIndex,
                onSelected: cubit.selectPreset,
              ),
              const SizedBox(height: 12),
              SensitivitySlider(
                value: state.sensitivity,
                onChanged: cubit.previewSensitivity,
                onChangeEnd: cubit.setSensitivity,
              ),
              DenoiseSwitch(
                value: state.denoise,
                onChanged: (v) => cubit.setDenoise(enabled: v),
              ),
              const SizedBox(height: 4),
              if (state.error case final message?)
                ErrorBanner(message: message),
              VoiceIndicator(
                active: state.detection.voiceActive,
                running: state.running,
              ),
              const SizedBox(height: 8),
              LastSoundCard(detection: state.detection, running: state.running),
              const SizedBox(height: 12),
              StatsCard(
                stats: state.stats,
                running: state.running,
                packetsReceived: state.packetCount,
                bytesReceived: state.packetBytes,
              ),
              const SizedBox(height: 16),
              Text('Events', style: Theme.of(context).textTheme.titleMedium),
              const SizedBox(height: 8),
              SizedBox(height: 220, child: EventLog(lines: state.log)),
            ],
          ),
        ),
      ),
    );
  }
}
