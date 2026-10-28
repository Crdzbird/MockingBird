// Keyword-spotter interface: takes frames, returns the keywords whose score
// crossed their threshold on this frame. While listen is false the spotter
// may skip its expensive work, as long as it scores the same once it listens.
#pragma once

#include "mockingbird/mockingbird.h"
#include "detect/keyword_set.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace mockingbird {
struct KeywordHit {
    uint32_t id    = 0;
    float    score = 0.0f;
};

class KeywordSpotter {
public:
    virtual ~KeywordSpotter() = default;

    virtual mockingbird_status_t init(const mockingbird_config_t& cfg, const KeywordSet& set) = 0;

    virtual const std::vector<KeywordHit>& feed(const int16_t* frame, uint32_t n, bool listen) = 0;
};

std::unique_ptr<KeywordSpotter> make_keyword_spotter();

}
