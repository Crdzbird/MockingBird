package main

import (
	"fmt"
	"os"

	"github.com/Crdzbird/MockingBird/bindings/go"
)

func printEvent(ev mockingbird.Event, labels []string) {
	switch ev.Type {
	case mockingbird.EventSpeechBegin:
		fmt.Println("voice   ▶ speech begin")
	case mockingbird.EventSpeechEnd:
		fmt.Println("voice   ■ speech end")
	case mockingbird.EventAnomaly:
		fmt.Printf("anomaly   %v conf=%.2f\n", ev.AnomalyReasons(), ev.Confidence)
	case mockingbird.EventSoundClass:
		class := ev.SoundClass()
		name := ""
		if class >= 0 && class < len(labels) {
			name = " " + labels[class]
		}
		fmt.Printf("sound   [%v]%s (class %d, conf %.2f)\n", ev.Category(), name, class, ev.Confidence)
	case mockingbird.EventDegradation:
		fmt.Printf("engine  degradation level %d (worker %.0f%% busy)\n",
			ev.DegradationLevel(), ev.BusyFraction()*100)
	case mockingbird.EventBackendError:
		fmt.Fprintf(os.Stderr, "backend error: %v\n", ev.BackendError())
	}
}
