package mockingbird

/*
#include <stdlib.h>
#include "mockingbird/mockingbird.h"
*/
import "C"

import "unsafe"

// SetSpeakerModel names the speaker embedding model; an empty path falls
// back to MOCKINGBIRD_SPEAKER_MODEL, then speaker_campplus.onnx. Only while
// the engine is stopped.
func (e *Engine) SetSpeakerModel(path string) error {
	e.mu.RLock()
	defer e.mu.RUnlock()
	if e.c == nil {
		return ErrClosed
	}
	p := cStringOrNil(path)
	defer C.free(unsafe.Pointer(p))
	if st := Status(C.mockingbird_set_speaker_model(e.c, p)); st != StatusOK {
		return st.err("mockingbird_set_speaker_model")
	}
	return nil
}

// SpeakerEmbedding returns the voice print of 16 kHz mono samples, at least
// 0.5 s; 3 to 10 s of one person speaking works best.
func (e *Engine) SpeakerEmbedding(samples []int16) ([]float32, error) {
	e.mu.RLock()
	defer e.mu.RUnlock()
	if e.c == nil {
		return nil, ErrClosed
	}
	if len(samples) == 0 {
		return nil, StatusErrInvalidArg.err("mockingbird_speaker_embedding")
	}
	out := make([]float32, 4096)
	var dim C.uint32_t
	st := Status(C.mockingbird_speaker_embedding(e.c, (*C.int16_t)(unsafe.Pointer(&samples[0])),
		C.uint32_t(len(samples)), (*C.float)(unsafe.Pointer(&out[0])), C.uint32_t(len(out)), &dim))
	if st != StatusOK {
		return nil, st.err("mockingbird_speaker_embedding")
	}
	return out[:int(dim)], nil
}

// SetTargetSpeaker compares every utterance with embedding and reports an
// EventSpeaker; threshold is the cosine similarity that counts as a match,
// 0 for the default. A nil embedding stops.
func (e *Engine) SetTargetSpeaker(embedding []float32, threshold float32) error {
	e.mu.RLock()
	defer e.mu.RUnlock()
	if e.c == nil {
		return ErrClosed
	}
	var p *C.float
	if len(embedding) > 0 {
		p = (*C.float)(unsafe.Pointer(&embedding[0]))
	}
	st := Status(C.mockingbird_set_target_speaker(e.c, p, C.uint32_t(len(embedding)), C.float(threshold)))
	if st != StatusOK {
		return st.err("mockingbird_set_target_speaker")
	}
	return nil
}
