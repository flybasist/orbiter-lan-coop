// Message headers (docs/PROTOCOL.md, section 4): the UDP header and length-prefixed TCP frames.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "serialize.h"

namespace olc {

// UDP header size: magic 4 + version 2 + type 1 + session 4 + seq 4 bytes.
constexpr size_t kUdpHeaderSize = 15;

// UDP header fields that differ from packet to packet.
struct UdpHeader {
    uint8_t type = 0;      // message type (PROTOCOL.md, section 6)
    uint32_t session = 0;  // session number from WELCOME
    uint32_t seq = 0;      // sender's sequence number
};

// Result of checking a UDP header. Anything but Ok means the packet is dropped
// and counted in the statistics by reason.
enum class UdpHeaderResult {
    Ok,
    TooShort,      // shorter than the header
    TooLong,       // longer than kMaxUdpPacketSize
    WrongMagic,    // not an OLC packet
    WrongVersion,  // another protocol version
};

// Writes the UDP header: the signature, the current protocol version and the header fields.
void writeUdpHeader(Writer& writer, const UdpHeader& header);

// Reads and checks the UDP header at the start of a packet. reader must be at the start
// of the packet; on Ok it is left right after the header, at the first message field.
// The session number and seq are not checked here: only the receiver knows them.
UdpHeaderResult readUdpHeader(Reader& reader, UdpHeader& header);

// Builds a TCP frame: length (u32), type (u8) and message body.
// The length counts the type and the body, but not itself.
std::vector<uint8_t> makeTcpFrame(uint8_t type, const std::vector<uint8_t>& body);

// TCP is a stream: a message may arrive in parts or together with its neighbours.
// TcpFrameReader collects received bytes and returns whole messages.
class TcpFrameReader {
public:
    enum class Result {
        Ready,     // a message is ready
        NeedMore,  // no whole message yet, waiting for bytes
        Broken,    // length is 0 or larger than kMaxTcpMessageSize: close the connection
    };

    // Adds bytes received from the socket.
    void append(const uint8_t* data, size_t size);

    // Takes the next whole message: type - its type, body - the body without length and type.
    // After Broken the stream cannot be parsed, and all later calls return Broken too.
    Result next(uint8_t& type, std::vector<uint8_t>& body);

private:
    std::vector<uint8_t> pending_;  // received but not yet returned bytes
    bool broken_ = false;
};

}  // namespace olc
