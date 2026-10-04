// Command auris-demo runs the Auris engine on the microphone or a WAV file
// with any combination of detectors, denoise and codec, prints detections,
// and reports the engine's counters.
//
//	go run .                                   # default mic, fast VAD + anomaly
//	go run . -preset everything                # neural VAD + anomaly + YAMNet + RNNoise
//	go run . -file ../../../tests/fixtures/hello.wav -vad neural -codec opus
//	go run . -sensitivity 0.8 -soundclass on   # one dial, everything eager
package main

import (
	"flag"
	"fmt"
	"os"

	"github.com/auris-audio/auris"
)

func main() {
	var f flags
	flag.StringVar(&f.preset, "preset", "", "voice | everything | anomaly | denoised-voice | music")
	flag.StringVar(&f.file, "file", "", "16-bit mono WAV to process instead of the mic")
	flag.StringVar(&f.vad, "vad", "", "off | fast | neural")
	flag.StringVar(&f.anomaly, "anomaly", "", "on | off")
	flag.StringVar(&f.sound, "soundclass", "", "on | off")
	flag.StringVar(&f.denoise, "rnnoise", "", "on | off")
	flag.StringVar(&f.codec, "codec", "", "pcm | opus")
	flag.Float64Var(&f.sensitivity, "sensitivity", -1, "0..1 dial driving every detector")
	flag.StringVar(&f.silero, "neural-model", "", "path to silero_vad.onnx")
	flag.StringVar(&f.yamnet, "yamnet-model", "", "path to yamnet_3s.onnx")
	flag.IntVar(&f.seconds, "seconds", 0, "mic capture seconds; 0 runs until Ctrl-C")
	flag.Float64Var(&f.gate, "gate", 0, "silence gate in dBFS; 0 keeps the default")
	flag.Parse()

	cfg := f.config()
	fmt.Printf("Auris %s — %s\n", auris.Version(), describe(cfg, f.preset))
	e, err := auris.New(cfg)
	if err != nil {
		fmt.Fprintln(os.Stderr, "create:", err)
		os.Exit(1)
	}
	defer func() {
		if err := e.Close(); err != nil {
			fmt.Fprintln(os.Stderr, "close:", err)
		}
	}()
	if err := e.Start(); err != nil {
		fmt.Fprintln(os.Stderr, "start:", err)
		os.Exit(1)
	}
	labels := loadLabels()
	if f.file != "" {
		runFile(e, f.file, labels)
	} else {
		runMic(e, f.seconds, labels)
	}
}
