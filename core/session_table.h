// Vessel and ownership table of one session.
//
// The host keeps the table itself: accepts clients (addPlayer), registers vessels and gives them
// net_id (addVessel), assigns owners (setOwner) and uses the table to check whose input
// may be applied (isControlledBy). The client does not hand out numbers: it mirrors the host's
// table from the messages VESSELS and VESSEL_ADDED (addVesselWithId), VESSEL_REMOVED
// (removeVessel) and OWNERSHIP (setOwner).
// Vessels are kept in a plain std::vector with linear search: a session has tens of vessels,
// and this code is simpler than a map or a name index.
// More: docs/PROTOCOL.md, sections 3 and 5; docs/ARCHITECTURE.md, section 1.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "messages.h"

namespace olc {

// Player limit (docs/DECISIONS.md, decision 14). Players are the host and all clients.
constexpr int kDefaultMaxPlayers = 4;
constexpr int kMaxPlayersLimit = 8;
constexpr uint8_t kHostOwner = 0;  // the "host" owner in OWNERSHIP

// How long a vessel stays reserved for a pilot who dropped out, seconds (decision 35).
constexpr double kOwnerHoldSeconds = 60.0;

// One vessel in the table.
struct VesselRecord {
    uint16_t netId = 0;
    std::string name;
    std::string className;
    uint8_t owner = kNoOwner;  // 0 - host, 1…254 - client, kNoOwner - nobody
    // Host only: the pilot dropped out and the vessel is reserved for this pilot until holdUntil.
    // The owner is kNoOwner meanwhile; only a pilot with the same name can take the vessel back.
    std::string holdPlayer;
    double holdUntil = 0.0;  // real time, seconds
};

class SessionTable {
public:
    // maxPlayers is clamped to 2…kMaxPlayersLimit (below 2 nobody could connect).
    explicit SessionTable(int maxPlayers = kDefaultMaxPlayers);

    // ----- Players (host only) -----

    // Accepts a client: gives the lowest free number 1…254. Returns false if there are
    // already maxPlayers - 1 clients (the host is a player too): the host answers REJECT with reason
    // NoFreeSlots.
    bool addPlayer(uint8_t& clientId);

    // A client left: its number is freed and its vessels become ownerless (kNoOwner), but
    // for kOwnerHoldSeconds after now they are reserved for the pilot named player (an empty
    // name reserves nothing). now - real time, seconds.
    // Returns false if there is no such client.
    bool removePlayer(uint8_t clientId, const std::string& player, double now);

    int playerCount() const;  // host + clients
    int maxPlayers() const;

    // ----- Vessels -----

    // Host: a new vessel, gets the next net_id. Numbers are given in order from 0 and are
    // not reused during the session (a late packet must not land on another vessel).
    // Returns false if the name is empty, longer than kMaxStringLength or already in the table,
    // the class is longer than kMaxStringLength, or numbers ran out (65536 given).
    // Do not mix addVessel and addVesselWithId on one table.
    bool addVessel(const std::string& name, const std::string& className, uint16_t& netId);

    // Client: a vessel with the number the host gave (VESSELS, VESSEL_ADDED). Returns
    // false if such a number or name already exists, the name is empty, or strings are longer than
    // kMaxStringLength.
    bool addVesselWithId(uint16_t netId, const std::string& name, const std::string& className);

    bool removeVessel(uint16_t netId);  // false if there is no such vessel

    const VesselRecord* findById(uint16_t netId) const;             // nullptr if none
    const VesselRecord* findByName(const std::string& name) const;  // nullptr if none

    // ----- Ownership -----

    // Does not check whether such a client is connected: the client does not know the player list,
    // and the host passes only numbers from addPlayer. Returns false if there is no vessel.
    bool setOwner(uint16_t netId, uint8_t owner);

    // The client takes a free vessel. Ok if the vessel has no owner, or is already this client's,
    // or is reserved for this pilot (player is the name from HELLO). A client may have only one
    // vessel. Host only.
    OwnerResult takeVessel(uint16_t netId, uint8_t clientId, const std::string& player, double now);

    // The client releases its own vessel: the owner becomes kNoOwner without a reservation.
    // NotOwner if the vessel is not this client's. Host only.
    OwnerResult releaseVessel(uint16_t netId, uint8_t clientId);

    // A pilot who dropped out came back: gives this pilot back the vessels reserved for this name.
    // Returns how many. Host only.
    int restoreHeldVessels(uint8_t clientId, const std::string& player, double now);

    // Whether input from client clientId may be applied to vessel netId: the vessel exists and its
    // owner is this client. kHostOwner and kNoOwner are never client numbers, so for them
    // it is always false.
    bool isControlledBy(uint16_t netId, uint8_t clientId) const;

    // ----- For messages -----

    std::vector<VesselEntry> vesselEntries() const;  // for VESSELS, ascending net_id
    std::vector<OwnerEntry> ownerEntries() const;    // for OWNERSHIP, ascending net_id

private:
    // Checks the name and class and inserts the vessel keeping netId order.
    bool insertVessel(uint16_t netId, const std::string& name, const std::string& className);

    int maxPlayers_;
    std::vector<uint8_t> clients_;       // numbers of connected clients
    std::vector<VesselRecord> vessels_;  // ascending netId
    uint32_t nextNetId_ = 0;             // next number for addVessel; 65536 - numbers ran out
};

}  // namespace olc
