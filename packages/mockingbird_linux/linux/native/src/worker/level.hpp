// The input level of one frame, in dBFS.
#pragma once

namespace mockingbird {
struct Level {
    float rms_dbfs  = -120.0f;
    float peak_dbfs = -120.0f;
};

}
