#include "messages.h"

namespace olc {

namespace {

// Общая проверка в конце разбора: все чтения удались и лишних байтов нет.
bool finished(const Reader& reader) {
    return reader.ok() && reader.remaining() == 0;
}

// Пишет число элементов списка; false, если их больше, чем помещается в u16.
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
    return writer.writeString(message.pluginVersion) && writer.writeString(message.player);
}

bool readHello(Reader& reader, Hello& message) {
    reader.readU16(message.protocol);
    reader.readU32(message.orbiterVersion);
    reader.readString(message.pluginVersion);
    reader.readString(message.player);
    return finished(reader);
}

bool writeWelcome(Writer& writer, const Welcome& message) {
    writer.writeU8(message.clientId);
    writer.writeU32(message.session);
    writer.writeU8(message.stateRateHz);
    return writer.writeString(message.system);
}

bool readWelcome(Reader& reader, Welcome& message) {
    reader.readU8(message.clientId);
    reader.readU32(message.session);
    reader.readU8(message.stateRateHz);
    reader.readString(message.system);
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

// ---------- Списки ----------
// При разборе списка число элементов из пакета заранее не резервируем: битый пакет
// может обещать 65535 элементов. Цикл прерывается на первой неудачной записи.

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

// ---------- Короткие сообщения ----------

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

void writeDock(Writer& writer, const Dock& message) {
    writer.writeU16(message.a);
    writer.writeU8(message.portA);
    writer.writeU16(message.b);
    writer.writeU8(message.portB);
    writer.writeU8(message.docked);
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
    // Положение и ориентацию стоящего на поверхности корабля не передаём.
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
        // Полей положения нет: обнуляем, чтобы не осталось значений от прошлого пакета.
        state.rpos = Vec3();
        state.rvel = Vec3();
        state.arot = Vec3();
        state.vrot = Vec3();
    } else {
        return false;  // неизвестный status: непонятно, какие поля дальше
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
    // Число кораблей пришло из сети, поэтому место не резервируем заранее, а добавляем
    // корабли по одному, пока они действительно читаются.
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
    // Как в WORLD_STATE: числа пришли из сети, поэтому добавляем по одному, пока читается.
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
    return finished(reader);
}

void writePingTime(Writer& writer, double time) {
    writer.writeF64(time);
}

bool readPingTime(Reader& reader, double& time) {
    reader.readF64(time);
    return finished(reader);
}

// ---------- SCENARIO ----------

bool writeScenario(Writer& writer, const Scenario& message) {
    if (message.data.size() > kMaxScenarioSize) {
        return false;
    }
    if (!writer.writeString(message.name)) {
        return false;
    }
    writer.writeU32(static_cast<uint32_t>(message.data.size()));
    writer.writeBytes(message.data);
    return true;
}

bool readScenario(Reader& reader, Scenario& message) {
    uint32_t size = 0;
    reader.readString(message.name);
    reader.readU32(size);
    // size пришёл из сети: сначала сверяем его с тем, что действительно есть в пакете,
    // и только потом читаем столько байтов.
    if (!reader.ok() || size > kMaxScenarioSize || size != reader.remaining()) {
        return false;
    }
    reader.readBytes(message.data, size);
    return finished(reader);
}

}  // namespace olc
