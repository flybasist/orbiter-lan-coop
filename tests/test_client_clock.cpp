// Client display clock tests: running between packets, pause, smooth adjustment, jump.

#include <cmath>

#include "check.h"
#include "client_clock.h"

namespace {

bool near(double a, double b) {
    return std::fabs(a - b) < 1e-9;
}

// The first packet sets display time to hostSimTime - delay × warp and does not count as a jump.
void testFirstPacket() {
    olc::ClientClock clock(0.1);
    CHECK(!clock.started());
    CHECK(near(clock.displayTime(5.0), 0.0));  // before the first packet - 0

    clock.onPacket(1000.0, 1.0, false, 5.0);
    CHECK(clock.started());
    CHECK(clock.jumps() == 0);
    CHECK(near(clock.displayTime(5.0), 999.9));

    olc::ClientClock fast(0.1);
    fast.onPacket(1000.0, 10.0, false, 5.0);
    CHECK(near(fast.displayTime(5.0), 999.0));  // 1000 - 0.1 × 10
}

// Between packets time runs with the host warp.
void testRunsWithWarp() {
    olc::ClientClock clock(0.1);
    clock.onPacket(1000.0, 5.0, false, 10.0);
    double start = clock.displayTime(10.0);
    CHECK(near(start, 999.5));
    CHECK(near(clock.displayTime(12.0), start + 10.0));  // 2 s × 5
}

// While paused, time stands still.
void testPause() {
    olc::ClientClock clock(0.1);
    clock.onPacket(1000.0, 1.0, true, 10.0);
    double start = clock.displayTime(10.0);
    CHECK(near(clock.displayTime(13.0), start));
}

// A small error is corrected smoothly: it shrinks but does not vanish at once.
void testSmoothCorrection() {
    olc::ClientClock clock(0.1);
    clock.onPacket(1000.0, 1.0, false, 10.0);
    // A second later the host is actually 0.3 s further than the clock runs.
    double now = 11.0;
    double before = clock.displayTime(now);       // 1000.9
    double target = 1001.3 - 0.1;                 // 1001.2
    double errorBefore = target - before;         // 0.3
    clock.onPacket(1001.3, 1.0, false, now);
    double after = clock.displayTime(now);
    double errorAfter = target - after;
    CHECK(errorAfter < errorBefore);
    CHECK(errorAfter > 0.0);
    CHECK(near(errorAfter, errorBefore * (1.0 - olc::kClockGain)));
    CHECK(clock.jumps() == 0);

    // Many packets in a row bring the error almost to zero.
    for (int i = 0; i < 300; i++) {
        clock.onPacket(1001.3, 1.0, true, now);  // the host stands still: the target does not change
    }
    CHECK(std::fabs(clock.displayTime(now) - target) < 1e-6);
}

// A large error is corrected with a jump; jumps are counted.
void testJump() {
    olc::ClientClock clock(0.1);
    clock.onPacket(1000.0, 1.0, false, 10.0);
    clock.onPacket(5000.0, 1.0, false, 10.0);  // the host moved its time
    CHECK(clock.jumps() == 1);
    CHECK(near(clock.displayTime(10.0), 4999.9));
    clock.onPacket(100.0, 1.0, false, 10.0);
    CHECK(clock.jumps() == 2);
    CHECK(near(clock.displayTime(10.0), 99.9));
}

// The jump threshold grows with warp: at warp 100 an error of 50 s is not yet a jump.
void testJumpThresholdScalesWithWarp() {
    olc::ClientClock clock(0.1);
    clock.onPacket(1000.0, 100.0, false, 10.0);
    clock.onPacket(1050.0, 100.0, true, 10.0);  // error 50 < 1 × 100
    CHECK(clock.jumps() == 0);
    clock.onPacket(1300.0, 100.0, true, 10.0);  // error above the threshold
    CHECK(clock.jumps() == 1);
}

// Real time going backwards does not move display time.
void testNowGoesBack() {
    olc::ClientClock clock(0.1);
    clock.onPacket(1000.0, 1.0, false, 10.0);
    double at12 = clock.displayTime(12.0);
    CHECK(near(clock.displayTime(11.0), at12));
    // After the "rollback" running continues from the largest now.
    CHECK(near(clock.displayTime(13.0), at12 + 1.0));
}

}  // namespace

int main() {
    testFirstPacket();
    testRunsWithWarp();
    testPause();
    testSmoothCorrection();
    testJump();
    testJumpThresholdScalesWithWarp();
    testNowGoesBack();
    return checkResult();
}
