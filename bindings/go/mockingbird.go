// Package mockingbird is a Go binding for the Mockingbird native audio engine: capture,
// optional RNNoise, language-agnostic voice detection, an anomaly gate, YAMNet
// sound classification, and Opus or PCM packets, all over the library's C ABI.
//
// Create an [Engine] with [New], call [Engine.Start], read [Engine.Events] and
// [Engine.Packets], and release it with [Engine.Close]. Control methods are
// safe to call from any goroutine; a slow consumer never stalls the engine,
// surplus is dropped and counted by [Engine.Dropped].
//
// The cgo directives below expect libmockingbird at the repository's build/ tree;
// override CGO_CFLAGS and CGO_LDFLAGS for an installed copy.
package mockingbird

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo LDFLAGS: -L${SRCDIR}/../../build -lmockingbird -Wl,-rpath,${SRCDIR}/../../build

#include "mockingbird/mockingbird.h"
*/
import "C"

import "errors"

// ErrClosed is returned by every method of an engine that has been closed.
var ErrClosed = errors.New("mockingbird: engine is closed")

// Version returns the "X.Y.Z" version of the linked libmockingbird.
func Version() string { return C.GoString(C.mockingbird_version_string()) }
