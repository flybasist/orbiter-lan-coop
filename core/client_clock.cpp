#include "client_clock.h"

#include <algorithm>
#include <cmath>

namespace olc {

ClientClock::ClientClock(double displayDelay) : displayDelay_(displayDelay) {}

void ClientClock::advance(double now) {
    if (!started_) {
        // The clock is not running yet: only remember the moment counting starts from.
        lastNow_ = now;
        return;
    }
    if (now > lastNow_) {
        displayTime_ += (now - lastNow_) * rate_;
        lastNow_ = now;
    }
    // If now went backwards, display time and lastNow_ are left unchanged.
}

void ClientClock::onPacket(double hostSimTime, double warp, bool paused, double now) {
    advance(now);

    double target = hostSimTime - displayDelay_ * warp;
    if (!started_) {
        // First packet: set the clock at once; this is not counted as a jump.
        started_ = true;
        displayTime_ = target;
    } else if (std::fabs(target - displayTime_) > kClockJumpThreshold * std::max(1.0, warp)) {
        displayTime_ = target;
        jumps_++;
    } else {
        displayTime_ += (target - displayTime_) * kClockGain;
    }
    rate_ = paused ? 0.0 : warp;
}

double ClientClock::displayTime(double now) {
    advance(now);
    return displayTime_;
}

}  // namespace olc
