// OLC network protocol constants (docs/PROTOCOL.md) and shared parsing rules.
#pragma once

#include <cstdint>

namespace olc {

// Protocol version: written into every UDP packet and into HELLO (PROTOCOL.md, sections 4 and 9).
constexpr uint16_t kProtocolVersion = 4;

// Default port: the host listens for TCP and UDP on the same number (section 3).
constexpr uint16_t kDefaultPort = 47800;

// Signature at the start of every UDP packet: bytes 'O', 'L', 'C', '0' (section 4).
constexpr uint8_t kUdpMagic[4] = {'O', 'L', 'C', '0'};

// Largest UDP packet: a packet this size is not fragmented on any ordinary network.
constexpr uint32_t kMaxUdpPacketSize = 1200;

// Largest TCP message after the length field: 1 MB.
constexpr uint32_t kMaxTcpMessageSize = 1024 * 1024;

// Whether UDP packet sequence number a is newer than b.
// The number (seq) is a 32-bit counter that wraps to zero after 4294967295.
// So a is newer than b if a is ahead of b by less than half the circle:
// for example, 2 is newer than 4294967290 (the counter wrapped), but 4294967290 is not newer than 2.
bool isSequenceNewer(uint32_t a, uint32_t b);

}  // namespace olc
