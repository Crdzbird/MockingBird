// Owns the optional preprocessor, the two frame detectors and the sound
// classifier, and runs them in the fixed per-frame order.
#pragma once

#include "detect/detector.hpp"
#include "detect/keyword_spotter.hpp"
#include "detect/sound_classifier.hpp"
#include "preprocess/preprocessor.hpp"
#include "worker/detection_pair.hpp"

#include <cstdint>
#include <memory>

struct mockingbird_engine;

namespace mockingbird {
class Tunables;

class DetectorBank {
public:
    void init(mockingbird_engine* e);

    void denoise(mockingbird_engine* e, int16_t* frame, uint32_t n, bool quiet);

    DetectionPair detect(const int16_t* frame, uint32_t n, const Tunables& t);

    void classify(mockingbird_engine* e, const int16_t* frame, uint32_t n, bool quiet,
                  const Tunables& t, uint64_t segment_id, uint64_t now_ts);

    void spot(mockingbird_engine* e, const int16_t* frame, uint32_t n, bool quiet,
              const DetectionPair& d, uint64_t segment_id, uint64_t now_ts);

    Detector* vad() { return vad_.get(); }
    Detector* anomaly() { return anomaly_.get(); }

private:
    void init_spotter(mockingbird_engine* e);

    template <typename Component>
    static bool prepare(mockingbird_engine* e, std::unique_ptr<Component>& c, const char* tag);

    std::unique_ptr<Preprocessor>    preproc_;
    std::unique_ptr<Detector>        vad_;
    std::unique_ptr<Detector>        anomaly_;
    std::unique_ptr<SoundClassifier> classifier_;
    std::unique_ptr<KeywordSpotter>  spotter_;
    uint32_t                         voice_idle_frames_ = UINT32_MAX;
    uint32_t                         listen_frames_     = 0;
};

}
