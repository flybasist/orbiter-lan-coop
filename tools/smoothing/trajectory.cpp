#include "trajectory.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kMu = 3.986004418e14;  // гравитационный параметр Земли, м^3/с^2
constexpr double kStartRadius = 6.771e6;  // начальный радиус орбиты, м

// Основной двигатель: с 30 по 50 секунду тяга 3 м/с^2 вдоль скорости.
// В профиле pilot она включается импульсами: 1 с работает, 1 с пауза.
constexpr double kBurnStart = 30.0;
constexpr double kBurnEnd = 50.0;
constexpr double kBurnAcc = 3.0;
constexpr double kBurnPulseOn = 1.0;
constexpr double kBurnPulsePeriod = 2.0;

// Вращение. В профиле smooth: с 60 по 70 секунду угловое ускорение 0.02 рад/с^2
// вокруг оси Z, потом постоянная угловая скорость.
constexpr double kSpinStart = 60.0;
constexpr double kSpinEnd = 70.0;
constexpr double kSpinAcc = 0.02;

// RCS-вращение в профиле pilot: с 60 по 90 секунду каждые 2 с импульс 0.5 с
// с угловым ускорением 0.1 рад/с^2; оси X, Y, Z идут по кругу, знак чередуется.
constexpr double kRcsSpinStart = 60.0;
constexpr double kRcsSpinEnd = 90.0;
constexpr double kRcsSpinAcc = 0.1;
constexpr double kRcsSpinPulseOn = 0.5;
constexpr double kRcsSpinPulsePeriod = 2.0;

// RCS-перемещение в профиле pilot: с 95 по 110 секунду 0.3 м/с^2 вдоль мировой +Z,
// 0.5 с включено, 1.5 с пауза.
constexpr double kRcsMoveStart = 95.0;
constexpr double kRcsMoveEnd = 110.0;
constexpr double kRcsMoveAcc = 0.3;
constexpr double kRcsMovePulseOn = 0.5;
constexpr double kRcsMovePulsePeriod = 2.0;

// Включён ли импульс в момент t: серия начинается в start, каждые period секунд
// импульс длится on секунд.
bool pulseIsOn(double t, double start, double end, double on, double period) {
    return t >= start && t < end && std::fmod(t - start, period) < on;
}

// Силы (кроме гравитации), действующие в момент t.
Forces forcesAt(Profile profile, double t) {
    Forces forces;
    if (profile == Profile::Smooth) {
        if (t >= kBurnStart && t < kBurnEnd) {
            forces.thrustAlongVelocity = kBurnAcc;
        }
        return forces;
    }
    if (pulseIsOn(t, kBurnStart, kBurnEnd, kBurnPulseOn, kBurnPulsePeriod)) {
        forces.thrustAlongVelocity = kBurnAcc;
    }
    if (pulseIsOn(t, kRcsMoveStart, kRcsMoveEnd, kRcsMovePulseOn, kRcsMovePulsePeriod)) {
        forces.constantAcc = {0.0, 0.0, kRcsMoveAcc};
    }
    return forces;
}

// Угловое ускорение в момент t (вектор в осях тела).
Vec3 angularAccelAt(Profile profile, double t) {
    if (profile == Profile::Smooth) {
        if (t >= kSpinStart && t < kSpinEnd) {
            return {0.0, 0.0, kSpinAcc};
        }
        return {};
    }
    if (!pulseIsOn(t, kRcsSpinStart, kRcsSpinEnd, kRcsSpinPulseOn, kRcsSpinPulsePeriod)) {
        return {};
    }
    // Номер импульса определяет ось (X, Y, Z по кругу) и знак (+, -, +, ...).
    int number = static_cast<int>((t - kRcsSpinStart) / kRcsSpinPulsePeriod);
    double signedAcc = (number % 2 == 0) ? kRcsSpinAcc : -kRcsSpinAcc;
    Vec3 acc;
    if (number % 3 == 0) {
        acc.x = signedAcc;
    } else if (number % 3 == 1) {
        acc.y = signedAcc;
    } else {
        acc.z = signedAcc;
    }
    return acc;
}

