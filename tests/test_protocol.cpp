// Тесты постоянных протокола и сравнения порядковых номеров.

#include "check.h"
#include "protocol.h"

namespace {

void testConstants() {
    // Значения должны совпадать с docs/PROTOCOL.md.
    CHECK(olc::kProtocolVersion == 1);
    CHECK(olc::kDefaultPort == 47800);
    CHECK(olc::kUdpMagic[0] == 'O' && olc::kUdpMagic[1] == 'L' && olc::kUdpMagic[2] == 'C' &&
          olc::kUdpMagic[3] == '0');
    CHECK(olc::kMaxUdpPacketSize == 1200);
    CHECK(olc::kMaxTcpMessageSize == 1048576);
}

void testSequenceNewer() {
    // Обычный случай: больший номер новее.
    CHECK(olc::isSequenceNewer(2, 1));
    CHECK(!olc::isSequenceNewer(1, 2));

    // Повтор того же пакета не новее.
    CHECK(!olc::isSequenceNewer(5, 5));

    // Переполнение счётчика: 2 пришёл после 4294967290.
    CHECK(olc::isSequenceNewer(2, 4294967290u));
    CHECK(!olc::isSequenceNewer(4294967290u, 2));
    CHECK(olc::isSequenceNewer(0, 4294967295u));

    // Граница половины круга: ровно 2^31 вперёд уже не считается новее.
    CHECK(olc::isSequenceNewer(0x7FFFFFFFu, 0));
    CHECK(!olc::isSequenceNewer(0x80000000u, 0));
}

}  // namespace

int main() {
    testConstants();
    testSequenceNewer();
    return checkResult();
}
