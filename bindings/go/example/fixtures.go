package main

import (
	"encoding/binary"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"strings"
)

func locateFixture(name string) string {
	_, self, _, ok := runtime.Caller(0)
	if !ok {
		return ""
	}
	p := filepath.Join(filepath.Dir(self), "..", "..", "..", "tests", "fixtures", name)
	if _, err := os.Stat(p); err != nil {
		return ""
	}
	return p
}

func loadLabels() []string {
	b, err := os.ReadFile(locateFixture("yamnet_class_map.txt"))
	if err != nil {
		return nil
	}
	var out []string
	for line := range strings.SplitSeq(string(b), "\n") {
		if line = strings.TrimSpace(line); line == "" {
			continue
		}
		if _, label, found := strings.Cut(line, " "); found {
			line = label
		}
		out = append(out, line)
	}
	return out
}

func readWAV(path string) (samples []int16, rate uint32, err error) {
	b, err := os.ReadFile(path)
	if err != nil {
		return nil, 0, err
	}
	if len(b) < 12 || string(b[0:4]) != "RIFF" || string(b[8:12]) != "WAVE" {
		return nil, 0, fmt.Errorf("not a WAV file")
	}
	for i := 12; i+8 <= len(b); {
		id := string(b[i : i+4])
		sz := int(binary.LittleEndian.Uint32(b[i+4 : i+8]))
		body := b[i+8:]
		if len(body) < sz {
			break
		}
		switch id {
		case "fmt ":
			rate = binary.LittleEndian.Uint32(body[4:8])
		case "data":
			samples = make([]int16, sz/2)
			for k := range samples {
				samples[k] = int16(binary.LittleEndian.Uint16(body[k*2 : k*2+2]))
			}
			return samples, rate, nil
		}
		i += 8 + sz + sz%2
	}
	return nil, 0, fmt.Errorf("no data chunk")
}
