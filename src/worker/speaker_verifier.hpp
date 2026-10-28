// Speaker verification on the worker: while a target voice print is set, it
// keeps the audio of each utterance, from SPEECH_BEGIN to SPEECH_END and at
// most the first 8 s, and compares the utterance's voice print with the
// target. Each utterance of at least half a second gets one
// MOCKINGBIRD_EVT_SPEAKER, sent when it ends or reaches 8 s.
#pragma once

#include <cstdint>
#include <vector>

struct mockingbird_engine;

namespace mockingbird {
class SpeakerVerifier {
public:
    static constexpr size_t kMaxSamples = 16000 * 8;

    void on_frame(mockingbird_engine* e, const int16_t* frame, uint32_t n, bool speech_open,
                  uint64_t segment_id, uint64_t now_ts);

private:
    void verdict(mockingbird_engine* e, uint64_t now_ts);

    std::vector<int16_t> audio_;
    uint64_t             segment_id_ = 0;
    bool                 collecting_ = false;
    bool                 decided_    = false;
    bool                 failed_     = false;
};

}
