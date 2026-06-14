import 'package:auris/auris.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('Auris.categoryForYamnetClass', () {
    test('the whole human-voice cluster (0..12) maps to speech', () {
      // This is the fix for "speech only recognized when speaking English":
      // AudioSet splits voice across many labels, and non-English speech
      // commonly lands on Conversation/Narration rather than Speech. All of
      // them must read as speech.
      const voiceCluster = {
        0: 'Speech',
        1: 'Child speech, kid speaking',
        2: 'Conversation', // most common non-English landing spot
        3: 'Narration, monologue',
        4: 'Babbling',
        6: 'Shout',
        9: 'Yell',
        12: 'Whispering',
      };
      for (final id in voiceCluster.keys) {
        expect(
          Auris.categoryForYamnetClass(id),
          AurisSoundCategory.speech,
          reason: 'class $id (${voiceCluster[id]}) should be speech',
        );
        expect(Auris.isSpeechClass(id), isTrue);
      }
    });

    test('animal classes map to animal', () {
      // Dog(69), Bark(70), Cat(76), Meow(78), Animal(67).
      for (final id in [67, 69, 70, 76, 78, 131]) {
        expect(Auris.categoryForYamnetClass(id), AurisSoundCategory.animal);
        expect(Auris.isSpeechClass(id), isFalse);
      }
    });

    test('music classes map to music', () {
      for (final id in [132, 133, 200, 282]) {
        expect(Auris.categoryForYamnetClass(id), AurisSoundCategory.music);
      }
    });

    test('nature classes map to nature', () {
      for (final id in [283, 290, 299]) {
        expect(Auris.categoryForYamnetClass(id), AurisSoundCategory.nature);
      }
    });

    test('silence (494) and out-of-range map to other', () {
      expect(Auris.categoryForYamnetClass(494), AurisSoundCategory.other);
      expect(Auris.categoryForYamnetClass(300), AurisSoundCategory.other);
      expect(Auris.categoryForYamnetClass(520), AurisSoundCategory.other);
      expect(Auris.categoryForYamnetClass(-1), AurisSoundCategory.other);
      expect(Auris.categoryForYamnetClass(99999), AurisSoundCategory.other);
    });

    test('humanOther covers laughter/singing/footsteps (13..66)', () {
      // Laughter(13), Singing(24), Walk/footsteps(48), Crowd(64).
      for (final id in [13, 24, 48, 64, 66]) {
        expect(
          Auris.categoryForYamnetClass(id),
          AurisSoundCategory.humanOther,
        );
        // These are human sounds but NOT speech.
        expect(Auris.isSpeechClass(id), isFalse);
      }
    });
  });

  group('Auris.isHumanVoice', () {
    test('covers speech + laughter + crying + singing (0..34)', () {
      for (final id in [0, 2, 12, 13, 19, 24, 31, 34]) {
        expect(Auris.isHumanVoice(id), isTrue, reason: 'class $id is voice');
      }
    });

    test('excludes non-vocal human sounds and everything else', () {
      // Whistling(35), Breathing(36), Footsteps(48), Animal(67), Music(132).
      for (final id in [35, 36, 48, 67, 132, 494, -1]) {
        expect(Auris.isHumanVoice(id), isFalse);
      }
    });
  });

  group('AurisConfig.sensitive', () {
    test('clamps and maps the dial to all detector thresholds', () {
      final low = AurisConfig.sensitive(0);
      final high = AurisConfig.sensitive(1);

      // Lower sensitivity => higher (stricter) thresholds.
      expect(low.vadNeuralThreshold, greaterThan(high.vadNeuralThreshold));
      expect(low.anomalyRmsZ, greaterThan(high.anomalyRmsZ));
      expect(low.anomalyFluxZ, greaterThan(high.anomalyFluxZ));
      expect(low.soundClassThreshold, greaterThan(high.soundClassThreshold));

      // Endpoints land on the documented ranges.
      expect(low.vadNeuralThreshold, closeTo(0.80, 1e-9));
      expect(high.vadNeuralThreshold, closeTo(0.20, 1e-9));
      expect(low.vadAggressiveness, 3);
      expect(high.vadAggressiveness, 0);
    });

    test('out-of-range sensitivity is clamped to [0,1]', () {
      expect(
        AurisConfig.sensitive(5).vadNeuralThreshold,
        AurisConfig.sensitive(1).vadNeuralThreshold,
      );
      expect(
        AurisConfig.sensitive(-5).vadNeuralThreshold,
        AurisConfig.sensitive(0).vadNeuralThreshold,
      );
    });

    test('enables all detectors and uses NEURAL VAD by default', () {
      final c = AurisConfig.sensitive(0.5);
      expect(c.enableVad, isTrue);
      expect(c.enableAnomaly, isTrue);
      expect(c.enableSoundClass, isTrue);
      expect(c.vadTier, auris_vad_tier.AURIS_VAD_NEURAL);
    });
  });
}
