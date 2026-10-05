import 'package:mockingbird/mockingbird.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('AurisSoundCategory.of', () {
    test('maps the AudioSet ranges', () {
      for (final id in [0, 2, 3, 12]) {
        expect(AurisSoundCategory.of(id), AurisSoundCategory.speech);
      }
      expect(AurisSoundCategory.of(13), AurisSoundCategory.humanOther);
      expect(AurisSoundCategory.of(66), AurisSoundCategory.humanOther);
      expect(AurisSoundCategory.of(67), AurisSoundCategory.animal);
      expect(AurisSoundCategory.of(131), AurisSoundCategory.animal);
      expect(AurisSoundCategory.of(132), AurisSoundCategory.music);
      expect(AurisSoundCategory.of(282), AurisSoundCategory.music);
      expect(AurisSoundCategory.of(283), AurisSoundCategory.nature);
      expect(AurisSoundCategory.of(299), AurisSoundCategory.nature);
    });

    test('everything else, including silence and bad ids, is other', () {
      for (final id in [300, 494, 520, -1, 99999]) {
        expect(AurisSoundCategory.of(id), AurisSoundCategory.other);
      }
    });
  });

  group('AurisSoundCategory.isHumanVoice', () {
    test('covers speech, laughter, crying and singing', () {
      for (final id in [0, 12, 13, 18, 19, 23, 24, 34]) {
        expect(AurisSoundCategory.isHumanVoice(id), isTrue, reason: '$id');
      }
    });

    test('excludes non-vocal human sounds and everything after', () {
      for (final id in [35, 66, 67, 494, -1]) {
        expect(AurisSoundCategory.isHumanVoice(id), isFalse, reason: '$id');
      }
    });
  });
}
