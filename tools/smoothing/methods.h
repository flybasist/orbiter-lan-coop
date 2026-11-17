// Ways of reconstructing motion on the client.
#pragma once

#include <vector>

#include "trajectory.h"

// What the client draws in a given frame.
struct Display {
    Vec3 pos;
    Quat att;
    bool starved = false;  // the buffer ran out: had to extrapolate (interpolation only)
};

// 1. Like a plain DefSetStateEx in Orbiter: the last state carried forward
//    to moment nowSim under gravity only.
Display snapGravity(const State& newest, double nowSim);

// 2. The last state carried to nowSim with the constant acceleration from the packet.
Display extrapolate(const State& newest, double nowSim);

// 3. Linear interpolation of position between packets, slerp for attitude.
//    buffer - received packets in ascending time, tDisp - displayed time.
Display interpLinear(const std::vector<State>& buffer, double tDisp);

// 4. Cubic Hermite interpolation over positions and velocities, slerp for attitude.
Display interpHermite(const std::vector<State>& buffer, double tDisp);
