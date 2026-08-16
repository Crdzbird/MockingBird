// Command conformance is the Go peer of the Auris cross-binding conformance
// harness. It exchanges deterministic audio with the Dart peer over TCP and
// proves that BOTH bindings, driving the same native libauris, turn identical
// input PCM into byte-identical packets, events, and stats — i.e. quality and
// information are the same and the two language surfaces stay in sync.
//
// Determinism contract (must match examples/conformance/dart/conformance.dart
// exactly):
//   - signal: integer-only xorshift64 generator (no floats), so the samples are
//     bit-identical across languages.
//   - config: HOST_PCM, 16 kHz mono, 20 ms frames, Opus, fast VAD + anomaly.
//   - processing: push every frame, Stop (flushes the tail), drain all.
//   - digest: FNV-1a/64 over a fixed big-endian canonical encoding of the
//     packets, events, and stats.
//
// Run the Go peer (listener), then the Dart peer (dialer):
//
//	go run .                       # listens on :9999
//	dart run ../dart/conformance.dart 127.0.0.1:9999
package main

import (
	"bytes"
	"encoding/binary"
	"flag"
	"fmt"
	"io"
	"math"
	"net"
	"os"

	"github.com/auris-audio/auris"
)

const (
	sampleRate = 16000
	frameLen   = 320   // 20 ms @ 16 kHz
	numSamples = 32000 // 2 s, 100 frames
	goSeed     = 0xA1A1A1A1A1A1A1A1
	dartSeed   = 0xB2B2B2B2B2B2B2B2
)

// --- deterministic signal (integer-only; identical to the Dart generator) ----

type xorshift uint64

func (x *xorshift) next() uint64 {
	v := uint64(*x)
	v ^= v << 13
	v ^= v >> 7
	v ^= v << 17
	*x = xorshift(v)
	return v
}

func genSignal(seed uint64) []int16 {
	rng := xorshift(seed)
	out := make([]int16, numSamples)
	for i := 0; i < numSamples; i++ {
		r := rng.next()
		v := int32((r>>40)&0x1FFF) - 4096 // noise in [-4096, 4095]
		if i%8000 < 1600 {                // 0.1 s loud burst every 0.5 s
			if (i>>3)&1 == 0 {
				v += 11000
			} else {
				v -= 11000
			}
		}
		if v > 32767 {
			v = 32767
		} else if v < -32768 {
			v = -32768
		}
		out[i] = int16(v)
	}
	return out
}

// --- processing + canonical digest -------------------------------------------

type result struct {
	digest            uint64
	nPackets, nEvents int
	nBytes            int
}

// fnv1a64 over a growable canonical byte stream.
type hasher struct{ h uint64 }

func newHasher() *hasher { return &hasher{h: 14695981039346656037} }
func (a *hasher) byte(b byte) {
	a.h ^= uint64(b)
	a.h *= 1099511628211
}
func (a *hasher) bytes(p []byte) {
	for _, b := range p {
		a.byte(b)
	}
}
func (a *hasher) u32(v uint32) {
	var b [4]byte
	binary.BigEndian.PutUint32(b[:], v)
	a.bytes(b[:])
}
func (a *hasher) u64(v uint64) {
	var b [8]byte
	binary.BigEndian.PutUint64(b[:], v)
	a.bytes(b[:])
}