Vec3 totalAccel(Vec3 pos, Vec3 vel, const Forces& forces) {
    return gravityAccel(pos) + forces.constantAcc + normalize(vel) * forces.thrustAlongVelocity;
}

// Один шаг Рунге-Кутты 4-го порядка для пары (положение, скорость).
void rk4Step(Vec3& pos, Vec3& vel, const Forces& forces, double dt) {
    Vec3 k1p = vel;
    Vec3 k1v = totalAccel(pos, vel, forces);

    Vec3 k2p = vel + k1v * (dt / 2.0);
    Vec3 k2v = totalAccel(pos + k1p * (dt / 2.0), k2p, forces);

    Vec3 k3p = vel + k2v * (dt / 2.0);
    Vec3 k3v = totalAccel(pos + k2p * (dt / 2.0), k3p, forces);

    Vec3 k4p = vel + k3v * dt;
    Vec3 k4v = totalAccel(pos + k3p * dt, k4p, forces);

    pos = pos + (k1p + k2p * 2.0 + k3p * 2.0 + k4p) * (dt / 6.0);
    vel = vel + (k1v + k2v * 2.0 + k3v * 2.0 + k4v) * (dt / 6.0);
}

}  // namespace

Vec3 gravityAccel(Vec3 pos) {
    double r = length(pos);
    return pos * (-kMu / (r * r * r));
}

void propagateOrbit(Vec3& pos, Vec3& vel, const Forces& forces, double duration) {
    if (duration <= 0.0) {
        return;
    }
    int steps = static_cast<int>(std::ceil(duration / kStep));
    double dt = duration / steps;
    for (int i = 0; i < steps; i++) {
        rk4Step(pos, vel, forces, dt);
    }
}

Trajectory::Trajectory(double duration, Profile profile) {
    // Запас в секунду, чтобы интерполяции в конце не упирались в край.
    int steps = static_cast<int>(std::ceil((duration + 1.0) / kStep));
    samples_.reserve(steps + 1);

    Vec3 pos = {kStartRadius, 0.0, 0.0};
    Vec3 vel = {0.0, std::sqrt(kMu / kStartRadius), 0.0};  // круговая скорость
    Quat att;
    Vec3 angVel;

    for (int i = 0; i <= steps; i++) {
        double t = i * kStep;

        // Небольшой сдвиг, чтобы границы импульсов не зависели от ошибок округления t.
        Forces now = forcesAt(profile, t + 1e-9);

        State s;
        s.time = t;
        s.pos = pos;
        s.vel = vel;
        s.acc = gravityAccel(pos) + now.constantAcc + normalize(vel) * now.thrustAlongVelocity;
        s.att = att;
        s.angVel = angVel;
        samples_.push_back(s);

        // Шаг к следующему образцу. Силы берём в середине шага, чтобы границы
        // импульсов (кратные шагу) не попадали на стык шагов.
        double mid = t + kStep / 2.0;
        rk4Step(pos, vel, forcesAt(profile, mid), kStep);
        Vec3 angAcc = angularAccelAt(profile, mid);
        Vec3 angVelMid = angVel + angAcc * (kStep / 2.0);
        att = rotateBy(att, angVelMid, kStep);
        angVel = angVel + angAcc * kStep;
    }
}

State Trajectory::at(double t) const {
    // Номер образца слева от t; t и индекс ограничиваем доступным диапазоном.
    double maxTime = samples_.back().time;
    t = std::clamp(t, 0.0, maxTime);
    size_t i = static_cast<size_t>(t / kStep);
    if (i > samples_.size() - 2) {
        i = samples_.size() - 2;
    }
    const State& a = samples_[i];
    const State& b = samples_[i + 1];
    double s = (t - a.time) / kStep;

    State result;
    result.time = t;
    result.pos = hermite(a.pos, a.vel, b.pos, b.vel, kStep, s);
    result.vel = lerp(a.vel, b.vel, s);
    result.acc = lerp(a.acc, b.acc, s);
    result.att = slerp(a.att, b.att, s);
    result.angVel = lerp(a.angVel, b.angVel, s);
    return result;
}
