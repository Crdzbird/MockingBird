package auris

import "strings"

// AnomalyReason is the set of signal statistics that tripped the anomaly gate.
type AnomalyReason uint32

const (
	// AnomalyRMS means loudness jumped against the rolling background.
	AnomalyRMS AnomalyReason = 1 << iota
	// AnomalySpectralFlux means the spectrum changed abruptly.
	AnomalySpectralFlux
	// AnomalyCentroid means the spectral centre of mass shifted.
	AnomalyCentroid
)

// Has reports whether every bit of r is set.
func (m AnomalyReason) Has(r AnomalyReason) bool { return m&r == r }

// String lists the set reasons, comma separated.
func (m AnomalyReason) String() string {
	var names []string
	if m.Has(AnomalyRMS) {
		names = append(names, "rms")
	}
	if m.Has(AnomalySpectralFlux) {
		names = append(names, "spectralFlux")
	}
	if m.Has(AnomalyCentroid) {
		names = append(names, "centroid")
	}
	return strings.Join(names, ",")
}