func process(pcm []int16) (result, error) {
	cfg := auris.DefaultConfig()
	cfg.Backend = auris.BackendHostPCM
	cfg.Codec = auris.CodecOpus
	cfg.EnableVAD = true
	cfg.VadTier = auris.VadFast
	cfg.VadAggressiveness = 2
	cfg.EnableAnomaly = true
	cfg.EnableSoundClass = false
	cfg.EnableRNNoise = false
	cfg.PcmRingFrames = 1 << 17
	cfg.PacketQueueCapacity = 4096

	e, err := auris.New(cfg)
	if err != nil {
		return result{}, err
	}
	defer e.Close()
	if err := e.Start(); err != nil {
		return result{}, err
	}

	var packets []auris.Packet
	var events []auris.Event
	collect := func() {
		for {
			select {
			case p := <-e.Packets():
				packets = append(packets, p)
			case ev := <-e.Events():
				events = append(events, ev)
			default:
				return
			}
		}
	}

	for off := 0; off+frameLen <= len(pcm); off += frameLen {
		if err := e.PushPCM(pcm[off:off+frameLen], uint64(off/frameLen)*20_000); err != nil {
			return result{}, err
		}
		if (off/frameLen)%8 == 7 {
			collect()
		}
	}
	if err := e.Stop(); err != nil { // flushes the worker's tail
		return result{}, err
	}
	collect()

	stats, err := e.Stats()
	if err != nil {
		return result{}, err
	}

	if debug {
		for i, p := range packets {
			fmt.Printf("P %02d size=%d flags=%d seg=%d pts=%d cap=%d\n",
				i, len(p.Data), p.Flags, p.SegmentID, p.PtsUs, p.CaptureTsUs)
		}
		for i, ev := range events {
			fmt.Printf("E %02d type=%d reason=%d conf=%08x seg=%d ts=%d\n",
				i, int(ev.Type), ev.ReasonMask, math.Float32bits(ev.Confidence), ev.SegmentID, ev.TimestampUs)
		}
		fmt.Printf("S captured=%d dropped=%d emitted=%d pktDropped=%d segOpen=%d segClose=%d\n",
			stats.FramesCaptured, stats.FramesDroppedCapture, stats.PacketsEmitted,
			stats.PacketsDropped, stats.SegmentsOpened, stats.SegmentsClosed)
	}

	h := newHasher()
	h.u32(uint32(len(packets)))
	nBytes := 0
	for _, p := range packets {
		h.u64(p.SegmentID)
		h.u64(p.PtsUs)
		h.u64(p.CaptureTsUs)
		h.u32(p.Flags)
		h.u32(uint32(p.Codec))
		h.u32(uint32(len(p.Data)))
		h.bytes(p.Data)
		nBytes += len(p.Data)
	}
	h.u32(uint32(len(events)))
	for _, ev := range events {
		h.u32(uint32(ev.Type))
		h.u64(ev.SegmentID)
		h.u64(ev.TimestampUs)
		h.u32(ev.ReasonMask)
		h.u32(math.Float32bits(ev.Confidence))
	}
	h.u64(stats.FramesCaptured)
	h.u64(stats.PacketsEmitted)
	h.u64(stats.SegmentsOpened)
	h.u64(stats.SegmentsClosed)

	return result{digest: h.h, nPackets: len(packets), nEvents: len(events), nBytes: nBytes}, nil
}

// --- wire protocol -----------------------------------------------------------

func writeFrame(w io.Writer, payload []byte) error {
	var hdr [4]byte
	binary.BigEndian.PutUint32(hdr[:], uint32(len(payload)))
	if _, err := w.Write(hdr[:]); err != nil {
		return err
	}
	_, err := w.Write(payload)
	return err
}

func readFrame(r io.Reader) ([]byte, error) {
	var hdr [4]byte
	if _, err := io.ReadFull(r, hdr[:]); err != nil {
		return nil, err
	}
	n := binary.BigEndian.Uint32(hdr[:])
	buf := make([]byte, n)
	if _, err := io.ReadFull(r, buf); err != nil {
		return nil, err
	}
	return buf, nil
}

func pcmToBytes(s []int16) []byte {
	b := make([]byte, len(s)*2)
	for i, v := range s {
		binary.LittleEndian.PutUint16(b[i*2:], uint16(v))
	}
	return b
}

func bytesToPCM(b []byte) []int16 {
	s := make([]int16, len(b)/2)
	for i := range s {
		s[i] = int16(binary.LittleEndian.Uint16(b[i*2:]))
	}
	return s
}

func encodeResult(computed, expected result) []byte {
	b := make([]byte, 28)
	binary.BigEndian.PutUint64(b[0:], computed.digest)
	binary.BigEndian.PutUint64(b[8:], expected.digest)
	binary.BigEndian.PutUint32(b[16:], uint32(computed.nPackets))
	binary.BigEndian.PutUint32(b[20:], uint32(computed.nEvents))
	binary.BigEndian.PutUint32(b[24:], uint32(computed.nBytes))
	return b
}

