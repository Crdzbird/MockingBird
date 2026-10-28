// Lists capture devices through a short-lived miniaudio context.
#include "capture/miniaudio_backend.hpp"

#include "capture/miniaudio_ids.hpp"

namespace mockingbird {
mockingbird_status_t miniaudio_list_input_devices(std::vector<InputDevice>& out) {
    out.clear();
    ma_context ctx;
    if (ma_context_init(nullptr, 0, nullptr, &ctx) != MA_SUCCESS) return MOCKINGBIRD_ERR_BACKEND;
    ma_device_info* playback = nullptr;
    ma_device_info* capture  = nullptr;
    ma_uint32 n_playback = 0;
    ma_uint32 n_capture  = 0;
    const ma_result r = ma_context_get_devices(&ctx, &playback, &n_playback, &capture, &n_capture);
    if (r == MA_SUCCESS) {
        for (ma_uint32 i = 0; i < n_capture; ++i) {
            out.push_back({miniaudio_encode_id(ctx.backend, capture[i].id), capture[i].name,
                           capture[i].isDefault != MA_FALSE});
        }
    }
    ma_context_uninit(&ctx);
    return r == MA_SUCCESS ? MOCKINGBIRD_OK : MOCKINGBIRD_ERR_BACKEND;
}

}
