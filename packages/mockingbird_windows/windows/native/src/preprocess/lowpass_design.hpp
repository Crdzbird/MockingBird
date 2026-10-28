// Windowed-sinc (Hann) low-pass prototype used by the integer resamplers.
// fc is the cutoff in cycles per sample at the rate the filter runs at.
#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

namespace mockingbird {
inline std::vector<float> design_lowpass(size_t taps, double fc, double dc_gain) {
    std::vector<float> h(taps, 0.0f);
    const double center = static_cast<double>(taps - 1) / 2.0;
    const double pi = 3.14159265358979323846;
    double sum = 0.0;
    for (size_t n = 0; n < taps; ++n) {
        const double m = static_cast<double>(n) - center;
        const double sinc = (std::fabs(m) < 1e-9)
            ? 2.0 * fc
            : std::sin(2.0 * pi * fc * m) / (pi * m);
        const double w = 0.5 - 0.5 * std::cos(2.0 * pi * static_cast<double>(n) /
                                              static_cast<double>(taps - 1));
        const double v = sinc * w;
        h[n] = static_cast<float>(v);
        sum += v;
    }
    const double scale = (sum != 0.0) ? (dc_gain / sum) : 1.0;
    for (auto& c : h) c = static_cast<float>(static_cast<double>(c) * scale);
    return h;
}

}
