// One frame, in the fixed order every binding's conformance digest depends
// on: retune, meter, denoise, tap, detect, classify, segment, account.
#include "worker/worker.hpp"

#include "engine.hpp"
#include "worker/level_meter.hpp"

namespace mockingbird {

void Worker::process_frame(uint64_t now_ts) {
    health_.begin_frame();
    tunables_.refresh(e_, bank_.vad(), bank_.anomaly());

    const Level level = LevelMeter::measure(scratch_.data(), frame_samples_);
    const bool  quiet = level.rms_dbfs < tunables_.silence_gate_dbfs;
    e_->input_rms_dbfs.store(level.rms_dbfs, std::memory_order_relaxed);
    e_->input_peak_dbfs.store(level.peak_dbfs, std::memory_order_relaxed);

    bank_.denoise(e_, scratch_.data(), frame_samples_, quiet);
    tap(scratch_.data());

    const DetectionPair d = bank_.detect(scratch_.data(), frame_samples_, tunables_);
    bank_.classify(e_, scratch_.data(), frame_samples_, quiet, tunables_,
                   segmenter_.open_segment_id(), now_ts);
    segmenter_.on_frame(scratch_.data(), now_ts, d, tunables_.hangover_frames);

    health_.end_frame(e_, now_ts, tunables_);
}

} // namespace mockingbird
