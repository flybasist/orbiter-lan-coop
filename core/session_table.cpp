#include "session_table.h"

#include <algorithm>

namespace olc {

namespace {

constexpr uint8_t kMinClientId = 1;
constexpr uint8_t kMaxClientId = 254;  // 255 - kNoOwner
constexpr uint32_t kNetIdCount = 65536;

}  // namespace

SessionTable::SessionTable(int maxPlayers) : maxPlayers_(maxPlayers) {
    if (maxPlayers_ < 2) {
        maxPlayers_ = 2;
    }
    if (maxPlayers_ > kMaxPlayersLimit) {
        maxPlayers_ = kMaxPlayersLimit;
    }
}

bool SessionTable::addPlayer(uint8_t& clientId) {
    if (static_cast<int>(clients_.size()) >= maxPlayers_ - 1) {
        return false;
    }

    // Наименьший номер, которого ещё нет среди клиентов.
    for (int id = kMinClientId; id <= kMaxClientId; id++) {
        if (std::find(clients_.begin(), clients_.end(), id) == clients_.end()) {
            clients_.push_back(static_cast<uint8_t>(id));
            clientId = static_cast<uint8_t>(id);
            return true;
        }
    }
    return false;  // недостижимо: клиентов меньше 8, а номеров 254
}

bool SessionTable::removePlayer(uint8_t clientId) {
    auto it = std::find(clients_.begin(), clients_.end(), clientId);
    if (it == clients_.end()) {
        return false;
    }
    clients_.erase(it);

    for (VesselRecord& vessel : vessels_) {
        if (vessel.owner == clientId) {
            vessel.owner = kNoOwner;
        }
    }
    return true;
}

int SessionTable::playerCount() const {
    return 1 + static_cast<int>(clients_.size());
}

int SessionTable::maxPlayers() const {
    return maxPlayers_;
}

bool SessionTable::insertVessel(uint16_t netId, const std::string& name,
                                const std::string& className) {
    if (name.empty() || name.size() > kMaxStringLength) {
        return false;
    }
    if (className.size() > kMaxStringLength) {
        return false;
    }
    if (findByName(name) != nullptr) {
        return false;
    }

    // Первое место, где номер уже не меньше нового: туда и вставляем.
    auto it = vessels_.begin();
    while (it != vessels_.end() && it->netId < netId) {
        ++it;
    }
    if (it != vessels_.end() && it->netId == netId) {
        return false;
    }

    VesselRecord record;
    record.netId = netId;
    record.name = name;
    record.className = className;
    vessels_.insert(it, record);
    return true;
}

bool SessionTable::addVessel(const std::string& name, const std::string& className,
                             uint16_t& netId) {
    if (nextNetId_ >= kNetIdCount) {
        return false;
    }

    uint16_t id = static_cast<uint16_t>(nextNetId_);
    if (!insertVessel(id, name, className)) {
        return false;
    }

    nextNetId_++;
    netId = id;
    return true;
}

bool SessionTable::addVesselWithId(uint16_t netId, const std::string& name,
                                   const std::string& className) {
    return insertVessel(netId, name, className);
}

bool SessionTable::removeVessel(uint16_t netId) {
    for (auto it = vessels_.begin(); it != vessels_.end(); ++it) {
        if (it->netId == netId) {
            vessels_.erase(it);
            return true;
        }
    }
    return false;
}

const VesselRecord* SessionTable::findById(uint16_t netId) const {
    for (const VesselRecord& vessel : vessels_) {
        if (vessel.netId == netId) {
            return &vessel;
        }
    }
    return nullptr;
}

const VesselRecord* SessionTable::findByName(const std::string& name) const {
    for (const VesselRecord& vessel : vessels_) {
        if (vessel.name == name) {
            return &vessel;
        }
    }
    return nullptr;
}

bool SessionTable::setOwner(uint16_t netId, uint8_t owner) {
    for (VesselRecord& vessel : vessels_) {
        if (vessel.netId == netId) {
            vessel.owner = owner;
            return true;
        }
    }
    return false;
}

bool SessionTable::isControlledBy(uint16_t netId, uint8_t clientId) const {
    if (clientId < kMinClientId || clientId > kMaxClientId) {
        return false;
    }
    const VesselRecord* vessel = findById(netId);
    return vessel != nullptr && vessel->owner == clientId;
}

std::vector<VesselEntry> SessionTable::vesselEntries() const {
    std::vector<VesselEntry> entries;
    for (const VesselRecord& vessel : vessels_) {
        VesselEntry entry;
        entry.netId = vessel.netId;
        entry.name = vessel.name;
        entry.className = vessel.className;
        entries.push_back(entry);
    }
    return entries;
}

std::vector<OwnerEntry> SessionTable::ownerEntries() const {
    std::vector<OwnerEntry> entries;
    for (const VesselRecord& vessel : vessels_) {
        OwnerEntry entry;
        entry.netId = vessel.netId;
        entry.owner = vessel.owner;
        entries.push_back(entry);
    }
    return entries;
}

}  // namespace olc
