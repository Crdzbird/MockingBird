// Package auris is a Go binding for the Auris native audio engine: capture,
// optional RNNoise, language-agnostic voice detection, an anomaly gate, YAMNet
// sound classification, and Opus or PCM packets, all over the library's C ABI.
//
// Create an [Engine] with [New], call [Engine.Start], read [Engine.Events] and
// [Engine.Packets], and release it with [Engine.Close]. Control methods are
// safe to call from any goroutine; a slow consumer never stalls the engine,
// surplus is dropped and counted by [Engine.Dropped].
//
// The cgo directives below expect libauris at the repository's build/ tree;
// override CGO_CFLAGS and CGO_LDFLAGS for an installed copy.
package auris

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo LDFLAGS: -L${SRCDIR}/../../build -lauris -Wl,-rpath,${SRCDIR}/../../build

#include "auris/auris.h"
*/
import "C"

import "errors"

// ErrClosed is returned by every method of an engine that has been closed.
var ErrClosed = errors.New("auris: engine is closed")

// Version returns the "X.Y.Z" version of the linked libauris.
func Version() string { return C.GoString(C.auris_version_string()) }
