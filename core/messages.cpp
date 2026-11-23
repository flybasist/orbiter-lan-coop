#include "messages.h"

#include <cmath>

namespace olc {

namespace {

// Whether a control value from the network is a number from -1 to 1 (NaN and infinity are not).
bool isAxisValue(float value) {
    return std::isfinite(value) && value >= -1.0f && value <= 1.0f;
}

// Common check at the end of parsing: all reads succeeded and there are no extra bytes.
bool finished(const Reader& reader) {
    return reader.ok() && reader.remaining() == 0;
}

// Writes the list item count; false if there are more than fit into u16.
bool writeCount(Writer& writer, size_t count) {
    if (count > 65535) {
        return false;
    }
    writer.writeU16(static_cast<uint16_t>(count));
    return true;
}

}  // namespace

bool writeHello(Writer& writer, const Hello& message) {
    writer.writeU16(message.protocol);
    writer.writeU32(message.orbiterVersion);
    writer.writeU32(message.scenarioId);
    if (!writer.writeString(message.pluginVersion) || !writer.writeString(message.player)) {
        return false;
    }
    writer.writeU8(message.flags);
    return true;
}

bool readHello(Reader& reader, Hello& message) {
    reader.readU16(message.protocol);
    reader.readU32(message.orbiterVersion);
    reader.readU32(message.scenarioId);
    reader.readString(message.pluginVersion);
    reader.readString(message.player);
    reader.readU8(message.flags);
    return finished(reader);
}

bool writeWelcome(Writer& writer, const Welcome& message) {
    writer.writeU8(message.clientId);
    writer.writeU32(message.session);
    writer.writeU8(message.stateRateHz);
    if (!writer.writeString(message.system)) {
        return false;
    }
    writer.writeU8(message.flags);
    return true;
}

bool readWelcome(Reader& reader, Welcome& message) {
    reader.readU8(message.clientId);
    reader.readU32(message.session);
    reader.readU8(message.stateRateHz);
    reader.readString(message.system);
    reader.readU8(message.flags);
    return finished(reader);
}

bool writeReject(Writer& writer, const Reject& message) {
    writer.writeU8(message.reason);
    return writer.writeString(message.text);
}

bool readReject(Reader& reader, Reject& message) {
    reader.readU8(message.reason);
    reader.readString(message.text);
    return finished(reader);
}

// ---------- Lists ----------
// When parsing a list we do not reserve the item count from the packet in advance: a broken packet
// may promise 65535 items. The loop stops at the first failed read.

bool writeBodies(Writer& writer, const std::vector<BodyEntry>& bodies) {
    if (!writeCount(writer, bodies.size())) {
        return false;
    }
    for (const BodyEntry& body : bodies) {
        writer.writeU16(body.netId);
        if (!writer.writeString(body.name)) {
            return false;
        }
    }
    return true;
}

bool readBodies(Reader& reader, std::vector<BodyEntry>& bodies) {
    bodies.clear();
    uint16_t count = 0;
    reader.readU16(count);
    for (uint16_t i = 0; i < count && reader.ok(); i++) {
        BodyEntry body;
        reader.readU16(body.netId);
        reader.readString(body.name);
        bodies.push_back(body);
    }
    return finished(reader);
}

bool writeVessels(Writer& writer, const std::vector<VesselEntry>& vessels) {
    if (!writeCount(writer, vessels.size())) {
        return false;
    }
    for (const VesselEntry& vessel : vessels) {
        writer.writeU16(vessel.netId);
        if (!writer.writeString(vessel.name) || !writer.writeString(vessel.className)) {
            return false;
        }
    }
    return true;
}

bool readVessels(Reader& reader, std::vector<VesselEntry>& vessels) {
    vessels.clear();
    uint16_t count = 0;
    reader.readU16(count);
    for (uint16_t i = 0; i < count && reader.ok(); i++) {
        VesselEntry vessel;
        reader.readU16(vessel.netId);
        reader.readString(vessel.name);
        reader.readString(vessel.className);
        vessels.push_back(vessel);
    }
    return finished(reader);
}

bool writeClassesMissing(Writer& writer, const std::vector<std::string>& classes) {
    if (!writeCount(writer, classes.size())) {
        return false;
    }
    for (const std::string& className : classes) {
        if (!writer.writeString(className)) {
            return false;
        }
    }
    return true;
}

bool readClassesMissing(Reader& reader, std::vector<std::string>& classes) {
    classes.clear();
    uint16_t count = 0;
    reader.readU16(count);
    for (uint16_t i = 0; i < count && reader.ok(); i++) {
        std::string className;
        reader.readString(className);
        classes.push_back(className);
    }
    return finished(reader);
}

bool writeOwnership(Writer& writer, const std::vector<OwnerEntry>& owners) {
    if (!writeCount(writer, owners.size())) {
        return false;
    }
    for (const OwnerEntry& entry : owners) {
        writer.writeU16(entry.netId);
        writer.writeU8(entry.owner);
    }
    return true;
}

bool readOwnership(Reader& reader, std::vector<OwnerEntry>& owners) {
    owners.clear();
    uint16_t count = 0;
    reader.readU16(count);
    for (uint16_t i = 0; i < count && reader.ok(); i++) {
        OwnerEntry entry;
        reader.readU16(entry.netId);
        reader.readU8(entry.owner);
        owners.push_back(entry);
    }
    return finished(reader);
}

// ---------- Short messages ----------

void writeVesselRemoved(Writer& writer, uint16_t netId) {
    writer.writeU16(netId);
}

bool readVesselRemoved(Reader& reader, uint16_t& netId) {
    reader.readU16(netId);
    return finished(reader);
}

void writeUdpReady(Writer& writer, uint16_t udpPort) {
    writer.writeU16(udpPort);
}

bool readUdpReady(Reader& reader, uint16_t& udpPort) {
    reader.readU16(udpPort);
    return finished(reader);
}

void writeKey(Writer& writer, const KeyPress& message) {
    writer.writeU16(message.netId);
    writer.writeU8(message.key);
    writer.writeU8(message.mods);
}

bool readKey(Reader& reader, KeyPress& message) {
    reader.readU16(message.netId);
    reader.readU8(message.key);
    reader.readU8(message.mods);
    return finished(reader);
}

void writeOwnerRequest(Writer& writer, const OwnerRequest& message) {
    writer.writeU16(message.netId);
    writer.writeU8(message.action);
}

bool readOwnerRequest(Reader& reader, OwnerRequest& message) {
    reader.readU16(message.netId);
    reader.readU8(message.action);
    return finished(reader);
}

void writeOwnerDenied(Writer& writer, const OwnerDenied& message) {
    writer.writeU16(message.netId);
    writer.writeU8(message.reason);
}

bool readOwnerDenied(Reader& reader, OwnerDenied& message) {
    reader.readU16(message.netId);
    reader.readU8(message.reason);
    return finished(reader);
}

void writeDock(Writer& writer, const Dock& message) {
    writer.writeU16(message.a);
    writer.writeU8(message.portA);
    writer.writeU16(message.b);
    writer.writeU8(message.portB);
    writer.writeU8(message.docked);
}

bool sameDockLink(const Dock& x, const Dock& y) {
    const bool direct = x.a == y.a && x.portA == y.portA && x.b == y.b && x.portB == y.portB;
    const bool swapped = x.a == y.b && x.portA == y.portB && x.b == y.a && x.portB == y.portA;
    return direct || swapped;
}

bool readDock(Reader& reader, Dock& message) {
    reader.readU16(message.a);
    reader.readU8(message.portA);
    reader.readU16(message.b);
    reader.readU8(message.portB);
    reader.readU8(message.docked);
    return finished(reader);
}

void writeLanded(Writer& writer, const Landed& message) {
    writer.writeU16(message.netId);
    writer.writeF64(message.longitude);
    writer.writeF64(message.latitude);
    writer.writeF64(message.heading);
}

bool readLanded(Reader& reader, Landed& message) {
    reader.readU16(message.netId);
    reader.readF64(message.longitude);
    reader.readF64(message.latitude);
    reader.readF64(message.heading);
    return finished(reader);
}

bool writeBye(Writer& writer, const std::string& reason) {
    return writer.writeString(reason);
}

bool readBye(Reader& reader, std::string& reason) {
    reader.readString(reason);
    return finished(reader);
}

namespace {

void writeVec3(Writer& writer, const Vec3& value) {
    writer.writeF64(value.x);
    writer.writeF64(value.y);
    writer.writeF64(value.z);
}

void readVec3(Reader& reader, Vec3& value) {
    reader.readF64(value.x);
    reader.readF64(value.y);
    reader.readF64(value.z);
}

}  // namespace

bool writeVesselState(Writer& writer, const VesselState& state) {
    if (state.thrusters.size() > kMaxThrusters) {
        return false;
    }
    writer.writeU16(state.netId);
    writer.writeU8(state.status);
    writer.writeU16(state.refBody);
    // Position and attitude of a landed vessel are not sent.
    if (state.status == static_cast<uint8_t>(VesselStatus::Flying)) {
        writeVec3(writer, state.rpos);
        writeVec3(writer, state.rvel);
        writeVec3(writer, state.arot);
        writeVec3(writer, state.vrot);
    }
    writer.writeU8(static_cast<uint8_t>(state.thrusters.size()));
    for (uint8_t level : state.thrusters) {
        writer.writeU8(level);
    }
    return true;
}

bool readVesselState(Reader& reader, VesselState& state) {
    reader.readU16(state.netId);
    reader.readU8(state.status);
    reader.readU16(state.refBody);
    if (!reader.ok()) {
        return false;
    }

    if (state.status == static_cast<uint8_t>(VesselStatus::Flying)) {
        readVec3(reader, state.rpos);
        readVec3(reader, state.rvel);
        readVec3(reader, state.arot);
        readVec3(reader, state.vrot);
    } else if (state.status == static_cast<uint8_t>(VesselStatus::Landed)) {
        // No position fields: zero them so that no values remain from the previous packet.
        state.rpos = Vec3();
        state.rvel = Vec3();
        state.arot = Vec3();
        state.vrot = Vec3();
    } else {
        return false;  // unknown status: unclear which fields follow
    }

    uint8_t count = 0;
    reader.readU8(count);
    state.thrusters.assign(count, 0);
    for (uint8_t& level : state.thrusters) {
        reader.readU8(level);
    }
    return reader.ok();
}

bool writeWorldState(Writer& writer, const WorldState& message) {
    if (message.vessels.size() > kMaxVesselsPerPacket) {
        return false;
    }
    writer.writeF64(message.simTime);
    writer.writeF64(message.mjd);
    writer.writeF64(message.warp);
    writer.writeU8(message.paused);
    writer.writeU8(static_cast<uint8_t>(message.vessels.size()));
    for (const VesselState& state : message.vessels) {
        if (!writeVesselState(writer, state)) {
            return false;
        }
    }
    return true;
}

bool readWorldState(Reader& reader, WorldState& message) {
    reader.readF64(message.simTime);
    reader.readF64(message.mjd);
    reader.readF64(message.warp);
    reader.readU8(message.paused);
    uint8_t count = 0;
    reader.readU8(count);
    if (!reader.ok()) {
        return false;
    }
    // The vessel count came from the network, so we do not reserve space in advance but add
    // vessels one by one while they actually read.
    message.vessels.clear();
    for (int i = 0; i < count; i++) {
        VesselState state;
        if (!readVesselState(reader, state)) {
            return false;
        }
        message.vessels.push_back(state);
    }
    return finished(reader);
}

// ---------- FUEL ----------

bool writeFuel(Writer& writer, const Fuel& message) {
    if (message.vessels.size() > kMaxVesselsPerPacket) {
        return false;
    }
    writer.writeU8(static_cast<uint8_t>(message.vessels.size()));
    for (const VesselFuel& fuel : message.vessels) {
        if (fuel.tankLevels.size() > kMaxTanks) {
            return false;
        }
        writer.writeU16(fuel.netId);
        writer.writeF64(fuel.emptyMass);
        writer.writeU8(static_cast<uint8_t>(fuel.tankLevels.size()));
        for (float level : fuel.tankLevels) {
            writer.writeF32(level);
        }
    }
    return true;
}

bool readFuel(Reader& reader, Fuel& message) {
    uint8_t count = 0;
    reader.readU8(count);
    if (!reader.ok()) {
        return false;
    }
    // As in WORLD_STATE: the numbers came from the network, so add one by one while reading works.
    message.vessels.clear();
    for (int i = 0; i < count; i++) {
        VesselFuel fuel;
        reader.readU16(fuel.netId);
        reader.readF64(fuel.emptyMass);
        uint8_t tanks = 0;
        reader.readU8(tanks);
        for (int t = 0; t < tanks && reader.ok(); t++) {
            float level = 0.0f;
            reader.readF32(level);
            fuel.tankLevels.push_back(level);
        }
        if (!reader.ok()) {
            return false;
        }
        message.vessels.push_back(fuel);
    }
    return finished(reader);
}

// ---------- VESSEL_ADDED ----------

bool writeVesselAdded(Writer& writer, const VesselAdded& message) {
    return writer.writeString(message.name) && writer.writeString(message.className) &&
           writeVesselState(writer, message.state);
}

bool readVesselAdded(Reader& reader, VesselAdded& message) {
    reader.readString(message.name);
    reader.readString(message.className);
    if (!readVesselState(reader, message.state)) {
        return false;
    }
    return finished(reader);
}

// ---------- INPUT, PING, PONG ----------

void writeInput(Writer& writer, const Input& message) {
    writer.writeU16(message.netId);
    writer.writeU32(message.keys);
    for (float value : message.rot) {
        writer.writeF32(value);
    }
    for (float value : message.lin) {
        writer.writeF32(value);
    }
    writer.writeF32(message.throttle);
}

bool readInput(Reader& reader, Input& message) {
    reader.readU16(message.netId);
    reader.readU32(message.keys);
    for (float& value : message.rot) {
        reader.readF32(value);
    }
    for (float& value : message.lin) {
        reader.readF32(value);
    }
    reader.readF32(message.throttle);
    if (!finished(reader)) {
        return false;
    }
    // The host passes these values to the vessel's thrusters: a broken client must not send
    // anything out of range there.
    for (int i = 0; i < 3; i++) {
        if (!isAxisValue(message.rot[i]) || !isAxisValue(message.lin[i])) {
            return false;
        }
    }
    return message.throttle == kNoThrottle ||
           (std::isfinite(message.throttle) && message.throttle >= 0.0f && message.throttle <= 1.0f);
}

void writePing(Writer& writer, const Ping& message) {
    writer.writeF64(message.time);
    writer.writeU8(message.clientId);
    writer.writeU16(message.rttMs);
}

bool readPing(Reader& reader, Ping& message) {
    reader.readF64(message.time);
    reader.readU8(message.clientId);
    reader.readU16(message.rttMs);
    return finished(reader);
}

// ---------- TIME_REQUEST and PLAYERS ----------

void writeTimeRequest(Writer& writer, const TimeRequest& message) {
    writer.writeU8(message.paused);
    writer.writeF64(message.warp);
}

bool readTimeRequest(Reader& reader, TimeRequest& message) {
    reader.readU8(message.paused);
    reader.readF64(message.warp);
    return finished(reader) && message.paused <= 1 && std::isfinite(message.warp) &&
           message.warp > 0.0;
}

bool writePlayers(Writer& writer, const std::vector<PlayerEntry>& players) {
    if (players.size() > 255) {
        return false;
    }
    writer.writeU8(static_cast<uint8_t>(players.size()));
    for (const PlayerEntry& entry : players) {
        writer.writeU8(entry.clientId);
        if (!writer.writeString(entry.player)) {
            return false;
        }
        writer.writeU8(entry.flags);
        writer.writeU16(entry.netId);
        writer.writeU16(entry.rttMs);
    }
    return true;
}

bool readPlayers(Reader& reader, std::vector<PlayerEntry>& players) {
    players.clear();
    uint8_t count = 0;
    reader.readU8(count);
    for (uint8_t i = 0; i < count && reader.ok(); i++) {
        PlayerEntry entry;
        reader.readU8(entry.clientId);
        reader.readString(entry.player);
        reader.readU8(entry.flags);
        reader.readU16(entry.netId);
        reader.readU16(entry.rttMs);
        players.push_back(entry);
    }
    return finished(reader);
}

// ---------- SCENARIO ----------

bool writeScenario(Writer& writer, const Scenario& message) {
    if (message.data.size() > kMaxScenarioSize) {
        return false;
    }
    writer.writeU32(message.scenarioId);
    if (!writer.writeString(message.name)) {
        return false;
    }
    writer.writeU32(static_cast<uint32_t>(message.data.size()));
    writer.writeBytes(message.data);
    return true;
}

bool readScenario(Reader& reader, Scenario& message) {
    uint32_t size = 0;
    reader.readU32(message.scenarioId);
    reader.readString(message.name);
    reader.readU32(size);
    // size came from the network: first compare it with what is actually in the packet,
    // and only then read that many bytes.
    if (!reader.ok() || size > kMaxScenarioSize || size != reader.remaining()) {
        return false;
    }
    reader.readBytes(message.data, size);
    return finished(reader);
}

// ---------- ANIM_STATE and NAV_STATE ----------

bool writeAnimState(Writer& writer, const AnimState& message) {
    if (message.anims.size() > 65535) {
        return false;
    }
    writer.writeU16(message.netId);
    writer.writeU16(static_cast<uint16_t>(message.anims.size()));
    for (const AnimValue& anim : message.anims) {
        writer.writeU16(anim.index);
        writer.writeF32(anim.value);
    }
    return true;
}

bool readAnimState(Reader& reader, AnimState& message) {
    message.anims.clear();
    uint16_t count = 0;
    reader.readU16(message.netId);
    reader.readU16(count);
    // Every item takes 6 bytes: a count larger than the rest of the message is a broken message.
    if (!reader.ok() || reader.remaining() != static_cast<size_t>(count) * 6) {
        return false;
    }
    for (uint16_t i = 0; i < count; i++) {
        AnimValue anim;
        reader.readU16(anim.index);
        reader.readF32(anim.value);
        // NaN, infinity and values outside 0...1 are not an animation state.
        if (!(anim.value >= 0.0f && anim.value <= 1.0f)) {
            return false;
        }
        message.anims.push_back(anim);
    }
    return finished(reader);
}

bool writeNavState(Writer& writer, const NavState& message) {
    if (message.channels.size() > kMaxNavChannels) {
        return false;
    }
    writer.writeU16(message.netId);
    writer.writeU16(message.transponder);
    writer.writeU8(static_cast<uint8_t>(message.channels.size()));
    for (uint16_t channel : message.channels) {
        writer.writeU16(channel);
    }
    return true;
}

bool readNavState(Reader& reader, NavState& message) {
    message.channels.clear();
    uint8_t count = 0;
    reader.readU16(message.netId);
    reader.readU16(message.transponder);
    reader.readU8(count);
    if (!reader.ok() || count > kMaxNavChannels) {
        return false;
    }
    for (uint8_t i = 0; i < count; i++) {
        uint16_t channel = 0;
        reader.readU16(channel);
        message.channels.push_back(channel);
    }
    return finished(reader);
}

}  // namespace olc
