// Interpolation buffer: smooth vessel motion on the client between WORLD_STATE packets.
//
// The client shows the world with a small delay and reconstructs the vessel state
// between two received packets (docs/SMOOTHING.md):
// - position - cubic Hermite interpolation over positions and velocities;
// - velocity and angular velocity - linear interpolation;
// - attitude - slerp (attitude.h);
// - thruster levels - from the earlier packet.
// One buffer - one vessel. The buffer works in host sim time; which moment to display
// is decided by the caller: the display delay (DisplayDelay, 30 ms by default) of real time behind
// the host, that is DisplayDelay × warp seconds of sim time (SMOOTHING.md).
#pragma once

#include <vector>

#include "messages.h"

namespace olc {

// How many recent packets the buffer keeps: 64 packets - about 1 s at 60 Hz (2 s at 30 Hz), with
// a large margin over the display delay.
constexpr size_t kInterpolationBufferSize = 64;

class InterpolationBuffer {
public:
    // Adds a vessel state at sim time simTime (from WORLD_STATE).
    // Returns false and adds nothing if simTime is not newer than the last packet:
    // the packet is late or repeated.
    bool add(double simTime, const VesselState& state);

    // Vessel state at sim time simTime. Returns false if the buffer is empty.
    // starved = true if simTime is newer than the last packet (the buffer ran out): then
    // the vessel keeps moving with the last velocity, and the attitude stays the last one.
    // If simTime is earlier than the first packet, the first packet is returned as is.
    bool sample(double simTime, VesselState& result, bool& starved) const;

    // The newest packet as it came, without interpolation. Returns false if the buffer is empty.
    // For measurements: what the host sent last, before the display delay.
    bool latest(VesselState& result) const;

    // How many packets are in the buffer.
    size_t size() const { return entries_.size(); }

private:
    struct Entry {
        double simTime;
        VesselState state;
    };

    std::vector<Entry> entries_;  // ascending simTime
};

}  // namespace olc
