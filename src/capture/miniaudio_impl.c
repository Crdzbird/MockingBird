/* The single translation unit that compiles miniaudio's implementation.
 * Capture and playback use the device API only, so the decoding, encoding,
 * generation and engine subsystems are compiled out. */
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
