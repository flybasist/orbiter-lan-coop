#include "methods.h"

#include <algorithm>

namespace {

// Attitude continued with the last angular velocity.
Quat advanceAttitude(const State& s, double t) {
    return rotateBy(s.att, s.angVel, t - s.time);
}

// The common part of interpolation; useHermite chooses the method for position.
Display interpolate(const std::vector<State>& buffer, double tDisp, bool useHermite) {
    const State& newest = buffer.back();

    // Displayed time past the buffer end: keep moving with the last velocity.
    if (buffer.size() < 2 || tDisp >= newest.time) {
        Display d;
        d.pos = newest.pos + newest.vel * (tDisp - newest.time);
        d.att = advanceAttitude(newest, tDisp);
        d.starved = tDisp > newest.time;
        return d;
    }

    // Search from the end for a pair of packets around tDisp
    // (if tDisp is earlier than the first packet, take the first pair).
    size_t i = buffer.size() - 2;
    while (i > 0 && buffer[i].time > tDisp) {
        i--;
    }
    const State& a = buffer[i];
    const State& b = buffer[i + 1];
    double h = b.time - a.time;
    double s = std::clamp((tDisp - a.time) / h, 0.0, 1.0);

    Display d;
    d.pos = useHermite ? hermite(a.pos, a.vel, b.pos, b.vel, h, s) : lerp(a.pos, b.pos, s);
    d.att = slerp(a.att, b.att, s);
    return d;
}

}  // namespace

Display snapGravity(const State& newest, double nowSim) {
    Vec3 pos = newest.pos;
    Vec3 vel = newest.vel;
    propagateOrbit(pos, vel, Forces{}, nowSim - newest.time);

    Display d;
    d.pos = pos;
    d.att = advanceAttitude(newest, nowSim);
    return d;
}

Display extrapolate(const State& newest, double nowSim) {
    // Thrust at the send moment = total acceleration minus gravity; keep it constant.
    Forces forces;
    forces.constantAcc = newest.acc - gravityAccel(newest.pos);

    Vec3 pos = newest.pos;
    Vec3 vel = newest.vel;
    propagateOrbit(pos, vel, forces, nowSim - newest.time);

    Display d;
    d.pos = pos;
    d.att = advanceAttitude(newest, nowSim);
    return d;
}

Display interpLinear(const std::vector<State>& buffer, double tDisp) {
    return interpolate(buffer, tDisp, false);
}

Display interpHermite(const std::vector<State>& buffer, double tDisp) {
    return interpolate(buffer, tDisp, true);
}
