#include "client_clock.h"

#include <algorithm>
#include <cmath>

namespace olc {

ClientClock::ClientClock(double displayDelay) : displayDelay_(displayDelay) {}

void ClientClock::advance(double now) {
    if (!started_) {
        // Часы ещё не идут: запоминаем только момент, от которого пойдёт отсчёт.
        lastNow_ = now;
        return;
    }
    if (now > lastNow_) {
        displayTime_ += (now - lastNow_) * rate_;
        lastNow_ = now;
    }
    // Если now ушло назад, время показа не меняем и lastNow_ не трогаем.
}

void ClientClock::onPacket(double hostSimTime, double warp, bool paused, double now) {
    advance(now);

    double target = hostSimTime - displayDelay_ * warp;
    if (!started_) {
        // Первый пакет: ставим часы сразу, прыжком его не считаем.
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
