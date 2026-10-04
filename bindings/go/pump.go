package auris

/*
#include "auris/auris.h"
*/
import "C"

import "time"

const pollInterval = 2 * time.Millisecond

func (e *Engine) pump(c *C.auris_engine_t, done chan struct{}) {
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

func (e *Engine) drainOnce(c *C.auris_engine_t) {
	for {
		var p C.auris_packet_t
		if Status(C.auris_drain(c, &p)) != StatusOK {
			break
		}
		pkt := packetFromC(p)
		C.auris_release(c, &p)
		select {
		case e.packets <- pkt:
		default:
			e.droppedPackets.Add(1)
		}
	}
	for {
		var ev C.auris_event_t
		if Status(C.auris_poll_event(c, &ev)) != StatusOK {
			break
		}
		select {
		case e.events <- eventFromC(ev):
		default:
			e.droppedEvents.Add(1)
		}
	}
}
