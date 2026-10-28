// A speaker embedding model over ONNX Runtime: 16 kHz speech in, a
// unit-length voice print out. The input is the Kaldi filterbank with each
// bin's mean over the utterance removed, the layout 3D-Speaker's exports in
// sherpa-onnx's format expect. embed() may run on several threads at once.
#pragma once

#include "mockingbird/mockingbird.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mockingbird {
class SpeakerModel {
public:
    static constexpr size_t kMinSamples = 16000 / 2;

    static std::shared_ptr<SpeakerModel> load(const std::string& path, mockingbird_status_t& status);

    virtual ~SpeakerModel() = default;

    virtual std::vector<float> embed(const int16_t* pcm, size_t n) const = 0;
};

float cosine(const std::vector<float>& a, const std::vector<float>& b);

}
