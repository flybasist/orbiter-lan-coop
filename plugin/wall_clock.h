// Wall clock time for latency measurements (tasks 3.6 and 3.10).
//
// oapiGetSysTime is not the wall clock: Orbiter caps one frame step at 0.1 s
// (Orbiter::BeginTimeStep, docs/ORBITER-INTERNALS.md, section 4), so a frame of 0.3 s counts as
// 0.1 s there. Measurements that must see long frames use this clock instead.
#pragma once

#include <chrono>

namespace olcplugin {

// Seconds of a monotonic clock; only differences between two calls make sense.
inline double wallClock() {
    using std::chrono::duration;
    using std::chrono::steady_clock;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace olcplugin
