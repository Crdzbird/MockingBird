// The ONNX Runtime side of SpeakerModel. Without ONNX Runtime in the build,
// load() reports MOCKINGBIRD_ERR_UNSUPPORTED.
#include "detect/speaker_model.hpp"

#include <cmath>

#ifdef MOCKINGBIRD_HAVE_SOUND_CLASS

#include "mockingbird_log.hpp"
#include "detect/kaldi_fbank.hpp"
#include "detect/model_path.hpp"
#include "detect/ort_env.hpp"

#include <array>

namespace mockingbird {
namespace {
class OrtSpeakerModel final : public SpeakerModel {
public:
    explicit OrtSpeakerModel(const std::string& path) {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(1);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        apply_execution_provider(opts);
        session_ = std::make_unique<Ort::Session>(shared_ort_env(), ort_model_path(path).c_str(), opts);
        Ort::AllocatorWithDefaultOptions alloc;
        input_  = session_->GetInputNameAllocated(0, alloc).get();
        output_ = session_->GetOutputNameAllocated(0, alloc).get();
    }

    std::vector<float> embed(const int16_t* pcm, size_t n) const override {
        if (n < kMinSamples) return {};
        std::vector<float> feats = fbank_.compute(pcm, n);
        const size_t frames = feats.size() / KaldiFbank::kBins;
        for (size_t b = 0; b < KaldiFbank::kBins; ++b) {
            double sum = 0.0;
            for (size_t f = 0; f < frames; ++f) sum += static_cast<double>(feats[f * KaldiFbank::kBins + b]);
            const auto mean = static_cast<float>(sum / static_cast<double>(frames));
            for (size_t f = 0; f < frames; ++f) feats[f * KaldiFbank::kBins + b] -= mean;
        }
        static const Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        const std::array<int64_t, 3> shape{1, static_cast<int64_t>(frames), KaldiFbank::kBins};
        Ort::Value x = Ort::Value::CreateTensor<float>(mem, feats.data(), feats.size(), shape.data(), 3);
        const char* in  = input_.c_str();
        const char* out = output_.c_str();
        auto y = session_->Run(Ort::RunOptions{nullptr}, &in, &x, 1, &out, 1);
        const float* v = y[0].GetTensorData<float>();
        std::vector<float> e(v, v + y[0].GetTensorTypeAndShapeInfo().GetElementCount());
        double norm = 0.0;
        for (float a : e) norm += static_cast<double>(a) * static_cast<double>(a);
        for (float& a : e) a = static_cast<float>(static_cast<double>(a) / std::sqrt(norm > 0.0 ? norm : 1.0));
        return e;
    }

private:
    KaldiFbank                    fbank_;
    std::unique_ptr<Ort::Session> session_;
    std::string                   input_, output_;
};

}

std::shared_ptr<SpeakerModel> SpeakerModel::load(const std::string& path, mockingbird_status_t& status) {
    try {
        const std::string p = resolve_model_path(path.c_str(), "MOCKINGBIRD_SPEAKER_MODEL", "speaker_campplus.onnx");
        MOCKINGBIRD_VLOG("speaker loading model: '%s'", p.c_str());
        auto m = std::make_shared<OrtSpeakerModel>(p);
        status = MOCKINGBIRD_OK;
        return m;
    } catch (const std::exception& ex) {
        MOCKINGBIRD_VLOG("speaker exception: %s", ex.what());
    }
    status = MOCKINGBIRD_ERR_UNSUPPORTED;
    return nullptr;
}

#else

namespace mockingbird {
std::shared_ptr<SpeakerModel> SpeakerModel::load(const std::string&, mockingbird_status_t& status) {
    status = MOCKINGBIRD_ERR_UNSUPPORTED;
    return nullptr;
}

#endif

float cosine(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.empty() || a.size() != b.size()) return 0.0f;
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        const auto x = static_cast<double>(a[i]), y = static_cast<double>(b[i]);
        dot += x * y;
        na += x * x;
        nb += y * y;
    }
    return na > 0.0 && nb > 0.0 ? static_cast<float>(dot / std::sqrt(na * nb)) : 0.0f;
}

}
