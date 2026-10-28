package mockingbird

/*
#include <stdlib.h>
#include "mockingbird/mockingbird.h"
*/
import "C"

import "unsafe"

// InputDevice is a microphone the system offers.
type InputDevice struct {
	// ID is what Engine.SelectInputDevice takes.
	ID        string
	Name      string
	IsDefault bool
}

// InputDevices lists the inputs backend can open. Android has no native
// listing; there it returns an error with StatusUnsupported.
func InputDevices(backend Backend) ([]InputDevice, error) {
	const capacity = 64
	out := make([]C.mockingbird_device_t, capacity)
	var n C.uint32_t
	st := Status(C.mockingbird_list_input_devices(C.mockingbird_backend_t(backend), &out[0], capacity, &n))
	if st != StatusOK {
		return nil, st.err("mockingbird_list_input_devices")
	}
	count := int(n)
	if count > capacity {
		count = capacity
	}
	devices := make([]InputDevice, 0, count)
	for _, d := range out[:count] {
		devices = append(devices, InputDevice{
			ID:        C.GoString(&d.id[0]),
			Name:      C.GoString(&d.name[0]),
			IsDefault: d.is_default != 0,
		})
	}
	return devices, nil
}

// SelectInputDevice opens id, from InputDevices, on the next Start; an empty
// id follows the system default. Only while the engine is stopped.
func (e *Engine) SelectInputDevice(id string) error {
	e.mu.RLock()
	defer e.mu.RUnlock()
	if e.c == nil {
		return ErrClosed
	}
	var cid *C.char
	if id != "" {
		cid = C.CString(id)
		defer C.free(unsafe.Pointer(cid))
	}
	if st := Status(C.mockingbird_set_input_device(e.c, cid)); st != StatusOK {
		return st.err("mockingbird_set_input_device")
	}
	return nil
}