func decodeResult(b []byte) (computedDigest, expectedDigest uint64, nPkts, nEvents, nBytes uint32) {
	return binary.BigEndian.Uint64(b[0:]), binary.BigEndian.Uint64(b[8:]),
		binary.BigEndian.Uint32(b[16:]), binary.BigEndian.Uint32(b[20:]), binary.BigEndian.Uint32(b[24:])
}

// --- peer logic --------------------------------------------------------------

// runPeer performs the symmetric exchange and returns true on full bidirectional
// parity.
func runPeer(conn net.Conn, name string, ownSeed, peerSeed uint64, peerName string) bool {
	own := genSignal(ownSeed)
	expected, err := process(own)
	must(err)
	fmt.Printf("[%s] generated %d samples (seed %#x); my engine: %d packets / %d events / %d bytes, digest %#016x\n",
		name, len(own), ownSeed, expected.nPackets, expected.nEvents, expected.nBytes, expected.digest)

	must(writeFrame(conn, pcmToBytes(own)))
	peerBytes, err := readFrame(conn)
	must(err)
	peerPCM := bytesToPCM(peerBytes)

	// Generator parity: the bytes the peer sent must equal what OUR generator
	// produces for the peer's seed — isolates a signal-generation divergence
	// from a processing divergence.
	if !bytes.Equal(peerBytes, pcmToBytes(genSignal(peerSeed))) {
		fmt.Printf("[%s] ❌ GENERATOR MISMATCH: %s's signal differs from our local regeneration\n", name, peerName)
		return false
	}
	fmt.Printf("[%s] received %d samples from %s; generator parity MATCH ✓ — processing through MY binding…\n",
		name, len(peerPCM), peerName)

	computed, err := process(peerPCM)
	must(err)
	fmt.Printf("[%s] my result for %s's audio: %d packets / %d events / %d bytes, digest %#016x\n",
		name, peerName, computed.nPackets, computed.nEvents, computed.nBytes, computed.digest)

	must(writeFrame(conn, encodeResult(computed, expected)))
	rb, err := readFrame(conn)
	must(err)
	peerComputedForMine, peerExpectedForTheirs, _, _, _ := decodeResult(rb)

	// Direction A: did the peer process MY audio identically to me?
	dirA := peerComputedForMine == expected.digest
	// Direction B: did I process the PEER's audio identically to the peer?
	dirB := computed.digest == peerExpectedForTheirs

	fmt.Printf("[%s] parity %s-audio→%s: %s   |   %s-audio→%s: %s\n",
		name, name, peerName, ok(dirA), peerName, name, ok(dirB))
	return dirA && dirB
}

var debug bool

func main() {
	addr := flag.String("addr", ":9999", "listen address")
	self := flag.Bool("self", false, "self-check determinism without networking")
	dump := flag.Bool("dump", false, "process the go-seed signal and dump per-packet/event detail")
	peer := flag.String("peer", "dart", "name of the dialing peer, for log labels only")
	flag.Parse()

	if *dump {
		debug = true
		_, err := process(genSignal(goSeed))
		must(err)
		return
	}

	if *self {
		a, err := process(genSignal(goSeed))
		must(err)
		b, err := process(genSignal(goSeed))
		must(err)
		fmt.Printf("[self] run1 digest %#016x, run2 digest %#016x — %s\n", a.digest, b.digest, ok(a.digest == b.digest))
		if a.digest != b.digest {
			os.Exit(1)
		}
		return
	}

	ln, err := net.Listen("tcp", *addr)
	must(err)
	fmt.Printf("[go] listening on %s — start the %s peer now\n", *addr, *peer)
	conn, err := ln.Accept()
	must(err)
	defer conn.Close()

	pass := runPeer(conn, "go", goSeed, dartSeed, *peer)
	if pass {
		fmt.Printf("\n✅ CONFORMANCE PASS — Go and %s produce byte-identical results both ways\n", *peer)
	} else {
		fmt.Println("\n❌ CONFORMANCE FAIL — divergence detected")
		os.Exit(1)
	}
}

func ok(b bool) string {
	if b {
		return "MATCH ✓"
	}
	return "MISMATCH ✗"
}

func must(err error) {
	if err != nil {
		fmt.Fprintln(os.Stderr, "fatal:", err)
		os.Exit(1)
	}
}
