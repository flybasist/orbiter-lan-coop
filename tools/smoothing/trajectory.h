// Reference ("true") vessel motion on the host.
#pragma once

#include <vector>

#include "vec.h"

// Integration step in simulation time, seconds.
constexpr double kStep = 0.01;

// Motion profile: smooth (one long maneuver) or "pilot" (engine and RCS pulses).
enum class Profile { Smooth, Pilot };

// Vessel state at simulation time time.
// This is also the content of a network packet.
struct State {
    double time = 0.0;  // simulation time, s
    Vec3 pos;           // position, m
    Vec3 vel;           // velocity, m/s
    Vec3 acc;           // total acceleration (gravity + thrust), m/s^2
    Quat att;           // attitude
    Vec3 angVel;        // angular velocity, rad/s
};

// Forces other than gravity, for the orbit calculation.
struct Forces {
    double thrustAlongVelocity = 0.0;  // thrust along the velocity, m/s^2
    Vec3 constantAcc;                  // constant acceleration in world axes, m/s^2
};

// Gravitational acceleration toward Earth's centre at point pos.
Vec3 gravityAccel(Vec3 pos);

// Carries pos and vel forward by duration seconds with RK4 (steps no longer than kStep).
void propagateOrbit(Vec3& pos, Vec3& vel, const Forces& forces, double duration);

// The true motion: computed once at creation, after that the state can be
// asked for at any moment of simulation time.
class Trajectory {
public:
    Trajectory(double duration, Profile profile);

    // State at moment t (outside the recorded range the edge value is taken).
    State at(double t) const;

private:
    std::vector<State> samples_;  // states every kStep seconds
};
