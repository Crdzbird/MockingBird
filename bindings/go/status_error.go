package auris

import "fmt"

// StatusError is a failed native call. Use errors.As to branch on Status.
type StatusError struct {
	// Op is the native function that failed, such as "auris_start".
	Op string
	// Status is the code it returned.
	Status Status
}

// Error formats the failure with the library's status name.
func (e *StatusError) Error() string {
	return fmt.Sprintf("auris: %s failed: %s (%d)", e.Op, e.Status, int(e.Status))
}
