/* src/capture/miniaudio_impl.c
 *
 * The ONE translation unit that compiles miniaudio's implementation.
 * Every other source includes "miniaudio.h" without MINIAUDIO_IMPLEMENTATION.
 *
 * Keep this file minimal: no project headers, no project symbols. Anything
 * we want to disable or trim from miniaudio's massive surface goes here as
 * a #define before the include.
 */

/* Auris is a capture-only library; we do not need playback or any of
 * miniaudio's mixing/effect subsystems. Disabling them shrinks the binary
 * and reduces the per-platform link surface (no AVFoundation playback hooks,
 * no decoders, etc.). */
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE

/* Capture is what we want. Playback can stay enabled (some backends require
 * the playback path to be reachable for shared init); revisit if a target
 * platform complains. */

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
