// Kaldi-compatible log mel filterbank, as kaldi-native-fbank computes it for
// sherpa-onnx's speaker models: 25 ms Povey windows every 10 ms at 16 kHz,
// DC removed, pre-emphasis 0.97, a 512-point power spectrum, 80 triangular
// mel bins from 20 to 7600 Hz, and frames centred on each hop with the
// signal mirrored at both ends. compute() may run on several threads at once.
#pragma once

#include "pffft.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mockingbird {
class KaldiFbank {
public:
    static constexpr size_t kBins   = 80;
    static constexpr size_t kLength = 400;
    static constexpr size_t kShift  = 160;
    static constexpr size_t kFft    = 512;

    KaldiFbank();
    ~KaldiFbank();
    KaldiFbank(const KaldiFbank&)            = delete;
    KaldiFbank& operator=(const KaldiFbank&) = delete;

    std::vector<float> compute(const int16_t* pcm, size_t n) const;

private:
    struct Bin {
        size_t             first = 0;
        std::vector<float> weights;
    };

    void frame_into(const int16_t* pcm, size_t n, size_t f, float* padded) const;

    PFFFT_Setup*       setup_ = nullptr;
    std::vector<float> window_;
    std::vector<Bin>   bins_;
};

}
