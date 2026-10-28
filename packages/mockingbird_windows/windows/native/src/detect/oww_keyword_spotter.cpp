// Session setup: the two front-end models and one classifier per keyword,
// each with its tensor names and, for classifiers, how many embeddings they
// read. Any model that fails to load fails the whole spotter. Also the firing
// rule applied to each score.
#include "detect/oww_keyword_spotter.hpp"

#ifdef MOCKINGBIRD_HAVE_SOUND_CLASS

#include "mockingbird_log.hpp"
#include "detect/model_path.hpp"
#include "detect/ort_env.hpp"

#include <algorithm>

namespace mockingbird {
std::unique_ptr<Ort::Session> OwwKeywordSpotter::open(const std::string& path) {
    Ort::SessionOptions opts;
    opts.SetIntraOpNumThreads(1);
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
    apply_execution_provider(opts);
    MOCKINGBIRD_VLOG("keywords loading model: '%s'", path.c_str());
    const std::filesystem::path model = ort_model_path(path);
    return std::make_unique<Ort::Session>(shared_ort_env(), model.c_str(), opts);
}

std::string OwwKeywordSpotter::name_of(Ort::Session& s, bool input) {
    Ort::AllocatorWithDefaultOptions alloc;
    return input ? std::string(s.GetInputNameAllocated(0, alloc).get())
                 : std::string(s.GetOutputNameAllocated(0, alloc).get());
}

mockingbird_status_t OwwKeywordSpotter::init(const mockingbird_config_t& cfg, const KeywordSet& set) {
    if (mel_ != nullptr) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (cfg.sample_rate_hz != 16000 || set.keywords.empty()) return MOCKINGBIRD_ERR_UNSUPPORTED;
    try {
        mel_ = open(resolve_model_path(set.melspectrogram_path.c_str(),
                                       "MOCKINGBIRD_KEYWORD_MELSPECTROGRAM_MODEL", "melspectrogram.onnx"));
        emb_ = open(resolve_model_path(set.embedding_path.c_str(),
                                       "MOCKINGBIRD_KEYWORD_EMBEDDING_MODEL", "embedding_model.onnx"));
        mel_in_  = name_of(*mel_, true);
        mel_out_ = name_of(*mel_, false);
        emb_in_  = name_of(*emb_, true);
        emb_out_ = name_of(*emb_, false);
        for (const KeywordModel& k : set.keywords) {
            Head h;
            h.session   = open(k.path);
            h.input     = name_of(*h.session, true);
            h.output    = name_of(*h.session, false);
            h.threshold = k.threshold;
            const auto shape = h.session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
            if (shape.size() == 3 && shape[1] > 0 && shape[1] <= 120) h.frames = static_cast<size_t>(shape[1]);
            feat_rows_ = std::max(feat_rows_, h.frames);
            heads_.push_back(std::move(h));
        }
    } catch (const std::exception& ex) {
        MOCKINGBIRD_VLOG("keywords exception: %s", ex.what());
        heads_.clear();
        mel_.reset();
        emb_.reset();
        return MOCKINGBIRD_ERR_UNSUPPORTED;
    }
    raw_.assign(kChunk + kContext, 0.0f);
    mel_rows_ = kMelWindow + kMelStep * (feat_rows_ - 1);
    mels_.assign(mel_rows_ * kMelBins, 1.0f);
    feats_.assign(feat_rows_ * kEmbedding, 0.0f);
    hits_.reserve(heads_.size());
    return MOCKINGBIRD_OK;
}

void OwwKeywordSpotter::score(Head& h, uint32_t id, float p) {
    if (p < h.threshold) {
        h.armed = true;
        return;
    }
    if (!h.armed || h.rest > 0) return;
    h.armed = false;
    h.rest  = kRestChunks;
    hits_.push_back(KeywordHit{id, p});
}

std::unique_ptr<KeywordSpotter> make_keyword_spotter() { return std::make_unique<OwwKeywordSpotter>(); }

}

#else

namespace mockingbird {
std::unique_ptr<KeywordSpotter> make_keyword_spotter() { return nullptr; }
}

#endif
