// Тесты буфера интерполяции: точность на орбите, края буфера, опоздавшие пакеты,
// посадка и смена опорного тела.

#include <cmath>

#include "check.h"
#include "interpolation.h"

namespace {

// Круговая орбита в плоскости xy: радиус 400 км над Землёй и круговая скорость.
const double kRadius = 6.771e6;
const double kSpeed = 7672.6;
const double kRate = 30.0;  // пакетов в секунду

// Летящий корабль на круговой орбите в момент t.
olc::VesselState orbitState(double t) {
    double angle = kSpeed / kRadius * t;
    olc::VesselState state;
    state.netId = 7;
    state.status = static_cast<uint8_t>(olc::VesselStatus::Flying);
    state.refBody = 3;
    state.rpos = {kRadius * std::cos(angle), kRadius * std::sin(angle), 0.0};
    state.rvel = {-kSpeed * std::sin(angle), kSpeed * std::cos(angle), 0.0};
    state.arot = {0.0, 0.0, angle};  // корабль поворачивается вместе с орбитой
    state.vrot = {0.0, 0.0, kSpeed / kRadius};
    return state;
}

double distance(const olc::Vec3& a, const olc::Vec3& b) {
    double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Совпадают ли числа с точностью до микрометра (координаты - миллионы метров).
bool near(double a, double b) {
    return std::fabs(a - b) < 1e-6;
}

void testOrbit() {
    olc::InterpolationBuffer buffer;
    for (int i = 0; i < 10; i++) {
        CHECK(buffer.add(i / kRate, orbitState(i / kRate)));
    }

    // Посередине между пакетами 4 и 5. Прямая между ними срезала бы дугу на ~1 мм,
    // Эрмит должен ошибаться намного меньше.
    double t = 4.5 / kRate;
    olc::VesselState shown;
    bool starved = true;
    CHECK(buffer.sample(t, shown, starved));
    CHECK(!starved);
    olc::VesselState truth = orbitState(t);
    CHECK(distance(shown.rpos, truth.rpos) < 1e-5);
    CHECK(distance(shown.rvel, truth.rvel) < 1e-3);
    CHECK(std::fabs(shown.arot.z - truth.arot.z) < 1e-12);
    CHECK(shown.netId == 7 && shown.refBody == 3);

    // Ровно в момент пакета - сам пакет.
    CHECK(buffer.sample(3 / kRate, shown, starved));
    CHECK(distance(shown.rpos, orbitState(3 / kRate).rpos) < 1e-6);
}

void testEdges() {
    olc::InterpolationBuffer buffer;
    olc::VesselState shown;
    bool starved = false;

    // Пустой буфер - показать нечего.
    CHECK(!buffer.sample(1.0, shown, starved));

    CHECK(buffer.add(1.0, orbitState(1.0)));
    CHECK(buffer.add(2.0, orbitState(2.0)));

    // Опоздавший и повторный пакеты не принимаются.
    CHECK(!buffer.add(1.5, orbitState(1.5)));
    CHECK(!buffer.add(2.0, orbitState(2.0)));
    CHECK(buffer.size() == 2);

    // Раньше первого пакета - первый пакет как есть.
    CHECK(buffer.sample(0.5, shown, starved));
    CHECK(!starved && shown.rpos.x == orbitState(1.0).rpos.x);

    // Буфер кончился: движение по последней скорости, ориентация последняя.
    olc::VesselState last = orbitState(2.0);
    CHECK(buffer.sample(2.5, shown, starved));
    CHECK(starved);
    CHECK(near(shown.rpos.x, last.rpos.x + last.rvel.x * 0.5));
    CHECK(near(shown.rpos.y, last.rpos.y + last.rvel.y * 0.5));
    CHECK(shown.arot.z == last.arot.z);
}

void testBufferLimit() {
    // Буфер хранит только последние kInterpolationBufferSize пакетов.
    olc::InterpolationBuffer buffer;
    for (int i = 0; i < 100; i++) {
        buffer.add(i, orbitState(i));
    }
    CHECK(buffer.size() == olc::kInterpolationBufferSize);

    // Самые старые забыты: раньше оставшихся показывается первый оставшийся.
    olc::VesselState shown;
    bool starved = false;
    CHECK(buffer.sample(0.0, shown, starved));
    double firstKept = 100.0 - olc::kInterpolationBufferSize;
    CHECK(shown.rpos.x == orbitState(firstKept).rpos.x);
}

void testNoInterpolationAcrossFrames() {
    // Корабль сел: между летящим и стоящим пакетами не интерполируем, а продолжаем
    // летящий по его скорости до прихода момента посадки.
    olc::VesselState flying = orbitState(0.0);
    flying.thrusters = {200};
    olc::VesselState landed;
    landed.netId = 7;
    landed.status = static_cast<uint8_t>(olc::VesselStatus::Landed);
    landed.refBody = 3;

    olc::InterpolationBuffer buffer;
    buffer.add(0.0, flying);
    buffer.add(0.1, landed);
    olc::VesselState shown;
    bool starved = true;
    CHECK(buffer.sample(0.05, shown, starved));
    CHECK(!starved);
    CHECK(shown.status == static_cast<uint8_t>(olc::VesselStatus::Flying));
    CHECK(near(shown.rpos.y, flying.rvel.y * 0.05));
    CHECK(shown.thrusters == flying.thrusters);

    // С момента посадки - стоящий корабль.
    CHECK(buffer.sample(0.1, shown, starved));
    CHECK(shown.status == static_cast<uint8_t>(olc::VesselStatus::Landed));

    // Смена опорного тела: положения в разных системах отсчёта, тоже не интерполируем.
    olc::VesselState otherBody = orbitState(0.1);
    otherBody.refBody = 4;
    olc::InterpolationBuffer soi;
    soi.add(0.0, flying);
    soi.add(0.1, otherBody);
    CHECK(soi.sample(0.05, shown, starved));
    CHECK(shown.refBody == 3);
    CHECK(near(shown.rpos.x, flying.rpos.x + flying.rvel.x * 0.05));
}

}  // namespace

int main() {
    testOrbit();
    testEdges();
    testBufferLimit();
    testNoInterpolationAcrossFrames();
    return checkResult();
}
