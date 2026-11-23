#include "header.h"

#include "protocol.h"

namespace olc {

void writeUdpHeader(Writer& writer, const UdpHeader& header) {
    for (uint8_t byte : kUdpMagic) {
        writer.writeU8(byte);
    }
    writer.writeU16(kProtocolVersion);
    writer.writeU8(header.type);
    writer.writeU32(header.session);
    writer.writeU32(header.seq);
}

std::vector<uint8_t> makeUdpPacket(uint8_t type, uint32_t session, uint32_t seq,
                                   const std::vector<uint8_t>& body) {
    UdpHeader header;
    header.type = type;
    header.session = session;
    header.seq = seq;
    Writer writer;
    writeUdpHeader(writer, header);
    writer.writeBytes(body);
    return writer.bytes();
}

UdpHeaderResult readUdpHeader(Reader& reader, UdpHeader& header) {
    if (reader.remaining() > kMaxUdpPacketSize) {
        return UdpHeaderResult::TooLong;
    }
    if (reader.remaining() < kUdpHeaderSize) {
        return UdpHeaderResult::TooShort;
    }

    // There are enough bytes for the header (checked above), so the reads below cannot fail.
    for (uint8_t expected : kUdpMagic) {
        uint8_t byte = 0;
        reader.readU8(byte);
        if (byte != expected) {
            return UdpHeaderResult::WrongMagic;
        }
    }
    uint16_t version = 0;
    reader.readU16(version);
    if (version != kProtocolVersion) {
        return UdpHeaderResult::WrongVersion;
    }
    reader.readU8(header.type);
    reader.readU32(header.session);
    reader.readU32(header.seq);
    return UdpHeaderResult::Ok;
}

std::vector<uint8_t> makeTcpFrame(uint8_t type, const std::vector<uint8_t>& body) {
    Writer writer;
    writer.writeU32(static_cast<uint32_t>(1 + body.size()));  // type + body
    writer.writeU8(type);
    std::vector<uint8_t> frame = writer.bytes();
    frame.insert(frame.end(), body.begin(), body.end());
    return frame;
}

void TcpFrameReader::append(const uint8_t* data, size_t size) {
    // Returned messages are removed here, once per received chunk, and not in next():
    // erasing the buffer start after every message is slow when many small messages
    // arrive together.
    pending_.erase(pending_.begin(), pending_.begin() + start_);
    start_ = 0;
    pending_.insert(pending_.end(), data, data + size);
}

TcpFrameReader::Result TcpFrameReader::next(uint8_t& type, std::vector<uint8_t>& body) {
    if (broken_) {
        return Result::Broken;
    }

    // First the length is needed: 4 bytes.
    Reader reader(pending_.data() + start_, pending_.size() - start_);
    uint32_t length = 0;
    if (!reader.readU32(length)) {
        return Result::NeedMore;
    }
    // The message has at least a type. Too long means a foreign or corrupted stream.
    if (length == 0 || length > kMaxTcpMessageSize) {
        broken_ = true;
        return Result::Broken;
    }
    if (reader.remaining() < length) {
        return Result::NeedMore;
    }

    // The message arrived whole: return it and move start_ past it.
    reader.readU8(type);
    auto bodyStart = pending_.begin() + start_ + 4 + 1;
    auto bodyEnd = pending_.begin() + start_ + 4 + length;
    body.assign(bodyStart, bodyEnd);
    start_ += 4 + length;
    return Result::Ready;
}

}  // namespace olc
