#include "network.h"

#include <algorithm>
#include <random>

std::vector<Packet> simulateNetwork(const Trajectory& truth, const Config& config) {
    // seed_seq перемешивает начальное число: если завести mt19937 напрямую маленьким
    // числом (1, 2, 3...), первые случайные значения у разных seed получаются похожими.
    std::seed_seq seedSequence{config.seed};
    std::mt19937 generator(seedSequence);
    std::uniform_real_distribution<double> unit(0.0, 1.0);

    // Хост начинает отправку не точно в момент 0, а со случайным сдвигом в пределах
    // одного интервала. Иначе моменты отправки совпадают с началом и концом импульсов
    // двигателей (целые и половинные секунды), и экстраполяция с интерполяцией
    // получают нечестное преимущество: в настоящей игре пилот не ждёт пакета.
    double phase = unit(generator);

    std::vector<Packet> packets;
    for (long k = 0;; k++) {
        double sendWall = (k + phase) / config.rate;
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
