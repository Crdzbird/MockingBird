// The input level of one frame, in dBFS.
#pragma once

namespace auris {

struct Level {
    float rms_dbfs  = -120.0f;
    float peak_dbfs = -120.0f;
};

} // namespace auris
