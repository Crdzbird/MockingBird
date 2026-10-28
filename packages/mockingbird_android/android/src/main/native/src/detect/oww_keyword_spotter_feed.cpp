// The 80 ms step: audio into mel frames, the last 76 frames into one
// embedding, the latest embeddings into each keyword's score. Samples go to
// the models as int16 values in float, as openWakeWord feeds them, and mel
// values are rescaled to x / 10 + 2 the same way.
#include "detect/oww_keyword_spotter.hpp"

#ifdef MOCKINGBIRD_HAVE_SOUND_CLASS

#include <algorithm>
#include <array>
#include <cstring>

namespace mockingbird {
namespace {
Ort::Value tensor(float* data, size_t count, const int64_t* shape, size_t rank) {
    static const Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    return Ort::Value::CreateTensor<float>(mem, data, count, shape, rank);
}

std::vector<Ort::Value> run(Ort::Session& s, const std::string& in, const std::string& out, Ort::Value v) {
    const char* in_name  = in.c_str();
    const char* out_name = out.c_str();
    return s.Run(Ort::RunOptions{nullptr}, &in_name, &v, 1, &out_name, 1);
}

}

const std::vector<KeywordHit>& OwwKeywordSpotter::feed(const int16_t* frame, uint32_t n, bool listen) {
    hits_.clear();
    if (mel_ == nullptr || frame == nullptr) return hits_;
    for (uint32_t i = 0; i < n;) {
        const size_t take = std::min<size_t>(n - i, kChunk - pending_);
        for (size_t k = 0; k < take; ++k) raw_[kContext + pending_ + k] = static_cast<float>(frame[i + k]);
        pending_ += take;
        i += static_cast<uint32_t>(take);
        if (pending_ < kChunk) continue;
        try {
            run_chunk(listen);
        } catch (...) {
        }
        std::memmove(raw_.data(), raw_.data() + kChunk, kContext * sizeof(float));
        pending_ = 0;
    }
    return hits_;
}

void OwwKeywordSpotter::run_chunk(bool listen) {
    run_melspectrogram();
    lag_ = std::min(lag_ + 1, feat_rows_);
    for (Head& h : heads_) h.rest = std::max(0, h.rest - 1);
    if (!listen) return;
    for (; lag_ > 0; --lag_) run_embedding(lag_ - 1);
    run_heads();
}

void OwwKeywordSpotter::run_melspectrogram() {
    const std::array<int64_t, 2> shape{1, static_cast<int64_t>(raw_.size())};
    auto out = run(*mel_, mel_in_, mel_out_, tensor(raw_.data(), raw_.size(), shape.data(), 2));
    const size_t count  = out[0].GetTensorTypeAndShapeInfo().GetElementCount();
    const size_t frames = std::min(count / kMelBins, mel_rows_);
    const float* m      = out[0].GetTensorData<float>();
    const size_t keep   = (mel_rows_ - frames) * kMelBins;
    std::memmove(mels_.data(), mels_.data() + frames * kMelBins, keep * sizeof(float));
    for (size_t k = 0; k < frames * kMelBins; ++k) mels_[keep + k] = m[k] / 10.0f + 2.0f;
}

void OwwKeywordSpotter::run_embedding(size_t steps_back) {
    const std::array<int64_t, 4> shape{1, kMelWindow, kMelBins, 1};
    float* window = mels_.data() + (mel_rows_ - kMelWindow - kMelStep * steps_back) * kMelBins;
    auto out = run(*emb_, emb_in_, emb_out_, tensor(window, kMelWindow * kMelBins, shape.data(), 4));
    if (out[0].GetTensorTypeAndShapeInfo().GetElementCount() < kEmbedding) return;
    std::memmove(feats_.data(), feats_.data() + kEmbedding, (feats_.size() - kEmbedding) * sizeof(float));
    std::memcpy(feats_.data() + feats_.size() - kEmbedding, out[0].GetTensorData<float>(),
                kEmbedding * sizeof(float));
    filled_ = std::min(filled_ + 1, feat_rows_);
}

void OwwKeywordSpotter::run_heads() {
    for (uint32_t id = 0; id < heads_.size(); ++id) {
        Head& h = heads_[id];
        if (filled_ < h.frames) continue;
        float* last = feats_.data() + (feat_rows_ - h.frames) * kEmbedding;
        const std::array<int64_t, 3> shape{1, static_cast<int64_t>(h.frames), kEmbedding};
        auto out = run(*h.session, h.input, h.output, tensor(last, h.frames * kEmbedding, shape.data(), 3));
        score(h, id, *out[0].GetTensorData<float>());
    }
}

}

#endif
