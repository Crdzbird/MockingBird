import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';

void main() {
  group('MockingbirdSoundCategory.of', () {
    test('maps the AudioSet ranges', () {
      for (final id in [0, 2, 3, 12]) {
        expect(
          MockingbirdSoundCategory.of(id),
          MockingbirdSoundCategory.speech,
        );
      }
      expect(
        MockingbirdSoundCategory.of(13),
        MockingbirdSoundCategory.humanOther,
      );
      expect(
        MockingbirdSoundCategory.of(66),
        MockingbirdSoundCategory.humanOther,
      );
      expect(MockingbirdSoundCategory.of(67), MockingbirdSoundCategory.animal);
      expect(MockingbirdSoundCategory.of(131), MockingbirdSoundCategory.animal);
      expect(MockingbirdSoundCategory.of(132), MockingbirdSoundCategory.music);
      expect(MockingbirdSoundCategory.of(282), MockingbirdSoundCategory.music);
      expect(MockingbirdSoundCategory.of(283), MockingbirdSoundCategory.nature);
      expect(MockingbirdSoundCategory.of(299), MockingbirdSoundCategory.nature);
    });

    test('everything else, including silence and bad ids, is other', () {
      for (final id in [300, 494, 520, -1, 99999]) {
        expect(MockingbirdSoundCategory.of(id), MockingbirdSoundCategory.other);
      }
    });
  });

  group('MockingbirdSoundCategory.isHumanVoice', () {
    test('covers speech, laughter, crying and singing', () {
      for (final id in [0, 12, 13, 18, 19, 23, 24, 34]) {
        expect(
          MockingbirdSoundCategory.isHumanVoice(id),
          isTrue,
          reason: '$id',
        );
      }
    });

    test('excludes non-vocal human sounds and everything after', () {
      for (final id in [35, 66, 67, 494, -1]) {
        expect(
          MockingbirdSoundCategory.isHumanVoice(id),
          isFalse,
          reason: '$id',
        );
      }
    });
  });
}
