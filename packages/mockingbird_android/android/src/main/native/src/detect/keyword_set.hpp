// The keywords an engine listens for and the front-end models they share,
// set through the C API before start and read by the worker when it starts.
#pragma once

#include <string>
#include <vector>

namespace mockingbird {
struct KeywordModel {
    std::string path;
    float       threshold = 0.5f;
};

struct KeywordSet {
    std::string               melspectrogram_path;
    std::string               embedding_path;
    std::vector<KeywordModel> keywords;
};

}
