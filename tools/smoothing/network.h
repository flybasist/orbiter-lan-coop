// A model of sending and delivering packets over UDP.
#pragma once

#include <vector>

#include "trajectory.h"

// All numeric simulation parameters.
struct Config {
    double rate = 20.0;         // send rate, packets per second of real time
    double warp = 1.0;          // how many times simulation time is faster than real time
    double lossPercent = 0.0;   // packet loss probability, %
    double latencyMs = 1.0;     // base delivery delay, ms of real time
    double jitterMs = 5.0;      // delay jitter (uniform from 0 to the value), ms
    double delayMs = 100.0;     // interpolation delay on the client, ms of real time
    unsigned seed = 1;          // random number generator seed
    double duration = 120.0;    // duration in seconds of simulation time
    Profile profile = Profile::Pilot;  // reference motion profile
};

// A delivered packet: the host state and the moment it arrived at the client.
struct Packet {
    State state;
    double arrivalWall = 0.0;  // real arrival time, s
};

// Returns the packets that reached the client, in arrival order
// (because of jitter the order by simulation time may be broken).
std::vector<Packet> simulateNetwork(const Trajectory& truth, const Config& config);
