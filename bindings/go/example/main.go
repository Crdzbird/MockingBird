// Command auris-demo captures from the default microphone and prints
// detections — voice, anomalies, and (when the neural models are wired) YAMNet
// sound classes — until interrupted with Ctrl-C.
//
//	go run ./example
package main

import (
	"fmt"
	"os"
	"os/signal"
	"time"

	"github.com/auris-audio/auris"
)

func main() {
	cfg := auris.DefaultConfig()
	cfg.Backend = auris.BackendAuto // use the OS default microphone
	cfg.EnableVAD = true            // multilingual voice activity
	cfg.EnableAnomaly = true        // sudden / unusual sounds

	// To enable multilingual NEURAL voice detection + YAMNet sound
	// classification, build libauris with ONNX Runtime and point at the models:
	//
	//   cfg.VadTier = auris.VadNeural
	//   cfg.NeuralVADModelPath = "silero_vad.onnx"
	//   cfg.EnableSoundClass = true
	//   cfg.SoundClassModelPath = "yamnet_3s.onnx"
	//
	// or simply: cfg = auris.Sensitive(0.6,
	//     auris.WithNeuralVADModel("silero_vad.onnx"),
	//     auris.WithSoundClassModel("yamnet_3s.onnx"))

	e, err := auris.New(cfg)
	if err != nil {
		fmt.Fprintln(os.Stderr, "create:", err)
		os.Exit(1)
	}
	defer e.Close()
	if err := e.Start(); err != nil {
		fmt.Fprintln(os.Stderr, "start:", err)
		os.Exit(1)
	}

	fmt.Printf("Auris %s — listening on the default mic. Ctrl-C to stop.\n", auris.Version())

	sig := make(chan os.Signal, 1)
	signal.Notify(sig, os.Interrupt)

	ticker := time.NewTicker(2 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case ev := <-e.Events():
			switch ev.Type {
			case auris.EventSpeechBegin:
				fmt.Println("voice  ▶ speech begin")
			case auris.EventSpeechEnd:
				fmt.Println("voice  ■ speech end")
			case auris.EventAnomaly:
				fmt.Printf("anomaly  reason=%d conf=%.2f\n", ev.ReasonMask, ev.Confidence)
			case auris.EventSoundClass:
				cls := ev.SoundClass()
				fmt.Printf("sound  [%s] class=%d conf=%.2f\n",
					auris.CategoryForYAMNetClass(cls), cls, ev.Confidence)
			case auris.EventOverrun:
				fmt.Println("(overrun — host fell behind)")
			case auris.EventBackendError:
				fmt.Fprintln(os.Stderr, "backend error:", ev.BackendError())
			}
		case <-e.Packets():
			// In a real app: forward/persist the encoded audio, then it's
			// already released. Here we just discard it.
		case <-ticker.C:
			st, _ := e.Stats()
			fmt.Printf("   … %d frames · %d packets · %d segments\n",
				st.FramesCaptured, st.PacketsEmitted, st.SegmentsOpened)
		case <-sig:
			fmt.Println("\nstopping…")
			_ = e.Stop()
			return
		}
	}
}
