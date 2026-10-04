// Converts mach absolute host time to microseconds.
#pragma once

#include <mach/mach_time.h>

#include <cstdint>

namespace auris {

class MachTimebase {
public:
    static const MachTimebase& instance() {
        static const MachTimebase tb;
        return tb;
    }

    uint64_t to_us(uint64_t host_time) const noexcept {
        if (!ready_ || info_.denom == 0) return 0;
        const __uint128_t ns = static_cast<__uint128_t>(host_time) * info_.numer / info_.denom;
        return static_cast<uint64_t>(ns / 1000ull);
    }

private:
    MachTimebase() { ready_ = mach_timebase_info(&info_) == KERN_SUCCESS; }

    mach_timebase_info_data_t info_{};
    bool                      ready_ = false;
};

} // namespace auris
