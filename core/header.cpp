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

UdpHeaderResult readUdpHeader(Reader& reader, UdpHeader& header) {
    if (reader.remaining() > kMaxUdpPacketSize) {
        return UdpHeaderResult::TooLong;
    }
    if (reader.remaining() < kUdpHeaderSize) {
        return UdpHeaderResult::TooShort;
    }

    // Байтов на заголовок хватает (проверено выше), поэтому чтения ниже не ошибаются.
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
    writer.writeU32(static_cast<uint32_t>(1 + body.size()));  // тип + тело
    writer.writeU8(type);
    std::vector<uint8_t> frame = writer.bytes();
    frame.insert(frame.end(), body.begin(), body.end());
    return frame;
}

void TcpFrameReader::append(const uint8_t* data, size_t size) {
    pending_.insert(pending_.end(), data, data + size);
}

TcpFrameReader::Result TcpFrameReader::next(uint8_t& type, std::vector<uint8_t>& body) {
    if (broken_) {
        return Result::Broken;
    }

    // Сначала нужна длина: 4 байта.
    Reader reader(pending_.data(), pending_.size());
    uint32_t length = 0;
    if (!reader.readU32(length)) {
        return Result::NeedMore;
    }
    // В сообщении есть хотя бы тип. Слишком длинное - признак чужого или испорченного потока.
    if (length == 0 || length > kMaxTcpMessageSize) {
        broken_ = true;
        return Result::Broken;
    }
    if (reader.remaining() < length) {
        return Result::NeedMore;
    }

    // Сообщение пришло целиком: выдаём его и убираем из накопленного.
    reader.readU8(type);
    auto bodyStart = pending_.begin() + 4 + 1;
    auto bodyEnd = pending_.begin() + 4 + length;
    body.assign(bodyStart, bodyEnd);
    pending_.erase(pending_.begin(), bodyEnd);
    return Result::Ready;
}

}  // namespace olc
