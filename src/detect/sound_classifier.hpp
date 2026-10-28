// Sound-classifier interface: buffers audio and classifies on a cadence.
#pragma once

#include "mockingbird/mockingbird.h"
#include "detect/classification_result.hpp"
#include "detect/feed_hints.hpp"

#include <cstdint>
#include <memory>

namespace mockingbird {
class SoundClassifier {
public:
    virtual ~SoundClassifier() = default;

    virtual mockingbird_status_t init(const mockingbird_config_t& cfg) = 0;

    virtual ClassificationResult feed(const int16_t* frame, uint32_t n,
                                      const FeedHints& hints = FeedHints{}) = 0;
};

std::unique_ptr<SoundClassifier> make_sound_classifier();

}
