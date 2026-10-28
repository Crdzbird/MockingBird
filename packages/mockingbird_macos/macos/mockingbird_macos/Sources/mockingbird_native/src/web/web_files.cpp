// Model files for the WebAssembly build: the page fetches a model and hands
// its bytes here, and the engine opens it from Emscripten's in-memory file
// system like a file on disk.
#ifdef __EMSCRIPTEN__

#include <emscripten/emscripten.h>

#include <cstdint>
#include <cstdio>
#include <sys/stat.h>

extern "C" {
EMSCRIPTEN_KEEPALIVE int mockingbird_web_store_file(const char* path, const uint8_t* data,
                                                    uint32_t len) {
    if (path == nullptr || (data == nullptr && len > 0)) return -1;
    mkdir("/models", 0755);
    FILE* f = std::fopen(path, "wb");
    if (f == nullptr) return -4;
    const size_t written = std::fwrite(data, 1, len, f);
    std::fclose(f);
    return written == len ? 0 : -4;
}

}

#endif
