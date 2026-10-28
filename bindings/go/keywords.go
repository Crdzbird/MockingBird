package mockingbird

/*
#include <stdlib.h>
#include "mockingbird/mockingbird.h"
*/
import "C"

import "unsafe"

func cStringOrNil(s string) *C.char {
	if s == "" {
		return nil
	}
	return C.CString(s)
}

// SetKeywordFrontend names the mel spectrogram and speech embedding models
// every keyword shares. Empty strings fall back to the
// MOCKINGBIRD_KEYWORD_MELSPECTROGRAM_MODEL and
// MOCKINGBIRD_KEYWORD_EMBEDDING_MODEL environment variables, then to
// melspectrogram.onnx and embedding_model.onnx in the working directory.
// Only while the engine is stopped.
func (e *Engine) SetKeywordFrontend(melspectrogram, embedding string) error {
	e.mu.RLock()
	defer e.mu.RUnlock()
	if e.c == nil {
		return ErrClosed
	}
	mel, emb := cStringOrNil(melspectrogram), cStringOrNil(embedding)
	defer C.free(unsafe.Pointer(mel))
	defer C.free(unsafe.Pointer(emb))
	if st := Status(C.mockingbird_set_keyword_frontend(e.c, mel, emb)); st != StatusOK {
		return st.err("mockingbird_set_keyword_frontend")
	}
	return nil
}

// AddKeyword registers an openWakeWord-compatible keyword model and returns
// the id its EventKeyword events carry. threshold is the score that fires
// it; 0 means 0.5. Takes effect from the next Start; up to 16 keywords.
func (e *Engine) AddKeyword(model string, threshold float32) (uint32, error) {
	e.mu.RLock()
	defer e.mu.RUnlock()
	if e.c == nil {
		return 0, ErrClosed
	}
	path := C.CString(model)
	defer C.free(unsafe.Pointer(path))
	var id C.uint32_t
	if st := Status(C.mockingbird_add_keyword(e.c, path, C.float(threshold), &id)); st != StatusOK {
		return 0, st.err("mockingbird_add_keyword")
	}
	return uint32(id), nil
}

// ClearKeywords removes every keyword. Only while the engine is stopped.
func (e *Engine) ClearKeywords() error {
	e.mu.RLock()
	defer e.mu.RUnlock()
	if e.c == nil {
		return ErrClosed
	}
	if st := Status(C.mockingbird_clear_keywords(e.c)); st != StatusOK {
		return st.err("mockingbird_clear_keywords")
	}
	return nil
}
