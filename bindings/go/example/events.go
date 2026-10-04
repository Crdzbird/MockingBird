package main

import (
	"fmt"
	"os"

	"github.com/auris-audio/auris"
)

func printEvent(ev auris.Event, labels []string) {
	switch ev.Type {
	case auris.EventSpeechBegin:
		fmt.Println("voice   ▶ speech begin")
	case auris.EventSpeechEnd:
		fmt.Println("voice   ■ speech end")
	case auris.EventAnomaly:
		fmt.Printf("anomaly   %v conf=%.2f\n", ev.AnomalyReasons(), ev.Confidence)
	case auris.EventSoundClass:
		class := ev.SoundClass()
		name := ""
		if class >= 0 && class < len(labels) {
			name = " " + labels[class]
		}
		fmt.Printf("sound   [%v]%s (class %d, conf %.2f)\n", ev.Category(), name, class, ev.Confidence)
	case auris.EventDegradation:
		fmt.Printf("engine  degradation level %d (worker %.0f%% busy)\n",
			ev.DegradationLevel(), ev.BusyFraction()*100)
	case auris.EventBackendError:
		fmt.Fprintf(os.Stderr, "backend error: %v\n", ev.BackendError())
	}
}
