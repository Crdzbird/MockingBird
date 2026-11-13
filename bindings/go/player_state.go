// PlayerState: where a Player is in its lifecycle, as its stats report it.

package mockingbird

// PlayerState is a player's lifecycle state.
type PlayerState int

const (
	// PlayerStateStopped is before Start or after Stop.
	PlayerStateStopped PlayerState = iota
	// PlayerStateIdle is started with nothing to play.
	PlayerStateIdle
	// PlayerStateBuffering is waiting for the target delay of audio.
	PlayerStateBuffering
	// PlayerStatePlaying is playing packets.
	PlayerStatePlaying
	// PlayerStatePaused is paused by Pause or by the route going away.
	PlayerStatePaused
	// PlayerStateInterrupted is the OS having taken the output away.
	PlayerStateInterrupted
)

// String names the state.
func (s PlayerState) String() string {
	switch s {
	case PlayerStateStopped:
		return "stopped"
	case PlayerStateIdle:
		return "idle"
	case PlayerStateBuffering:
		return "buffering"
	case PlayerStatePlaying:
		return "playing"
	case PlayerStatePaused:
		return "paused"
	case PlayerStateInterrupted:
		return "interrupted"
	}
	return "unknown"
}
