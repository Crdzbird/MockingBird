package auris

import "testing"

func TestSoundCategoryOf(t *testing.T) {
	cases := map[SoundCategory][]int{
		CategorySpeech:     {0, 1, 2, 3, 4, 6, 9, 12},
		CategoryHumanOther: {13, 40, 66},
		CategoryAnimal:     {67, 69, 70, 76, 131},
		CategoryMusic:      {132, 200, 282},
		CategoryNature:     {283, 299},
		CategoryOther:      {494, 300, -1, 99999},
	}
	for want, classes := range cases {
		for _, c := range classes {
			if got := SoundCategoryOf(c); got != want {
				t.Errorf("class %d: got %v, want %v", c, got, want)
			}
		}
	}
}

func TestIsHumanVoice(t *testing.T) {
	for _, c := range []int{0, 2, 12, 13, 24, 34} {
		if !IsHumanVoice(c) {
			t.Errorf("class %d should be human voice", c)
		}
	}
	for _, c := range []int{35, 36, 48, 67, 132, 494, -1} {
		if IsHumanVoice(c) {
			t.Errorf("class %d should not be human voice", c)
		}
	}
}

func TestEventAccessors(t *testing.T) {
	sound := Event{Type: EventSoundClass, ReasonMask: 70, Confidence: 0.9}
	if sound.Category() != CategoryAnimal || sound.IsHumanVoice() {
		t.Errorf("sound class 70: %v %v", sound.Category(), sound.IsHumanVoice())
	}
	anomaly := Event{Type: EventAnomaly, ReasonMask: 5}
	if r := anomaly.AnomalyReasons(); !r.Has(AnomalyRMS) || !r.Has(AnomalyCentroid) || r.Has(AnomalySpectralFlux) {
		t.Errorf("reasons = %v", r)
	}
	backend := Event{Type: EventBackendError, ReasonMask: 0xFFFFFFFC}
	if backend.BackendError() != StatusErrBackend {
		t.Errorf("backend error = %v", backend.BackendError())
	}
	degraded := Event{Type: EventDegradation, ReasonMask: 2, Confidence: 0.83}
	if degraded.DegradationLevel() != 2 || degraded.BusyFraction() < 0.82 {
		t.Errorf("degradation = %d %v", degraded.DegradationLevel(), degraded.BusyFraction())
	}
}
