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

}  // namespace olc
