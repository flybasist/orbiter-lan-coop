// Vessel attitude: Orbiter Euler angles, quaternions and a smooth transition between them.
//
// Over the network attitude goes as Euler angles, as in VESSELSTATUS2::arot (PROTOCOL.md,
// section 6). The angles cannot be interpolated one by one: for example, between 3.1 and -3.1 rad
// (almost the same rotation) we would get 0 - a 180° turn. So the angles
// are converted into a quaternion, the quaternions are interpolated (slerp), and the result
// is converted back into angles.
#pragma once

#include "messages.h"

namespace olc {

// Unit rotation quaternion: w - scalar part, x, y, z - vector part.
struct Quat {
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

// arot angles (rad) → quaternion. The rotation matrix is built from the angles the same way as in
// the Orbiter core (Src/Orbiter/Vessel.cpp:900, Vessel::SetGlobalOrientation).
Quat eulerToQuat(const Vec3& arot);

// Quaternion → arot angles (rad): x and z within -π…π, y within -π/2…π/2.
// If y = ±π/2 (the vessel points exactly along the axis), x and z are not defined
// separately: then x = 0, and the whole rotation around the axis goes into z.
Vec3 quatToEuler(const Quat& q);

// The rotation between a and b at fraction s (0 - a, 1 - b), along the shortest path.
Quat slerp(const Quat& a, const Quat& b, double s);

}  // namespace olc
