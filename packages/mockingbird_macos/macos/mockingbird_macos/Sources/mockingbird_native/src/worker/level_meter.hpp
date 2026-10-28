// Measures a frame's RMS and peak before any processing, so a meter shows
// what the microphone received; floored at -120 dBFS for digital silence.
#pragma once

#include "worker/level.hpp"

#include <cmath>
#include <cstdint>

namespace mockingbird {
class LevelMeter {
public:
    static Level measure(const int16_t* samples, uint32_t n) {
        double  sum_sq = 0.0;
        int32_t peak   = 0;
        for (uint32_t i = 0; i < n; ++i) {
            const int32_t s = samples[i];
            sum_sq += static_cast<double>(s) * static_cast<double>(s);
            const int32_t a = (s < 0) ? -s : s;
            if (a > peak) peak = a;
        }
        Level level;
        level.rms_dbfs  = to_dbfs(std::sqrt(sum_sq / static_cast<double>(n)));
        level.peak_dbfs = to_dbfs(static_cast<double>(peak));
        return level;
    }

private:
    static float to_dbfs(double v) {
        constexpr float  kFloor     = -120.0f;
        constexpr double kFullScale = 32768.0;
        if (v <= 0.0) return kFloor;
        const double db = 20.0 * std::log10(v / kFullScale);
        return (db < static_cast<double>(kFloor)) ? kFloor : static_cast<float>(db);
    }
};

}
