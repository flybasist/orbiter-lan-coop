#include "protocol.h"

namespace olc {

bool isSequenceNewer(uint32_t a, uint32_t b) {
    // Разность беззнаковых чисел считается по модулю 2^32, переполнение здесь законно.
    uint32_t ahead = a - b;
    return ahead != 0 && ahead < 0x80000000u;
}

}  // namespace olc
