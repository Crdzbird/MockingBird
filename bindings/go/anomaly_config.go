package auris

// AnomalyConfig drives the anomaly gate: sudden or unusual sounds measured
// against a rolling background.
type AnomalyConfig struct {
	Enabled bool
	// RMSZ is the loudness z-score trigger; lower is more sensitive.
	RMSZ float32
	// FluxZ is the spectral-flux z-score trigger; lower is more sensitive.
	FluxZ float32
	// BackgroundMs is the rolling window the z-scores are measured against.
	BackgroundMs uint32
}

// DefaultAnomalyConfig triggers at three sigma over a 2 s background.
func DefaultAnomalyConfig() AnomalyConfig {
	return AnomalyConfig{Enabled: true, RMSZ: 3, FluxZ: 3, BackgroundMs: 2000}
}

func (a AnomalyConfig) sensitive(s Sensitivity) AnomalyConfig {
	a.Enabled = true
	a.RMSZ = float32(s.Lerp(5, 1.5))
	a.FluxZ = a.RMSZ
	return a
}
