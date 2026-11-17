#include "network.h"

#include <algorithm>
#include <random>

std::vector<Packet> simulateNetwork(const Trajectory& truth, const Config& config) {
    // seed_seq mixes the initial number: if mt19937 is seeded directly with a small
    // number (1, 2, 3...), the first random values for different seeds come out similar.
    std::seed_seq seedSequence{config.seed};
    std::mt19937 generator(seedSequence);
    std::uniform_real_distribution<double> unit(0.0, 1.0);

    // The host does not start sending exactly at moment 0 but with a random offset within
    // one interval. Otherwise the send moments coincide with the start and end of thruster
    // pulses (whole and half seconds), and extrapolation and interpolation
    // get an unfair advantage: in a real game the pilot does not wait for a packet.
    double phase = unit(generator);

    std::vector<Packet> packets;
    for (long k = 0;; k++) {
        double sendWall = (k + phase) / config.rate;
        double simTime = sendWall * config.warp;
        if (simTime > config.duration) {
            break;
        }

        // Both random numbers are always taken, so that loss does not shift the sequence.
        double lossRoll = unit(generator);
        double jitterRoll = unit(generator);
        if (lossRoll * 100.0 < config.lossPercent) {
            continue;  // packet lost
        }

        Packet packet;
        packet.state = truth.at(simTime);
        packet.arrivalWall = sendWall + (config.latencyMs + jitterRoll * config.jitterMs) / 1000.0;
        packets.push_back(packet);
    }

    // The client gets packets in arrival order, not in send order.
    std::stable_sort(packets.begin(), packets.end(),
                     [](const Packet& a, const Packet& b) { return a.arrivalWall < b.arrivalWall; });
    return packets;
}
