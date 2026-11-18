#include "protocol.h"

namespace olc {

bool isSequenceNewer(uint32_t a, uint32_t b) {
    // Unsigned subtraction is modulo 2^32, so wrapping is legal here.
    uint32_t ahead = a - b;
    return ahead != 0 && ahead < 0x80000000u;
}

double nextSendTime(double planned, double now, double period) {
    const double next = planned + period;
    return next > now ? next : now + period;
}

}  // namespace olc
