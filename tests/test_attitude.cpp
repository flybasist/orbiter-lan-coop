// Тесты ориентации: углы ↔ кватернион туда-обратно, особый случай y = ±π/2, slerp.

#include <cmath>

#include "attitude.h"
#include "check.h"

namespace {

const double kPi = 3.14159265358979323846;

// Одинаковые ли повороты: q и -q - один и тот же поворот, поэтому сравниваем
// модуль скалярного произведения с 1.
bool sameRotation(const olc::Quat& a, const olc::Quat& b) {
    double dot = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    return std::fabs(std::fabs(dot) - 1.0) < 1e-12;
}

bool near(double a, double b) {
    return std::fabs(a - b) < 1e-9;
}

void testRoundTrip() {
    // Сетка углов по всем трём осям, кроме y = ±π/2 (проверяется отдельно).
    int bad = 0;
    for (double x = -3.1; x <= 3.1; x += 0.7) {
        for (double y = -1.5; y <= 1.5; y += 0.5) {
            for (double z = -3.1; z <= 3.1; z += 0.7) {
                olc::Vec3 arot = {x, y, z};
                olc::Vec3 back = olc::quatToEuler(olc::eulerToQuat(arot));
                if (!near(back.x, x) || !near(back.y, y) || !near(back.z, z)) {
                    bad++;
                }
            }
        }
    }
    CHECK(bad == 0);
}

void testGimbalLock() {
    // При y = π/2 углы x и z по отдельности не определены, но поворот должен
    // сохраниться: x становится 0, а z вбирает весь поворот вокруг оси.
    olc::Vec3 arot = {0.3, kPi / 2, 0.5};
    olc::Quat q = olc::eulerToQuat(arot);
    olc::Vec3 back = olc::quatToEuler(q);
    CHECK(back.x == 0.0);
    CHECK(near(back.y, kPi / 2));
    CHECK(sameRotation(olc::eulerToQuat(back), q));

    olc::Vec3 down = {-0.4, -kPi / 2, 1.2};
    olc::Quat qd = olc::eulerToQuat(down);
    CHECK(sameRotation(olc::eulerToQuat(olc::quatToEuler(qd)), qd));
}

void testSlerp() {
    // Поворот только вокруг z: середина между 0 и 0,2 рад - 0,1 рад.
    olc::Quat a = olc::eulerToQuat({0.0, 0.0, 0.0});
    olc::Quat b = olc::eulerToQuat({0.0, 0.0, 0.2});
    olc::Vec3 middle = olc::quatToEuler(olc::slerp(a, b, 0.5));
    CHECK(near(middle.x, 0.0) && near(middle.y, 0.0) && near(middle.z, 0.1));

    // Концы: s = 0 - a, s = 1 - b.
    CHECK(sameRotation(olc::slerp(a, b, 0.0), a));
    CHECK(sameRotation(olc::slerp(a, b, 1.0), b));

    // Ради этого и нужен slerp: 3,1 и -3,1 рад почти совпадают, и середина между
    // ними - π, а не 0 (как дало бы среднее углов).
    olc::Quat c = olc::eulerToQuat({0.0, 0.0, 3.1});
    olc::Quat d = olc::eulerToQuat({0.0, 0.0, -3.1});
    olc::Vec3 across = olc::quatToEuler(olc::slerp(c, d, 0.5));
    CHECK(near(std::fabs(across.z), kPi));

    // Почти одинаковые повороты (ветка без деления на sin угла).
    olc::Quat e = olc::eulerToQuat({0.0, 0.0, 1e-6});
    olc::Vec3 tiny = olc::quatToEuler(olc::slerp(a, e, 0.5));
    CHECK(std::fabs(tiny.z - 0.5e-6) < 1e-12);
}

}  // namespace

int main() {
    testRoundTrip();
    testGimbalLock();
    testSlerp();
    return checkResult();
}
