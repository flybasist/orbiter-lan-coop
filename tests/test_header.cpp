// Тесты заголовков: UDP-заголовок и сборка TCP-сообщений из потока,
// включая битые и чужие пакеты.

#include <vector>

#include "check.h"
#include "header.h"
#include "protocol.h"

namespace {

using olc::UdpHeaderResult;
using Frame = olc::TcpFrameReader::Result;

// Пакет с правильным заголовком и телом из двух байтов.
std::vector<uint8_t> makeUdpPacket() {
    olc::Writer w;
    olc::UdpHeader header;
    header.type = 7;
    header.session = 123456;
    header.seq = 42;
    olc::writeUdpHeader(w, header);
    w.writeU8(0xAA);
    w.writeU8(0xBB);
    return w.bytes();
}

UdpHeaderResult parsePacket(const std::vector<uint8_t>& packet, olc::UdpHeader& header) {
    olc::Reader r(packet.data(), packet.size());
    return olc::readUdpHeader(r, header);
}

void testUdpHeader() {
    std::vector<uint8_t> packet = makeUdpPacket();
    CHECK(packet.size() == olc::kUdpHeaderSize + 2);
    // Начало пакета: сигнатура OLC0 и версия 1 little-endian.
    CHECK(packet[0] == 'O' && packet[1] == 'L' && packet[2] == 'C' && packet[3] == '0');
    CHECK(packet[4] == 1 && packet[5] == 0);

    olc::Reader r(packet.data(), packet.size());
    olc::UdpHeader header;
    CHECK(olc::readUdpHeader(r, header) == UdpHeaderResult::Ok);
    CHECK(header.type == 7 && header.session == 123456 && header.seq == 42);
    // Reader стоит на теле сообщения.
    uint8_t first = 0;
    CHECK(r.readU8(first) && first == 0xAA);
}

void testBrokenUdp() {
    olc::UdpHeader header;

    // Короче заголовка, в том числе пустой пакет.
    std::vector<uint8_t> packet = makeUdpPacket();
    packet.resize(olc::kUdpHeaderSize - 1);
    CHECK(parsePacket(packet, header) == UdpHeaderResult::TooShort);
    CHECK(parsePacket({}, header) == UdpHeaderResult::TooShort);

    // Чужая сигнатура.
    packet = makeUdpPacket();
    packet[3] = '1';
    CHECK(parsePacket(packet, header) == UdpHeaderResult::WrongMagic);

    // Другая версия протокола.
    packet = makeUdpPacket();
    packet[4] = 2;
    CHECK(parsePacket(packet, header) == UdpHeaderResult::WrongVersion);

    // Больше предельного размера.
    packet = makeUdpPacket();
    packet.resize(olc::kMaxUdpPacketSize + 1);
    CHECK(parsePacket(packet, header) == UdpHeaderResult::TooLong);
    packet.resize(olc::kMaxUdpPacketSize);
    CHECK(parsePacket(packet, header) == UdpHeaderResult::Ok);
}

void testTcpFrame() {
    std::vector<uint8_t> frame = olc::makeTcpFrame(3, {10, 20, 30});
    // Длина 4 (тип + 3 байта тела), тип, тело.
    CHECK(frame == std::vector<uint8_t>({4, 0, 0, 0, 3, 10, 20, 30}));

    // Два сообщения, пришедшие по одному байту: собираются целиком и по порядку.
    std::vector<uint8_t> stream = frame;
    std::vector<uint8_t> second = olc::makeTcpFrame(9, {});
    stream.insert(stream.end(), second.begin(), second.end());

    olc::TcpFrameReader reader;
    uint8_t type = 0;
    std::vector<uint8_t> body;
    int ready = 0;
    for (uint8_t byte : stream) {
        reader.append(&byte, 1);
        Frame result = reader.next(type, body);
        if (result == Frame::Ready) {
            ready++;
            if (ready == 1) {
                CHECK(type == 3 && body == std::vector<uint8_t>({10, 20, 30}));
            } else {
                CHECK(type == 9 && body.empty());
            }
        } else {
            CHECK(result == Frame::NeedMore);
        }
    }
    CHECK(ready == 2);
    CHECK(reader.next(type, body) == Frame::NeedMore);

    // Два сообщения одним куском.
    olc::TcpFrameReader together;
    together.append(stream.data(), stream.size());
    CHECK(together.next(type, body) == Frame::Ready && type == 3);
    CHECK(together.next(type, body) == Frame::Ready && type == 9);
    CHECK(together.next(type, body) == Frame::NeedMore);
}

void testBrokenTcp() {
    uint8_t type = 0;
    std::vector<uint8_t> body;

    // Длина 0: даже типа нет.
    olc::TcpFrameReader zero;
    const uint8_t zeroLength[4] = {0, 0, 0, 0};
    zero.append(zeroLength, sizeof(zeroLength));
    CHECK(zero.next(type, body) == Frame::Broken);

    // Длина больше 1 МБ: поток испорчен, и дальше он не разбирается,
    // даже если потом придёт правильное сообщение.
    olc::TcpFrameReader huge;
    const uint8_t hugeLength[4] = {0x01, 0x00, 0x10, 0x00};  // 1 МБ + 1
    huge.append(hugeLength, sizeof(hugeLength));
    CHECK(huge.next(type, body) == Frame::Broken);
    std::vector<uint8_t> good = olc::makeTcpFrame(1, {});
    huge.append(good.data(), good.size());
    CHECK(huge.next(type, body) == Frame::Broken);

    // Ровно 1 МБ - допустимо: ждём остальные байты.
    olc::TcpFrameReader limit;
    const uint8_t limitLength[4] = {0x00, 0x00, 0x10, 0x00};
    limit.append(limitLength, sizeof(limitLength));
    CHECK(limit.next(type, body) == Frame::NeedMore);
}

}  // namespace

int main() {
    testUdpHeader();
    testBrokenUdp();
    testTcpFrame();
    testBrokenTcp();
    return checkResult();
}
