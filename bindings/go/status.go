package auris

/*
#include "auris/auris.h"
*/
import "C"

// Status mirrors the engine's result codes.
type Status int

const (
	StatusOK                Status = 0
	StatusErrInvalidArg     Status = -1
	StatusErrInvalidState   Status = -2
	StatusErrNoMemory       Status = -3
	StatusErrBackend        Status = -4
	StatusErrNotRunning     Status = -5
	StatusErrAlreadyRunning Status = -6
	StatusErrQueueEmpty     Status = -7
	StatusErrPermission     Status = -8
	StatusErrUnsupported    Status = -9
	StatusErrInternal       Status = -10
)

// String returns the library's own name for the status.
func (s Status) String() string { return C.GoString(C.auris_status_str(C.int(s))) }

func (s Status) err(op string) error { return &StatusError{Op: op, Status: s} }
