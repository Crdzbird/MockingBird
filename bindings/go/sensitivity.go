package auris

// Sensitivity is one 0..1 dial that drives every detector's thresholds:
// 0 is the least sensitive, 1 catches faint and borderline sounds.
type Sensitivity float64

// NewSensitivity clamps v into 0..1.
func NewSensitivity(v float64) Sensitivity { return Sensitivity(min(1, max(0, v))) }

// Lerp is the threshold value at this sensitivity, moving from atZero (least
// sensitive) to atOne (most sensitive).
func (s Sensitivity) Lerp(atZero, atOne float64) float64 {
	return atZero + (atOne-atZero)*float64(s)
}

// Sensitive builds a Config with every detector on, neural VAD, and all
// thresholds derived from dial. Options adjust the rest.
func Sensitive(dial float64, opts ...Option) Config {
	s := NewSensitivity(dial)
	c := DefaultConfig()
	c.VAD = c.VAD.sensitive(s)
	c.Anomaly = c.Anomaly.sensitive(s)
	c.SoundClass = c.SoundClass.sensitive(s)
	for _, o := range opts {
		o(&c)
	}
	return c
}
