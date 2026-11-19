// OLC network protocol constants (docs/PROTOCOL.md) and shared parsing rules.
#pragma once

#include <cstdint>

namespace olc {

// Protocol version: written into every UDP packet and into HELLO (PROTOCOL.md, sections 4 and 9).
constexpr uint16_t kProtocolVersion = 6;

// Default port: the host listens for TCP and UDP on the same number (section 3).
constexpr uint16_t kDefaultPort = 47800;

// Signature at the start of every UDP packet: bytes 'O', 'L', 'C', '0' (section 4).
constexpr uint8_t kUdpMagic[4] = {'O', 'L', 'C', '0'};

// Largest UDP packet: a packet this size is not fragmented on any ordinary network.
constexpr uint32_t kMaxUdpPacketSize = 1200;

// Largest TCP message after the length field: 1 MB.
constexpr uint32_t kMaxTcpMessageSize = 1024 * 1024;

// After the handshake both sides hear each other at least once a second: the client sends PING,
// the host sends WORLD_STATE and PONG, also while paused. A side that has heard nothing for this
// many seconds treats the other one as gone (a frozen computer or a pulled cable leaves the TCP
// connection open). Long enough for a slow frame, such as loading textures, to pass.
constexpr double kSilenceTimeout = 15.0;

// Whether UDP packet sequence number a is newer than b.
// The number (seq) is a 32-bit counter that wraps to zero after 4294967295.
// So a is newer than b if a is ahead of b by less than half the circle:
// for example, 2 is newer than 4294967290 (the counter wrapped), but 4294967290 is not newer than 2.
bool isSequenceNewer(uint32_t a, uint32_t b);

// When the next periodic packet (INPUT, WORLD_STATE) may go out, after one went out at now; planned -
// the time this one was planned for, period - 1 / rate. The next time counts from planned, not from
// now: a sender whose frames come at about the limit rate (60 frames and 60 INPUT per second) then
// sends in every frame instead of every other one. If the sender is late by more than a period (a
// long frame), it counts from now, so no burst of packets follows.
double nextSendTime(double planned, double now, double period);

}  // namespace olc
