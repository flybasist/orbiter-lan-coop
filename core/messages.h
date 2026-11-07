// Сообщения протокола (docs/PROTOCOL.md, разделы 5 и 6): номера типов, упаковка и разбор.
//
// write... пишет тело сообщения (без длины и типа) и возвращает false, если строка
// длиннее 255 байтов или список длиннее 65535 элементов (у сообщений без строк и
// списков write... ничего не возвращает: ошибиться там нечему). read... разбирает тело и возвращает false, если байтов не хватает
// или после последнего поля остались лишние: такое сообщение считается битым.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "serialize.h"

namespace olc {

// Номера типов TCP-сообщений (раздел 5).
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

// Номера типов UDP-сообщений (раздел 6).
enum class UdpType : uint8_t {
    WorldState = 1,
    Input = 2,
    Ping = 3,
    Pong = 4,
};

// HELLO, клиент → хост: первое сообщение после подключения.
struct Hello {
    uint16_t protocol = 0;        // версия протокола клиента
    uint32_t orbiterVersion = 0;  // oapiGetOrbiterVersion
    std::string pluginVersion;
    std::string player;           // имя игрока
};

// WELCOME, хост → клиент: клиент принят.
struct Welcome {
    uint8_t clientId = 0;
    uint32_t session = 0;      // номер сессии, пишется в каждый UDP-пакет
    uint8_t stateRateHz = 0;   // частота WORLD_STATE
    std::string system;        // имя планетной системы
};

// Причины отказа в REJECT.
enum class RejectReason : uint8_t {
    ProtocolVersion = 1,
    OrbiterVersion = 2,
    OtherSystem = 3,
    NoFreeSlots = 4,
};

// REJECT, хост → клиент: клиент не принят.
struct Reject {
    uint8_t reason = 0;  // RejectReason; число, потому что от чужой версии может прийти любое
    std::string text;    // пояснение для игрока
};

bool writeHello(Writer& writer, const Hello& message);
bool readHello(Reader& reader, Hello& message);

bool writeWelcome(Writer& writer, const Welcome& message);
bool readWelcome(Reader& reader, Welcome& message);

bool writeReject(Writer& writer, const Reject& message);
bool readReject(Reader& reader, Reject& message);

// Номер и имя небесного тела - элемент BODIES.
struct BodyEntry {
    uint16_t netId = 0;
    std::string name;
};

// Номер, имя и класс корабля - элемент VESSELS.
struct VesselEntry {
    uint16_t netId = 0;
    std::string name;
    std::string className;  // класс корабля, например DeltaGlider
};

// Кто управляет кораблём - элемент OWNERSHIP.
struct OwnerEntry {
    uint16_t netId = 0;
    uint8_t owner = 0;  // 0 - хост, 1…254 - клиент, kNoOwner - никто
};
constexpr uint8_t kNoOwner = 255;

// KEY, клиент → хост: разовое нажатие клавиши.
struct KeyPress {
    uint16_t netId = 0;
    uint8_t key = 0;   // код OAPI_KEY_*
    uint8_t mods = 0;  // биты: 0 - Shift, 1 - Ctrl, 2 - Alt
};

// DOCK, хост → клиент: корабли состыковались или расстыковались.
struct Dock {
    uint16_t a = 0;
    uint8_t portA = 0;
    uint16_t b = 0;
    uint8_t portB = 0;
    uint8_t docked = 0;  // 1 - состыкованы, 0 - расстыкованы
};

// LANDED, хост → клиент: корабль встал на поверхность.
struct Landed {
    uint16_t netId = 0;
    double longitude = 0.0;  // рад
    double latitude = 0.0;   // рад
    double heading = 0.0;    // рад
};

// Списки (BODIES, VESSELS, CLASSES_MISSING, OWNERSHIP) начинаются с числа элементов u16,
// поэтому в одном сообщении не больше 65535 элементов: иначе write... вернёт false.
bool writeBodies(Writer& writer, const std::vector<BodyEntry>& bodies);
bool readBodies(Reader& reader, std::vector<BodyEntry>& bodies);

bool writeVessels(Writer& writer, const std::vector<VesselEntry>& vessels);
bool readVessels(Reader& reader, std::vector<VesselEntry>& vessels);

bool writeClassesMissing(Writer& writer, const std::vector<std::string>& classes);
bool readClassesMissing(Reader& reader, std::vector<std::string>& classes);

bool writeOwnership(Writer& writer, const std::vector<OwnerEntry>& owners);
bool readOwnership(Reader& reader, std::vector<OwnerEntry>& owners);

// VESSEL_REMOVED: номер корабля.
void writeVesselRemoved(Writer& writer, uint16_t netId);
bool readVesselRemoved(Reader& reader, uint16_t& netId);

// UDP_READY: UDP-порт клиента.
void writeUdpReady(Writer& writer, uint16_t udpPort);
bool readUdpReady(Reader& reader, uint16_t& udpPort);

void writeKey(Writer& writer, const KeyPress& message);
bool readKey(Reader& reader, KeyPress& message);

void writeDock(Writer& writer, const Dock& message);
bool readDock(Reader& reader, Dock& message);

void writeLanded(Writer& writer, const Landed& message);
bool readLanded(Reader& reader, Landed& message);

// BYE: причина отключения.
bool writeBye(Writer& writer, const std::string& reason);
bool readBye(Reader& reader, std::string& reason);

}  // namespace olc
