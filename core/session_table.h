// Таблица кораблей и владения одной сессии.
//
// Хост ведёт таблицу сам: принимает клиентов (addPlayer), заводит корабли и выдаёт им
// net_id (addVessel), назначает владельцев (setOwner) и по таблице проверяет, чей ввод
// можно применять (isControlledBy). Клиент не выдаёт номера: он повторяет таблицу
// хоста по сообщениям VESSELS и VESSEL_ADDED (addVesselWithId), VESSEL_REMOVED
// (removeVessel) и OWNERSHIP (setOwner).
// Корабли хранятся в простом std::vector с линейным поиском: кораблей в сессии десятки,
// и такой код проще, чем карта или индекс по имени.
// Подробнее: docs/PROTOCOL.md, разделы 3 и 5; docs/ARCHITECTURE.md, раздел 1.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "messages.h"

namespace olc {

// Предел игроков (docs/DECISIONS.md, решение 14). Игроки - хост и все клиенты.
constexpr int kDefaultMaxPlayers = 4;
constexpr int kMaxPlayersLimit = 8;
constexpr uint8_t kHostOwner = 0;  // владелец "хост" в OWNERSHIP

// Один корабль в таблице.
struct VesselRecord {
    uint16_t netId = 0;
    std::string name;
    std::string className;
    uint8_t owner = kNoOwner;  // 0 - хост, 1…254 - клиент, kNoOwner - никто
};

class SessionTable {
public:
    // maxPlayers приводится к пределам 2…kMaxPlayersLimit (меньше 2 - некому подключаться).
    explicit SessionTable(int maxPlayers = kDefaultMaxPlayers);

    // ----- Игроки (только на хосте) -----

    // Принимает клиента: выдаёт наименьший свободный номер 1…254. Возвращает false, если
    // клиентов уже maxPlayers - 1 (хост - тоже игрок): хост ответит REJECT с причиной
    // NoFreeSlots.
    bool addPlayer(uint8_t& clientId);

    // Клиент ушёл: его номер освобождается, его корабли становятся ничьими (kNoOwner).
    // Возвращает false, если такого клиента нет.
    bool removePlayer(uint8_t clientId);

    int playerCount() const;  // хост + клиенты
    int maxPlayers() const;

    // ----- Корабли -----

    // Хост: новый корабль, выдаёт следующий net_id. Номера выдаются по порядку с 0 и за
    // сессию не переиспользуются (опоздавший пакет не должен попасть на другой корабль).
    // Возвращает false, если имя пустое, длиннее kMaxStringLength или уже есть в таблице,
    // класс длиннее kMaxStringLength или номера кончились (выдано 65536).
    // На одной таблице не смешивайте addVessel и addVesselWithId.
    bool addVessel(const std::string& name, const std::string& className, uint16_t& netId);

    // Клиент: корабль с номером, который выдал хост (VESSELS, VESSEL_ADDED). Возвращает
    // false, если такой номер или имя уже есть, имя пустое или строки длиннее
    // kMaxStringLength.
    bool addVesselWithId(uint16_t netId, const std::string& name, const std::string& className);

    bool removeVessel(uint16_t netId);  // false, если нет такого

    const VesselRecord* findById(uint16_t netId) const;             // nullptr, если нет
    const VesselRecord* findByName(const std::string& name) const;  // nullptr, если нет

    // ----- Владение -----

    // Не проверяет, подключён ли такой клиент: на клиенте список игроков неизвестен, а
    // хост передаёт только номера из addPlayer. Возвращает false, если корабля нет.
    bool setOwner(uint16_t netId, uint8_t owner);

    // Можно ли применять ввод клиента clientId к кораблю netId: корабль есть и его
    // владелец - этот клиент. Номера kHostOwner и kNoOwner клиентами не бывают, для них
    // всегда false.
    bool isControlledBy(uint16_t netId, uint8_t clientId) const;

    // ----- Для сообщений -----

    std::vector<VesselEntry> vesselEntries() const;  // для VESSELS, по возрастанию net_id
    std::vector<OwnerEntry> ownerEntries() const;    // для OWNERSHIP, по возрастанию net_id

private:
    // Проверяет имя и класс и вставляет корабль с сохранением порядка по netId.
    bool insertVessel(uint16_t netId, const std::string& name, const std::string& className);

    int maxPlayers_;
    std::vector<uint8_t> clients_;       // номера подключённых клиентов
    std::vector<VesselRecord> vessels_;  // по возрастанию netId
    uint32_t nextNetId_ = 0;             // следующий номер для addVessel; 65536 - номера кончились
};

}  // namespace olc
