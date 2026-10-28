// Turns miniaudio's per-backend device id union into the opaque string the
// C ABI hands out, and finds the device a string names again.
#pragma once

#include "miniaudio.h"

#include <string>

namespace mockingbird {
std::string miniaudio_encode_id(ma_backend backend, const ma_device_id& id);

bool miniaudio_find_capture_device(ma_context& ctx, const std::string& id, ma_device_id& out);

}
