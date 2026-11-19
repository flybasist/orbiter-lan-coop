// Tests of protocol constants and sequence number comparison.

#include "check.h"
#include "protocol.h"

namespace {

void testConstants() {
    // The values must match docs/PROTOCOL.md.
    CHECK(olc::kProtocolVersion == 6);
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

// The next send time counts from the planned time, and from now after a long frame.
void testNextSendTime() {
    const double period = 1.0 / 60.0;
    // On time or a little late: the next one is one period after the planned time.
    CHECK(olc::nextSendTime(1.0, 1.0, period) == 1.0 + period);
    CHECK(olc::nextSendTime(1.0, 1.001, period) == 1.0 + period);
    // Late by more than a period: one period after now, not a burst to catch up.
    CHECK(olc::nextSendTime(1.0, 1.5, period) == 1.5 + period);
    // Nothing planned yet (0): one period after now.
    CHECK(olc::nextSendTime(0.0, 100.0, period) == 100.0 + period);

    // Frames at 60.2 per second with a limit of 60 per second: almost every frame sends.
    double planned = 0.0;
    double frame = 10.0;
    int sent = 0;
    for (int i = 0; i < 60; i++) {
        if (frame >= planned) {
            planned = olc::nextSendTime(planned, frame, period);
            sent++;
        }
        frame += 1.0 / 60.2;
    }
    CHECK(sent >= 58);
}

int main() {
    testConstants();
    testSequenceNewer();
    testNextSendTime();
    return checkResult();
}
