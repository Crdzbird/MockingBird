package mockingbird

import "encoding/binary"

// WAVHeaderBytes is the size of the header WAV places ahead of the samples.
const WAVHeaderBytes = 44

// WAV wraps 16-bit little-endian PCM in a RIFF WAVE header and returns the
// whole file. Concatenate one segment's CodecPCMS16LE payloads in drain order
// and pass them here to get a file any player opens.
func WAV(pcm []byte, sampleRateHz, channels int) []byte {
	out := make([]byte, WAVHeaderBytes+len(pcm))
	le := binary.LittleEndian
	copy(out[0:], "RIFF")
	le.PutUint32(out[4:], uint32(36+len(pcm)))
	copy(out[8:], "WAVEfmt ")
	le.PutUint32(out[16:], 16)
	le.PutUint16(out[20:], 1)
	le.PutUint16(out[22:], uint16(channels))
	le.PutUint32(out[24:], uint32(sampleRateHz))
	le.PutUint32(out[28:], uint32(sampleRateHz*channels*2))
	le.PutUint16(out[32:], uint16(channels*2))
	le.PutUint16(out[34:], 16)
	copy(out[36:], "data")
	le.PutUint32(out[40:], uint32(len(pcm)))
	copy(out[WAVHeaderBytes:], pcm)
	return out
}
