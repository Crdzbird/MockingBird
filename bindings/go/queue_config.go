package auris

// QueueConfig sizes the queues between capture, the worker and the host.
type QueueConfig struct {
	// PCMRingFrames is the capture ring capacity in samples.
	PCMRingFrames uint32
	// PacketQueueCapacity is the host packet queue depth; oldest drops first.
	PacketQueueCapacity uint32
	// PCMTap also exposes every processed frame, ungated, via Engine.ReadPCM.
	PCMTap bool
	// PCMTapFrames is the tap ring capacity; 0 is one second of audio.
	PCMTapFrames uint32
}

// DefaultQueueConfig holds about a second of PCM and 256 packets.
func DefaultQueueConfig() QueueConfig {
	return QueueConfig{PCMRingFrames: 1 << 14, PacketQueueCapacity: 256}
}
