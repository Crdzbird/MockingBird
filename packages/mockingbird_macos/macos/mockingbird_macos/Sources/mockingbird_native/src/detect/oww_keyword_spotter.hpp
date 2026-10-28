// openWakeWord-compatible keyword spotting over ONNX Runtime. Every 80 ms
// the mel spectrogram model turns the last 110 ms of audio into eight frames,
// the speech embedding model turns the last 76 frames into one embedding, and
// each keyword's classifier scores the most recent embeddings. A keyword
// fires when its score crosses the threshold, then rests for one second and
// until the score falls back below it. The mel model always runs; while the
// engine is not listening the embeddings wait, and on listening again the
// ones the classifiers need are computed from the kept mel frames, so the
// scores are the same as if they had run throughout.
#pragma once

#include "detect/keyword_spotter.hpp"

#ifdef MOCKINGBIRD_HAVE_SOUND_CLASS

#include "detect/ort_headers.hpp"

#include <string>

namespace mockingbird {
class OwwKeywordSpotter final : public KeywordSpotter {
public:
    static constexpr size_t kChunk       = 1280;
    static constexpr size_t kContext     = 480;
    static constexpr size_t kMelBins     = 32;
    static constexpr size_t kMelWindow   = 76;
    static constexpr size_t kEmbedding   = 96;
    static constexpr size_t kMelStep     = 8;
    static constexpr int    kRestChunks  = 12;

    mockingbird_status_t             init(const mockingbird_config_t& cfg, const KeywordSet& set) override;
    const std::vector<KeywordHit>& feed(const int16_t* frame, uint32_t n, bool listen) override;

private:
    struct Head {
        std::unique_ptr<Ort::Session> session;
        std::string                   input;
        std::string                   output;
        size_t                        frames    = 16;
        float                         threshold = 0.5f;
        int                           rest      = 0;
        bool                          armed     = true;
    };

    static std::unique_ptr<Ort::Session> open(const std::string& path);
    static std::string                   name_of(Ort::Session& s, bool input);
    void run_chunk(bool listen);
    void run_melspectrogram();
    void run_embedding(size_t steps_back);
    void run_heads();
    void score(Head& h, uint32_t id, float p);

    std::unique_ptr<Ort::Session> mel_;
    std::unique_ptr<Ort::Session> emb_;
    std::vector<Head>             heads_;
    std::string                   mel_in_, mel_out_, emb_in_, emb_out_;
    std::vector<float>            raw_;
    std::vector<float>            mels_;
    std::vector<float>            feats_;
    std::vector<KeywordHit>       hits_;
    size_t                        pending_   = 0;
    size_t                        feat_rows_ = 0;
    size_t                        filled_    = 0;
    size_t                        mel_rows_  = 0;
    size_t                        lag_       = 0;
};

}

#endif
