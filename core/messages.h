// Protocol messages (docs/PROTOCOL.md, sections 5 and 6): type numbers, packing and parsing.
//
// write... writes the message body (without length and type) and returns false if a string
// is longer than 255 bytes or a list is longer than 65535 items. Messages without strings
// and lists have write... return nothing: nothing can go wrong there.
// read... parses the body and returns false if bytes are missing or extra bytes remain
// after the last field: such a message is treated as broken.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "protocol.h"
#include "serialize.h"

namespace olc {

// TCP message type numbers (section 5).
enum class TcpType : uint8_t {
    Hello = 1,
    Welcome = 2,
    Reject = 3,
    Scenario = 4,
    Bodies = 5,
    Vessels = 6,
    VesselAdded = 7,
    VesselRemoved = 8,
    ClassesMissing = 9,
    Ownership = 10,
    UdpReady = 11,
    Key = 12,
    Dock = 13,
    Landed = 14,
    Bye = 15,
};

// UDP message type numbers (section 6).
enum class UdpType : uint8_t {
    WorldState = 1,
    Input = 2,
    Ping = 3,
    Pong = 4,
    Fuel = 5,
};

// HELLO.scenarioId: the client has no session yet and asks the host for its current scenario.
constexpr uint32_t kNoScenario = 0;
// HELLO.scenarioId: the player started a scenario by hand (as at stage 1), the host does not send one.
constexpr uint32_t kOwnScenario = 0xFFFFFFFF;

// HELLO, client → host: the first message after connecting.
struct Hello {
    uint16_t protocol = 0;        // client protocol version
    uint32_t orbiterVersion = 0;  // oapiGetOrbiterVersion
    uint32_t scenarioId = 0;      // kNoScenario, kOwnScenario or the number from SCENARIO
    std::string pluginVersion;
    std::string player;           // player name
};

// WELCOME, host → client: the client is accepted.
struct Welcome {
    uint8_t clientId = 0;
    uint32_t session = 0;      // session number, written into every UDP packet
    uint8_t stateRateHz = 0;   // WORLD_STATE rate
    std::string system;        // planetary system name
};

// Reasons for REJECT.
enum class RejectReason : uint8_t {
    ProtocolVersion = 1,
    OrbiterVersion = 2,
    OtherSystem = 3,
    NoFreeSlots = 4,
};

// REJECT, host → client: the client is not accepted.
struct Reject {
    uint8_t reason = 0;  // RejectReason; a plain number because another version may send anything
    std::string text;    // explanation for the player
};

bool writeHello(Writer& writer, const Hello& message);
bool readHello(Reader& reader, Hello& message);

bool writeWelcome(Writer& writer, const Welcome& message);
bool readWelcome(Reader& reader, Welcome& message);

bool writeReject(Writer& writer, const Reject& message);
bool readReject(Reader& reader, Reject& message);

// Celestial body number and name - an item of BODIES.
struct BodyEntry {
    uint16_t netId = 0;
    std::string name;
};

// Vessel number, name and class - an item of VESSELS.
struct VesselEntry {
    uint16_t netId = 0;
    std::string name;
    std::string className;  // vessel class, e.g. DeltaGlider
};

// Who controls a vessel - an item of OWNERSHIP.
struct OwnerEntry {
    uint16_t netId = 0;
    uint8_t owner = 0;  // 0 - host, 1…254 - client, kNoOwner - nobody
};
constexpr uint8_t kNoOwner = 255;

// KEY, client → host: a single key press.
struct KeyPress {
    uint16_t netId = 0;
    uint8_t key = 0;   // OAPI_KEY_* code
    uint8_t mods = 0;  // bits: 0 - Shift, 1 - Ctrl, 2 - Alt
};

// DOCK, host → client: vessels docked or undocked.
struct Dock {
    uint16_t a = 0;
    uint8_t portA = 0;
    uint16_t b = 0;
    uint8_t portB = 0;
    uint8_t docked = 0;  // 1 - docked, 0 - undocked
};

// LANDED, host → client: a vessel has come to rest on a surface.
struct Landed {
    uint16_t netId = 0;
    double longitude = 0.0;  // rad
    double latitude = 0.0;   // rad
    double heading = 0.0;    // rad
};

// Lists (BODIES, VESSELS, CLASSES_MISSING, OWNERSHIP) start with a u16 item count,
// so one message holds at most 65535 items: otherwise write... returns false.
bool writeBodies(Writer& writer, const std::vector<BodyEntry>& bodies);
bool readBodies(Reader& reader, std::vector<BodyEntry>& bodies);

bool writeVessels(Writer& writer, const std::vector<VesselEntry>& vessels);
bool readVessels(Reader& reader, std::vector<VesselEntry>& vessels);

bool writeClassesMissing(Writer& writer, const std::vector<std::string>& classes);
bool readClassesMissing(Reader& reader, std::vector<std::string>& classes);

bool writeOwnership(Writer& writer, const std::vector<OwnerEntry>& owners);
bool readOwnership(Reader& reader, std::vector<OwnerEntry>& owners);

// VESSEL_REMOVED: vessel number.
void writeVesselRemoved(Writer& writer, uint16_t netId);
bool readVesselRemoved(Reader& reader, uint16_t& netId);

// UDP_READY: the client's UDP port.
void writeUdpReady(Writer& writer, uint16_t udpPort);
bool readUdpReady(Reader& reader, uint16_t& udpPort);

void writeKey(Writer& writer, const KeyPress& message);
bool readKey(Reader& reader, KeyPress& message);

void writeDock(Writer& writer, const Dock& message);
bool readDock(Reader& reader, Dock& message);

void writeLanded(Writer& writer, const Landed& message);
bool readLanded(Reader& reader, Landed& message);

// BYE: reason for disconnecting.
bool writeBye(Writer& writer, const std::string& reason);
bool readBye(Reader& reader, std::string& reason);

// Three f64 numbers: a vector or angles.
struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

// Where a vessel is - the status field of a vessel state.
enum class VesselStatus : uint8_t {
    Flying = 0,  // flying: position, velocity and attitude are sent
    Landed = 1,  // resting on a surface: the landing place came in LANDED
};

// State of one vessel - an item of WORLD_STATE (also part of VESSEL_ADDED).
struct VesselState {
    uint16_t netId = 0;
    uint8_t status = 0;    // VesselStatus
    uint16_t refBody = 0;  // reference body number from BODIES
    Vec3 rpos;             // position relative to the reference body, m (flying only)
    Vec3 rvel;             // velocity relative to the reference body, m/s (flying only)
    Vec3 arot;             // attitude, Euler angles as in VESSELSTATUS2 (flying only)
    Vec3 vrot;             // angular velocity, rad/s (flying only)
    std::vector<uint8_t> thrusters;  // thruster levels 0…255 → 0…1
};

// The thruster count and the vessel count in a packet are written as one byte.
constexpr size_t kMaxThrusters = 255;
constexpr size_t kMaxVesselsPerPacket = 255;

// A vessel state is part of a message and other fields may follow it, so
// readVesselState does not check for extra bytes at the end.
// writeVesselState returns false if there are more than kMaxThrusters thrusters.
// readVesselState returns false if bytes are missing or status is neither 0 nor 1:
// status decides which fields follow, so the rest of the packet cannot be parsed.
bool writeVesselState(Writer& writer, const VesselState& state);
bool readVesselState(Reader& reader, VesselState& state);

// WORLD_STATE, host → client: vessel states at the moment simTime.
struct WorldState {
    double simTime = 0.0;  // host oapiGetSimTime
    double mjd = 0.0;      // oapiGetSimMJD
    double warp = 1.0;     // time acceleration
    uint8_t paused = 0;    // 1 - paused
    std::vector<VesselState> vessels;  // at most kMaxVesselsPerPacket
};

// Writes and reads only the body: the UDP header is writeUdpHeader and readUdpHeader.
// writeWorldState returns false if there are more than kMaxVesselsPerPacket vessels or
// some vessel has too many thrusters. Whether the packet fits kMaxUdpPacketSize is
// checked by the sender using writer.bytes(): vessels that do not fit go into the
// next packet with the same simTime.
bool writeWorldState(Writer& writer, const WorldState& message);
bool readWorldState(Reader& reader, WorldState& message);

// Fuel of one vessel - an item of FUEL.
struct VesselFuel {
    uint16_t netId = 0;
    double emptyMass = 0.0;           // empty mass, kg (GetEmptyMass)
    std::vector<float> tankLevels;    // tank fill 0…1, in tank index order
};

// The tank count is written as one byte.
constexpr size_t kMaxTanks = 255;

// FUEL, host → client, once per second: fuel and empty mass of vessels.
// Vessels that do not fit into one packet go into the next FUEL packet.
struct Fuel {
    std::vector<VesselFuel> vessels;  // at most kMaxVesselsPerPacket
};

// Writes and reads only the body. writeFuel returns false if there are more than
// kMaxVesselsPerPacket vessels or some vessel has more than kMaxTanks tanks.
bool writeFuel(Writer& writer, const Fuel& message);
bool readFuel(Reader& reader, Fuel& message);

// VESSEL_ADDED, host → client: a new vessel appeared on the host.
// Body: name, class and vessel state as in WORLD_STATE (it starts with net_id).
struct VesselAdded {
    std::string name;
    std::string className;  // vessel class, e.g. DeltaGlider
    VesselState state;      // vessel number is state.netId
};

// writeVesselAdded returns false if the name or class is longer than 255 bytes or the
// vessel has more than kMaxThrusters thrusters.
bool writeVesselAdded(Writer& writer, const VesselAdded& message);
bool readVesselAdded(Reader& reader, VesselAdded& message);

// The throttle value in INPUT when there is no throttle lever or it has not moved.
constexpr float kNoThrottle = -1.0f;

// INPUT, client → host: the full current state of the controls (section 6).
// Triples are per axis: rot - pitch, yaw, bank; lin - up-down,
// left-right, forward-back. Values from -1 to 1.
struct Input {
    uint16_t netId = 0;              // vessel controlled by the client
    uint32_t keys = 0;               // keys held down, bits - PROTOCOL.md, section 7
    float rot[3] = {0.0f, 0.0f, 0.0f};
    float lin[3] = {0.0f, 0.0f, 0.0f};
    float throttle = kNoThrottle;    // throttle 0…1 or kNoThrottle
};

// UDP message bodies: the header is writeUdpHeader and readUdpHeader.
void writeInput(Writer& writer, const Input& message);
bool readInput(Reader& reader, Input& message);

// PING and PONG: the sender's system time (oapiGetSysTime) and the client number from WELCOME.
// PONG returns the PING body unchanged, so both messages share the same body.
// From PING the host learns the client's real UDP address: if the client is behind NAT,
// the port the host sees may differ from the port in UDP_READY.
struct Ping {
    double time = 0.0;
    uint8_t clientId = 0;
};
void writePing(Writer& writer, const Ping& message);
bool readPing(Reader& reader, Ping& message);

// Largest file size in SCENARIO: the whole message (type, scenario number, longest name,
// size field and file) must fit into a TCP frame of kMaxTcpMessageSize.
constexpr size_t kMaxScenarioSize = kMaxTcpMessageSize - 1 - 4 - (1 + kMaxStringLength) - 4;

// SCENARIO, host → client: a scenario file with the host's current world (stage 2).
struct Scenario {
    uint32_t scenarioId = 0;    // save number; the client returns it in HELLO after loading
    std::string name;           // scenario file name
    std::vector<uint8_t> data;  // file contents as is
};

// writeScenario returns false if the name is longer than 255 bytes or the file is larger
// than kMaxScenarioSize. Besides the usual checks, readScenario rejects a message whose
// size field does not match the number of remaining bytes.
bool writeScenario(Writer& writer, const Scenario& message);
bool readScenario(Reader& reader, Scenario& message);

}  // namespace olc
