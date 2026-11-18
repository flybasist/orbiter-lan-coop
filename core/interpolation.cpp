#include "interpolation.h"

#include "attitude.h"

namespace olc {

namespace {

bool isFlying(const VesselState& state) {
    return state.status == static_cast<uint8_t>(VesselStatus::Flying);
}

Vec3 lerp(const Vec3& a, const Vec3& b, double s) {
    Vec3 result;
    result.x = a.x + (b.x - a.x) * s;
    result.y = a.y + (b.y - a.y) * s;
    result.z = a.z + (b.z - a.z) * s;
    return result;
}

// Cubic Hermite interpolation: the curve passes through p0 and p1 and has
// velocities v0 and v1 there. h - time between the points, s - fraction of the way from 0 to 1.
// Velocities are multiplied by h because the formula works in fractions, not in seconds.
double hermite(double p0, double v0, double p1, double v1, double h, double s) {
    double s2 = s * s;
    double s3 = s2 * s;
    double h00 = 2 * s3 - 3 * s2 + 1;
    double h10 = s3 - 2 * s2 + s;
    double h01 = -2 * s3 + 3 * s2;
    double h11 = s3 - s2;
    return h00 * p0 + h10 * h * v0 + h01 * p1 + h11 * h * v1;
}

// A state continued from a packet for dt seconds with its velocity (attitude unchanged).
VesselState moveOn(const VesselState& state, double dt) {
    VesselState result = state;
    if (isFlying(state)) {
        result.rpos.x += state.rvel.x * dt;
        result.rpos.y += state.rvel.y * dt;
        result.rpos.z += state.rvel.z * dt;
    }
    return result;
}

}  // namespace

bool InterpolationBuffer::add(double simTime, const VesselState& state) {
    if (!entries_.empty() && simTime <= entries_.back().simTime) {
        return false;
    }
    entries_.push_back({simTime, state});
    // Old packets are no longer needed: the displayed moment only moves forward.
    if (entries_.size() > kInterpolationBufferSize) {
        entries_.erase(entries_.begin());
    }
    return true;
}

bool InterpolationBuffer::latest(VesselState& result) const {
    if (entries_.empty()) {
        return false;
    }
    result = entries_.back().state;
    return true;
}

bool InterpolationBuffer::sample(double simTime, VesselState& result, bool& starved) const {
    starved = false;
    if (entries_.empty()) {
        return false;
    }

    // Before the first packet there is nothing to show but the packet itself.
    if (simTime <= entries_.front().simTime) {
        result = entries_.front().state;
        return true;
    }

    // The buffer ran out: keep moving with the last velocity.
    const Entry& newest = entries_.back();
    if (simTime >= newest.simTime) {
        starved = simTime > newest.simTime;
        result = moveOn(newest.state, simTime - newest.simTime);
        return true;
    }

    // Search from the end for a pair of packets a and b around simTime: the displayed
    // moment is usually near the end of the buffer.
    size_t i = entries_.size() - 2;
    while (entries_[i].simTime > simTime) {
        i--;
    }
    const Entry& a = entries_[i];
    const Entry& b = entries_[i + 1];

    // We cannot interpolate between a landed and a flying state, or between packets with different
    // reference bodies: the positions are in different frames.
    // Then until moment b arrives we show a, continued with its velocity.
    if (!isFlying(a.state) || !isFlying(b.state) || a.state.refBody != b.state.refBody) {
        result = moveOn(a.state, simTime - a.simTime);
        return true;
    }

    double h = b.simTime - a.simTime;
    double s = (simTime - a.simTime) / h;
    const VesselState& from = a.state;
    const VesselState& to = b.state;

    result = from;  // number, status, reference body and thruster levels - from a
    result.rpos.x = hermite(from.rpos.x, from.rvel.x, to.rpos.x, to.rvel.x, h, s);
    result.rpos.y = hermite(from.rpos.y, from.rvel.y, to.rpos.y, to.rvel.y, h, s);
    result.rpos.z = hermite(from.rpos.z, from.rvel.z, to.rpos.z, to.rvel.z, h, s);
    result.rvel = lerp(from.rvel, to.rvel, s);
    result.arot = quatToEuler(slerp(eulerToQuat(from.arot), eulerToQuat(to.arot), s));
    result.vrot = lerp(from.vrot, to.vrot, s);
    return true;
}

}  // namespace olc
