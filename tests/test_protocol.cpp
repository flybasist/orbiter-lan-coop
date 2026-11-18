// Tests of protocol constants and sequence number comparison.

#include "check.h"
#include "protocol.h"

namespace {

void testConstants() {
    // The values must match docs/PROTOCOL.md.
    CHECK(olc::kProtocolVersion == 4);
    CHECK(olc::kDefaultPort == 47800);
    CHECK(olc::kUdpMagic[0] == 'O' && olc::kUdpMagic[1] == 'L' && olc::kUdpMagic[2] == 'C' &&
          olc::kUdpMagic[3] == '0');
    CHECK(olc::kMaxUdpPacketSize == 1200);
    CHECK(olc::kMaxTcpMessageSize == 1048576);
}

void testSequenceNewer() {
    // The usual case: a larger number is newer.
    CHECK(olc::isSequenceNewer(2, 1));
    CHECK(!olc::isSequenceNewer(1, 2));

    // A repeat of the same packet is not newer.
    CHECK(!olc::isSequenceNewer(5, 5));

    // Counter wrap: 2 came after 4294967290.
    CHECK(olc::isSequenceNewer(2, 4294967290u));
    CHECK(!olc::isSequenceNewer(4294967290u, 2));
    CHECK(olc::isSequenceNewer(0, 4294967295u));

    // The half circle boundary: exactly 2^31 ahead is no longer newer.
    CHECK(olc::isSequenceNewer(0x7FFFFFFFu, 0));
    CHECK(!olc::isSequenceNewer(0x80000000u, 0));
}

}  // namespace

int main() {
    testConstants();
    testSequenceNewer();
    return checkResult();
}
