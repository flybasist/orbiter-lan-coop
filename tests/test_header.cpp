// Header tests: the UDP header and assembling TCP messages from a stream,
// including broken and foreign packets.

#include <vector>

#include "check.h"
#include "header.h"
#include "protocol.h"

namespace {

using olc::UdpHeaderResult;
using Frame = olc::TcpFrameReader::Result;

// A packet with a correct header and a two-byte body.
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
    // Start of the packet: the OLC0 signature and the protocol version, little-endian.
    CHECK(packet[0] == 'O' && packet[1] == 'L' && packet[2] == 'C' && packet[3] == '0');
    CHECK(packet[4] == (olc::kProtocolVersion & 0xFF) && packet[5] == (olc::kProtocolVersion >> 8));

    olc::Reader r(packet.data(), packet.size());
    olc::UdpHeader header;
    CHECK(olc::readUdpHeader(r, header) == UdpHeaderResult::Ok);
    CHECK(header.type == 7 && header.session == 123456 && header.seq == 42);
    // Reader is at the message body.
    uint8_t first = 0;
    CHECK(r.readU8(first) && first == 0xAA);
}

void testBrokenUdp() {
    olc::UdpHeader header;

    // Shorter than the header, including an empty packet.
    std::vector<uint8_t> packet = makeUdpPacket();
    packet.resize(olc::kUdpHeaderSize - 1);
    CHECK(parsePacket(packet, header) == UdpHeaderResult::TooShort);
    CHECK(parsePacket({}, header) == UdpHeaderResult::TooShort);

    // Foreign signature.
    packet = makeUdpPacket();
    packet[3] = '1';
    CHECK(parsePacket(packet, header) == UdpHeaderResult::WrongMagic);

    // Another protocol version.
    packet = makeUdpPacket();
    packet[4] = static_cast<uint8_t>(olc::kProtocolVersion + 1);
    CHECK(parsePacket(packet, header) == UdpHeaderResult::WrongVersion);

    // Larger than the size limit.
    packet = makeUdpPacket();
    packet.resize(olc::kMaxUdpPacketSize + 1);
    CHECK(parsePacket(packet, header) == UdpHeaderResult::TooLong);
    packet.resize(olc::kMaxUdpPacketSize);
    CHECK(parsePacket(packet, header) == UdpHeaderResult::Ok);
}

void testTcpFrame() {
    std::vector<uint8_t> frame = olc::makeTcpFrame(3, {10, 20, 30});
    // Length 4 (type + 3 body bytes), type, body.
    CHECK(frame == std::vector<uint8_t>({4, 0, 0, 0, 3, 10, 20, 30}));

    // Two messages arriving one byte at a time: assembled whole and in order.
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

    // Two messages in one chunk.
    olc::TcpFrameReader together;
    together.append(stream.data(), stream.size());
    CHECK(together.next(type, body) == Frame::Ready && type == 3);
    CHECK(together.next(type, body) == Frame::Ready && type == 9);
    CHECK(together.next(type, body) == Frame::NeedMore);

    // One chunk holds a whole message and the start of the next; the rest comes later.
    olc::TcpFrameReader split;
    size_t cut = frame.size() + 2;
    split.append(stream.data(), cut);
    CHECK(split.next(type, body) == Frame::Ready && type == 3);
    CHECK(split.next(type, body) == Frame::NeedMore);
    split.append(stream.data() + cut, stream.size() - cut);
    CHECK(split.next(type, body) == Frame::Ready && type == 9 && body.empty());
    CHECK(split.next(type, body) == Frame::NeedMore);
}

void testBrokenTcp() {
    uint8_t type = 0;
    std::vector<uint8_t> body;

    // Length 0: not even a type.
    olc::TcpFrameReader zero;
    const uint8_t zeroLength[4] = {0, 0, 0, 0};
    zero.append(zeroLength, sizeof(zeroLength));
    CHECK(zero.next(type, body) == Frame::Broken);

    // Length above 1 MB: the stream is corrupted and is not parsed any further,
    // even if a correct message comes later.
    olc::TcpFrameReader huge;
    const uint8_t hugeLength[4] = {0x01, 0x00, 0x10, 0x00};  // 1 MB + 1
    huge.append(hugeLength, sizeof(hugeLength));
    CHECK(huge.next(type, body) == Frame::Broken);
    std::vector<uint8_t> good = olc::makeTcpFrame(1, {});
    huge.append(good.data(), good.size());
    CHECK(huge.next(type, body) == Frame::Broken);

    // Exactly 1 MB is allowed: we wait for the remaining bytes.
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
