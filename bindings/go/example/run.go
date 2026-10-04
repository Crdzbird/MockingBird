package main

import (
	"fmt"
	"os"
	"os/signal"
	"path/filepath"
	"time"

	"github.com/auris-audio/auris"
)

type tally struct{ packets, bytes int }

func (t *tally) drain(e *auris.Engine, labels []string) {
	for {
		select {
		case ev := <-e.Events():
			printEvent(ev, labels)
		case p := <-e.Packets():
			t.packets++
			t.bytes += len(p.Data)
		default:
			return
		}
	}
}

func runMic(e *auris.Engine, seconds int, labels []string) {
	fmt.Println("listening on the default mic — Ctrl-C to stop")
	sig := make(chan os.Signal, 1)
	signal.Notify(sig, os.Interrupt)
	var deadline <-chan time.Time
	if seconds > 0 {
		deadline = time.After(time.Duration(seconds) * time.Second)
	}
	ticker := time.NewTicker(2 * time.Second)
	defer ticker.Stop()
	var t tally
	for {
		select {
		case ev := <-e.Events():
			printEvent(ev, labels)
		case p := <-e.Packets():
			t.packets++
			t.bytes += len(p.Data)
		case <-ticker.C:
			st, _ := e.Stats()
			fmt.Printf("   … %d frames · %d packets (%d B) · %d segments\n",
				st.FramesCaptured, t.packets, t.bytes, st.SegmentsOpened)
		case <-sig:
			report(e, t)
			return
		case <-deadline:
			report(e, t)
			return
		}
	}
}

func runFile(e *auris.Engine, path string, labels []string) {
	samples, rate, err := readWAV(path)
	if err != nil {
		fmt.Fprintln(os.Stderr, "wav:", err)
		os.Exit(1)
	}
	fmt.Printf("processing %s (%d samples @ %d Hz)\n", filepath.Base(path), len(samples), rate)
	frame := max(int(rate/1000)*20, 320)
	var t tally
	for off := 0; off+frame <= len(samples); off += frame {
		_ = e.PushPCM(samples[off:off+frame], uint64(off/frame)*20_000)
		if (off/frame)%16 == 15 {
			time.Sleep(15 * time.Millisecond)
			t.drain(e, labels)
		}
	}
	time.Sleep(250 * time.Millisecond)
	t.drain(e, labels)
	_ = e.Stop()
	t.drain(e, labels)
	report(e, t)
}

func report(e *auris.Engine, t tally) {
	st, _ := e.Stats()
	de, dp := e.Dropped()
	w := st.Worker
	fmt.Printf("\n── summary ──\n"+
		"frames captured : %d\n"+
		"segments        : %d opened / %d closed\n"+
		"packets to host : %d (%d bytes encoded)\n"+
		"native dropped  : %d frames, %d packets  |  host-side dropped: %d events, %d packets\n"+
		"worker          : %.2f%% busy, max frame %d µs, %d wakeups\n"+
		"work shed       : YAMNet %d ran / %d skipped, RNNoise skipped %d quiet frames, degradation level %d\n",
		st.FramesCaptured, st.SegmentsOpened, st.SegmentsClosed, t.packets, t.bytes,
		st.FramesDroppedCapture, st.PacketsDropped, de, dp,
		w.BusyFraction()*100, w.MaxFrameUs, w.Wakeups,
		w.SoundClassInferences, w.SoundClassInferencesSkipped, w.PreprocFramesSkipped, w.DegradationLevel)
}
