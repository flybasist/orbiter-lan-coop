#include "trajectory.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kMu = 3.986004418e14;  // Earth's gravitational parameter, m^3/s^2
constexpr double kStartRadius = 6.771e6;  // initial orbit radius, m

// Main engine: from second 30 to 50, thrust 3 m/s^2 along the velocity.
// In the pilot profile it fires in pulses: 1 s on, 1 s off.
constexpr double kBurnStart = 30.0;
constexpr double kBurnEnd = 50.0;
constexpr double kBurnAcc = 3.0;
constexpr double kBurnPulseOn = 1.0;
constexpr double kBurnPulsePeriod = 2.0;

// Rotation. In the smooth profile: from second 60 to 70 angular acceleration 0.02 rad/s^2
// around the Z axis, then a constant angular velocity.
constexpr double kSpinStart = 60.0;
constexpr double kSpinEnd = 70.0;
constexpr double kSpinAcc = 0.02;

// RCS rotation in the pilot profile: from second 60 to 90, every 2 s a 0.5 s pulse
// with angular acceleration 0.1 rad/s^2; the X, Y, Z axes go in a circle, the sign alternates.
constexpr double kRcsSpinStart = 60.0;
constexpr double kRcsSpinEnd = 90.0;
constexpr double kRcsSpinAcc = 0.1;
constexpr double kRcsSpinPulseOn = 0.5;
constexpr double kRcsSpinPulsePeriod = 2.0;

// RCS translation in the pilot profile: from second 95 to 110, 0.3 m/s^2 along world +Z,
// 0.5 s on, 1.5 s off.
constexpr double kRcsMoveStart = 95.0;
constexpr double kRcsMoveEnd = 110.0;
constexpr double kRcsMoveAcc = 0.3;
constexpr double kRcsMovePulseOn = 0.5;
constexpr double kRcsMovePulsePeriod = 2.0;

// Whether a pulse is on at moment t: the series starts at start, every period seconds
// the pulse lasts on seconds.
bool pulseIsOn(double t, double start, double end, double on, double period) {
    return t >= start && t < end && std::fmod(t - start, period) < on;
}

// Forces (other than gravity) acting at moment t.
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

// Angular acceleration at moment t (a vector in body axes).
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
    // The pulse number defines the axis (X, Y, Z in a circle) and the sign (+, -, +, ...).
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

// One step of 4th order Runge-Kutta for the pair (position, velocity).
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
    // A one-second margin, so that interpolations at the end do not hit the edge.
    int steps = static_cast<int>(std::ceil((duration + 1.0) / kStep));
    samples_.reserve(steps + 1);

    Vec3 pos = {kStartRadius, 0.0, 0.0};
    Vec3 vel = {0.0, std::sqrt(kMu / kStartRadius), 0.0};  // circular velocity
    Quat att;
    Vec3 angVel;

    for (int i = 0; i <= steps; i++) {
        double t = i * kStep;

        // A small offset, so that pulse boundaries do not depend on rounding errors of t.
        Forces now = forcesAt(profile, t + 1e-9);

        State s;
        s.time = t;
        s.pos = pos;
        s.vel = vel;
        s.acc = gravityAccel(pos) + now.constantAcc + normalize(vel) * now.thrustAlongVelocity;
        s.att = att;
        s.angVel = angVel;
        samples_.push_back(s);

        // Step to the next sample. Forces are taken in the middle of the step, so that pulse
        // boundaries (multiples of the step) do not fall on the step joints.
        double mid = t + kStep / 2.0;
        rk4Step(pos, vel, forcesAt(profile, mid), kStep);
        Vec3 angAcc = angularAccelAt(profile, mid);
        Vec3 angVelMid = angVel + angAcc * (kStep / 2.0);
        att = rotateBy(att, angVelMid, kStep);
        angVel = angVel + angAcc * kStep;
    }
}

State Trajectory::at(double t) const {
    // The sample number to the left of t; t and the index are clamped to the available range.
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
