// Тесты часов показа клиента: ход между пакетами, пауза, плавная подстройка, прыжок.

#include <cmath>

#include "check.h"
#include "client_clock.h"

namespace {

bool near(double a, double b) {
    return std::fabs(a - b) < 1e-9;
}

// Первый пакет ставит время показа на hostSimTime - задержка × ускорение и не считается прыжком.
void testFirstPacket() {
    olc::ClientClock clock(0.1);
    CHECK(!clock.started());
    CHECK(near(clock.displayTime(5.0), 0.0));  // до первого пакета - 0

    clock.onPacket(1000.0, 1.0, false, 5.0);
    CHECK(clock.started());
    CHECK(clock.jumps() == 0);
    CHECK(near(clock.displayTime(5.0), 999.9));

    olc::ClientClock fast(0.1);
    fast.onPacket(1000.0, 10.0, false, 5.0);
    CHECK(near(fast.displayTime(5.0), 999.0));  // 1000 - 0.1 × 10
}

// Между пакетами время идёт с ускорением хоста.
void testRunsWithWarp() {
    olc::ClientClock clock(0.1);
    clock.onPacket(1000.0, 5.0, false, 10.0);
    double start = clock.displayTime(10.0);
    CHECK(near(start, 999.5));
    CHECK(near(clock.displayTime(12.0), start + 10.0));  // 2 с × 5
}

// На паузе время стоит.
void testPause() {
    olc::ClientClock clock(0.1);
    clock.onPacket(1000.0, 1.0, true, 10.0);
    double start = clock.displayTime(10.0);
    CHECK(near(clock.displayTime(13.0), start));
}

// Небольшая ошибка исправляется плавно: уменьшается, но не пропадает сразу.
void testSmoothCorrection() {
    olc::ClientClock clock(0.1);
    clock.onPacket(1000.0, 1.0, false, 10.0);
    // Через секунду хост на самом деле ушёл на 0,3 с дальше, чем идут часы.
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

    // Много пакетов подряд сводят ошибку почти к нулю.
    for (int i = 0; i < 300; i++) {
        clock.onPacket(1001.3, 1.0, true, now);  // хост стоит: цель не меняется
    }
    CHECK(std::fabs(clock.displayTime(now) - target) < 1e-6);
}

// Большая ошибка исправляется прыжком, прыжки считаются.
void testJump() {
    olc::ClientClock clock(0.1);
    clock.onPacket(1000.0, 1.0, false, 10.0);
    clock.onPacket(5000.0, 1.0, false, 10.0);  // хост перемотал время
    CHECK(clock.jumps() == 1);
    CHECK(near(clock.displayTime(10.0), 4999.9));
    clock.onPacket(100.0, 1.0, false, 10.0);
    CHECK(clock.jumps() == 2);
    CHECK(near(clock.displayTime(10.0), 99.9));
}

// Порог прыжка растёт с ускорением: при ускорении 100 ошибка 50 с - ещё не прыжок.
void testJumpThresholdScalesWithWarp() {
    olc::ClientClock clock(0.1);
    clock.onPacket(1000.0, 100.0, false, 10.0);
    clock.onPacket(1050.0, 100.0, true, 10.0);  // ошибка 50 < 1 × 100
    CHECK(clock.jumps() == 0);
    clock.onPacket(1300.0, 100.0, true, 10.0);  // ошибка больше порога
    CHECK(clock.jumps() == 1);
}

// Реальное время назад время показа не двигает.
void testNowGoesBack() {
    olc::ClientClock clock(0.1);
    clock.onPacket(1000.0, 1.0, false, 10.0);
    double at12 = clock.displayTime(12.0);
    CHECK(near(clock.displayTime(11.0), at12));
    // После "возврата" ход продолжается от наибольшего now.
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
