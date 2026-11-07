#include "network.h"

#include <algorithm>
#include <random>

std::vector<Packet> simulateNetwork(const Trajectory& truth, const Config& config) {
    std::mt19937 generator(config.seed);
    std::uniform_real_distribution<double> unit(0.0, 1.0);

    std::vector<Packet> packets;
    for (long k = 0;; k++) {
        double sendWall = k / config.rate;
        double simTime = sendWall * config.warp;
        if (simTime > config.duration) {
            break;
        }

        // Оба случайных числа берём всегда, чтобы потери не сдвигали последовательность.
        double lossRoll = unit(generator);
        double jitterRoll = unit(generator);
        if (lossRoll * 100.0 < config.lossPercent) {
            continue;  // пакет потерян
        }

        Packet packet;
        packet.state = truth.at(simTime);
        packet.arrivalWall = sendWall + (config.latencyMs + jitterRoll * config.jitterMs) / 1000.0;
        packets.push_back(packet);
    }

    // Клиент получает пакеты в порядке прихода, а не отправки.
    std::stable_sort(packets.begin(), packets.end(),
                     [](const Packet& a, const Packet& b) { return a.arrivalWall < b.arrivalWall; });
    return packets;
}
