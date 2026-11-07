// Сообщения протокола (docs/PROTOCOL.md, разделы 5 и 6): номера типов, упаковка и разбор.
//
// write... пишет тело сообщения (без длины и типа) и возвращает false, если строка
// длиннее 255 байтов или список длиннее 65535 элементов. У сообщений без строк и
// списков write... ничего не возвращает: ошибиться там нечему.
// read... разбирает тело и возвращает false, если байтов не хватает или после
// последнего поля остались лишние: такое сообщение считается битым.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "protocol.h"
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
    Fuel = 5,
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

// Три числа f64: вектор или углы.
struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

// Где находится корабль - поле status в состоянии корабля.
enum class VesselStatus : uint8_t {
    Flying = 0,  // летит: передаются положение, скорость и ориентация
    Landed = 1,  // стоит на поверхности: место посадки пришло в LANDED
};

// Состояние одного корабля - элемент WORLD_STATE (оно же войдёт в VESSEL_ADDED).
struct VesselState {
    uint16_t netId = 0;
    uint8_t status = 0;    // VesselStatus
    uint16_t refBody = 0;  // номер опорного тела из BODIES
    Vec3 rpos;             // положение относительно опорного тела, м (только в полёте)
    Vec3 rvel;             // скорость относительно опорного тела, м/с (только в полёте)
    Vec3 arot;             // ориентация, углы Эйлера как в VESSELSTATUS2 (только в полёте)
    Vec3 vrot;             // угловая скорость, рад/с (только в полёте)
    std::vector<uint8_t> thrusters;  // уровни двигателей 0…255 → 0…1
};

// Число двигателей и число кораблей в пакете пишутся одним байтом.
constexpr size_t kMaxThrusters = 255;
constexpr size_t kMaxVesselsPerPacket = 255;

// Состояние корабля - часть сообщения, за ним могут идти другие поля, поэтому
// readVesselState не проверяет лишние байты в конце.
// writeVesselState возвращает false, если двигателей больше kMaxThrusters.
// readVesselState возвращает false, если байтов не хватает или status не 0 и не 1:
// от status зависит, какие поля идут дальше, так что остаток пакета не разобрать.
bool writeVesselState(Writer& writer, const VesselState& state);
bool readVesselState(Reader& reader, VesselState& state);

// WORLD_STATE, хост → клиент: состояние кораблей на момент simTime.
struct WorldState {
    double simTime = 0.0;  // oapiGetSimTime хоста
    double mjd = 0.0;      // oapiGetSimMJD
    double warp = 1.0;     // ускорение времени
    uint8_t paused = 0;    // 1 - пауза
    std::vector<VesselState> vessels;  // не больше kMaxVesselsPerPacket
};

// Пишет и читает только тело: UDP-заголовок - writeUdpHeader и readUdpHeader.
// writeWorldState возвращает false, если кораблей больше kMaxVesselsPerPacket или у
// какого-то корабля слишком много двигателей. Помещается ли пакет в kMaxUdpPacketSize,
// проверяет отправитель по размеру writer.bytes(): не поместившиеся корабли уходят
// следующим пакетом с тем же simTime.
bool writeWorldState(Writer& writer, const WorldState& message);
bool readWorldState(Reader& reader, WorldState& message);

// Топливо одного корабля - элемент FUEL.
struct VesselFuel {
    uint16_t netId = 0;
    double emptyMass = 0.0;           // пустая масса, кг (GetEmptyMass)
    std::vector<float> tankLevels;    // заполнение баков 0…1, по порядку индексов баков
};

// Число баков пишется одним байтом.
constexpr size_t kMaxTanks = 255;

// FUEL, хост → клиент, раз в секунду: топливо и пустая масса кораблей.
// Корабли, не поместившиеся в один пакет, уходят следующим пакетом FUEL.
struct Fuel {
    std::vector<VesselFuel> vessels;  // не больше kMaxVesselsPerPacket
};

// Пишет и читает только тело. writeFuel возвращает false, если кораблей больше
// kMaxVesselsPerPacket или у какого-то корабля больше kMaxTanks баков.
bool writeFuel(Writer& writer, const Fuel& message);
bool readFuel(Reader& reader, Fuel& message);

// VESSEL_ADDED, хост → клиент: у хоста появился новый корабль.
// Тело: имя, класс и состояние корабля как в WORLD_STATE (оно начинается с net_id).
struct VesselAdded {
    std::string name;
    std::string className;  // класс корабля, например DeltaGlider
    VesselState state;      // номер корабля - state.netId
};

// writeVesselAdded возвращает false, если имя или класс длиннее 255 байтов или у
// корабля больше kMaxThrusters двигателей.
bool writeVesselAdded(Writer& writer, const VesselAdded& message);
bool readVesselAdded(Reader& reader, VesselAdded& message);

// Значение throttle в INPUT, когда ручки газа нет или она не двигалась.
constexpr float kNoThrottle = -1.0f;

// INPUT, клиент → хост: полное текущее состояние органов управления (раздел 6).
// Тройки чисел - по осям: rot - тангаж, рыскание, крен; lin - вверх-вниз,
// влево-вправо, вперёд-назад. Значения от -1 до 1.
struct Input {
    uint16_t netId = 0;              // корабль, которым управляет клиент
    uint32_t keys = 0;               // удерживаемые клавиши, биты - PROTOCOL.md, раздел 7
    float rot[3] = {0.0f, 0.0f, 0.0f};
    float lin[3] = {0.0f, 0.0f, 0.0f};
    float throttle = kNoThrottle;    // ручка газа 0…1 или kNoThrottle
};

// Тела UDP-сообщений: заголовок - writeUdpHeader и readUdpHeader.
void writeInput(Writer& writer, const Input& message);
bool readInput(Reader& reader, Input& message);

// PING и PONG: системное время отправителя (oapiGetSysTime). PONG возвращает время из
// PING без изменений, поэтому у обоих сообщений одно и то же тело.
void writePingTime(Writer& writer, double time);
bool readPingTime(Reader& reader, double& time);

// Наибольший размер файла в SCENARIO: всё сообщение (тип, имя самой большой длины,
// поле size и файл) должно поместиться в TCP-кадр kMaxTcpMessageSize.
constexpr size_t kMaxScenarioSize = kMaxTcpMessageSize - 1 - (1 + kMaxStringLength) - 4;

// SCENARIO, хост → клиент: файл сценария с текущим миром хоста (этап 2).
struct Scenario {
    std::string name;           // имя файла сценария
    std::vector<uint8_t> data;  // содержимое файла как есть
};

// writeScenario возвращает false, если имя длиннее 255 байтов или файл больше
// kMaxScenarioSize. readScenario, кроме обычных проверок, отбрасывает сообщение, у
// которого поле size не совпадает с числом оставшихся байтов.
bool writeScenario(Writer& writer, const Scenario& message);
bool readScenario(Reader& reader, Scenario& message);

}  // namespace olc
