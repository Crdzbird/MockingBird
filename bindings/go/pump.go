package mockingbird

/*
#include "mockingbird/mockingbird.h"
*/
import "C"

import "time"

const pollInterval = 2 * time.Millisecond

func (e *Engine) pump(c *C.mockingbird_engine_t, done chan struct{}) {
	defer e.wg.Done()
	t := time.NewTicker(pollInterval)
	defer t.Stop()
	for {
		select {
		case <-done:
			e.drainOnce(c)
			return
		case <-t.C:
			e.drainOnce(c)
		}
	}
}

func (e *Engine) drainOnce(c *C.mockingbird_engine_t) {
	for {
		var p C.mockingbird_packet_t
		if Status(C.mockingbird_drain(c, &p)) != StatusOK {
			break
		}
		pkt := packetFromC(p)
		C.mockingbird_release(c, &p)
		select {
		case e.packets <- pkt:
		default:
			e.droppedPackets.Add(1)
		}
	}
	for {
		var ev C.mockingbird_event_t
		if Status(C.mockingbird_poll_event(c, &ev)) != StatusOK {
			break
		}
		select {
		case e.events <- eventFromC(ev):
		default:
			e.droppedEvents.Add(1)
		}
	}
}
