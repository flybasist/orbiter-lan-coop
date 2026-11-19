// Host network side: client connections and world state broadcast.
//
// What it does (docs/PROTOCOL.md, section 3):
// - listens for TCP and UDP on one port;
// - runs the handshake: HELLO → WELCOME (or REJECT) → BODIES → VESSELS → OWNERSHIP;
//   then waits for CLASSES_MISSING and UDP_READY from the client;
// - answers a HELLO without a scenario number (kNoScenario) with SCENARIO - the host's current
//   world from HostConfig::makeScenario - and closes the connection once the file has gone out;
// - answers PING with PONG and learns the client's real UDP address from PING (client behind NAT);
// - sends WORLD_STATE and FUEL to clients whose UDP port is known; a state that does
//   not fit into one packet (kMaxUdpPacketSize) is split into several packets
//   with the same sim_time;
// - sends DOCK (docked and undocked) over TCP and keeps the list of current links for
//   clients that join later;
// - sends TCP messages (LANDED, VESSEL_ADDED, VESSEL_REMOVED) to all clients that finished
//   the handshake;
// - receives INPUT only from the UDP address of the client that owns the vessel, keeps the latest one
//   per vessel (latestInput); the plugin applies it to the vessel;
// - receives KEY (single key presses) only for the vessel the client owns and queues them for the
//   plugin (takeKeys), which passes them to the vessel (SendBufferedKey);
// - handles OWNER_REQUEST: a client takes a free vessel or releases its own one (the rules are in
//   SessionTable); every change goes to all clients in OWNERSHIP, a refusal - to the sender
//   in OWNER_DENIED. A pilot who dropped out keeps the vessel for kOwnerHoldSeconds and gets it
//   back when a player with the same name joins;
// - handles TIME_REQUEST (pause and warp) only from the flight director, whose name is
//   HostConfig::director; the plugin applies the requests (takeTimeRequests). Other senders are
//   logged and ignored. The director and clients that said so in HELLO are observers: they get
//   OWNER_DENIED, reason Observer, when they ask for a vessel;
// - sends ANIM_STATE (animations of every vessel, read by the plugin on the host) and NAV_STATE
//   (radio frequencies) over TCP; keeps the latest values of each vessel and sends them to a client
//   that joins later, after OWNERSHIP and DOCK. NAV_STATE from a client is accepted only for the
//   vessel the client owns: the plugin applies it (takeNavStates) and the host sends it to the others;
// - sends PLAYERS (who is in the session, whose vessel, how good the connection is) to everyone
//   after a join, a leave or an ownership change and every kPlayersInterval seconds;
// - removes a client that left (BYE, broken connection, no HELLO within kHelloTimeout, nothing heard
//   for kSilenceTimeout) from the session table.
//
// Orbiter is not needed here: the plugin describes vessels and bodies via SessionTable and
// setBodies, collects the world state itself and passes it to sendWorldState. So
// HostLink is tested on loopback together with ClientLink.
//
// No function waits: poll is called every frame (and by a timer while paused) from
// Orbiter's main thread (decision 23). Messages for Orbiter.log accumulate and are taken
// with takeLog - the lines are in English.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "header.h"
#include "messages.h"
#include "session_table.h"
#include "socket.h"

namespace olc {

// How many seconds the host waits for HELLO after a client connects.
constexpr double kHelloTimeout = 5.0;

// How many seconds the host gives a SCENARIO to go out before it drops the connection.
constexpr double kScenarioSendTimeout = 30.0;

// How often the host repeats PLAYERS, seconds: the round trip times change.
constexpr double kPlayersInterval = 2.0;

// How many seconds an INPUT stays valid. The plugin releases the vessel's controls if the latest
// INPUT is older (connection lost, so no key stays stuck).
constexpr double kInputTimeout = 0.5;

// Host settings.
struct HostConfig {
    uint16_t port = 0;            // TCP and UDP; 0 - any free port (for tests)
    uint32_t session = 0;         // session number, written into every UDP packet
    uint32_t orbiterVersion = 0;  // oapiGetOrbiterVersion; a different client version gets REJECT
    std::string system;           // planetary system name, goes into WELCOME
    uint8_t stateRateHz = 60;     // WORLD_STATE rate, goes into WELCOME (decision 36)
    // The host's own player name in PLAYERS. Empty - the host does not play (a host without
    // graphics) and is left out of PLAYERS.
    std::string hostPlayer = "Host";
    // Name of the flight director (decision 46); a client with this name in HELLO is the director.
    // Empty - there is no director, nobody may pause or warp from a client.
    std::string director;
    // Saves the current world and fills scenario.name and scenario.data (the number is set by
    // HostLink). false - the world could not be saved. If not set, clients that ask for a
    // scenario get REJECT.
    std::function<bool(Scenario& scenario)> makeScenario;
};

// Statistics for the log.
struct HostStats {
    uint64_t udpPacketsSent = 0;
    uint64_t udpBadPackets = 0;   // foreign and broken incoming UDP packets
    uint64_t inputRejected = 0;   // INPUT for a vessel the client does not own
    uint64_t inputAccepted = 0;   // INPUT stored as a client's latest
    uint64_t keysAccepted = 0;    // KEY presses queued for the plugin
    uint64_t keysRejected = 0;    // KEY for a vessel the client does not own
    uint64_t timeRequestsAccepted = 0;  // TIME_REQUEST from the director, queued for the plugin
    uint64_t timeRequestsRejected = 0;  // TIME_REQUEST from anyone else
    uint64_t saveRequestsAccepted = 0;  // SAVE_REQUEST from the director, queued for the plugin
    uint64_t saveRequestsRejected = 0;  // SAVE_REQUEST from anyone else
    uint64_t navStatesAccepted = 0;     // NAV_STATE from the owner of the vessel
    uint64_t navStatesRejected = 0;     // NAV_STATE for a vessel the client does not own
};

class HostLink {
public:
    // table - the plugin's session table: players, vessels and owners. HostLink adds and
    // removes players in it; the plugin registers vessels. The table must outlive HostLink.
    explicit HostLink(SessionTable& table);
    ~HostLink();
    HostLink(const HostLink&) = delete;
    HostLink& operator=(const HostLink&) = delete;

    // Opens the TCP listener and the UDP socket on config.port. false - the port is busy or an error
    // (the reason is in takeLog). The network must already be started (startNetwork).
    bool start(const HostConfig& config);

    // Sends BYE with a reason to all clients and closes the sockets. A repeated call does nothing.
    void stop(const std::string& reason);

    // The port after start (with port 0 the system chooses it); 0 if the host is not running.
    uint16_t port() const;

    // Celestial bodies for BODIES. Set before clients connect.
    void setBodies(const std::vector<BodyEntry>& bodies);

    // Accepts connections and messages, runs the handshake, answers PING, sends the rest of
    // the TCP queues. now - real time in seconds (oapiGetSysTime), needed for kHelloTimeout.
    void poll(double now);

    // Sends WORLD_STATE to clients with a known UDP port. There may be any number of
    // vessels: they are split into packets no larger than kMaxUdpPacketSize.
    void sendWorldState(const WorldState& state);

    // Sends FUEL the same way, split into packets.
    void sendFuel(const Fuel& fuel);

    // Sends LANDED over TCP to all clients that finished the handshake.
    void sendLanded(const Landed& landed);

    // Sends DOCK over TCP to all clients that finished the handshake. HostLink remembers the
    // links that are docked now and sends them to clients that join later, right after OWNERSHIP.
    void sendDock(const Dock& dock);

    // Sends ANIM_STATE (the changed animations of one vessel) over TCP to all clients that finished
    // the handshake. HostLink remembers the latest value of every animation and sends them to
    // clients that join later. false - the message could not be built (nothing is sent).
    bool sendAnimState(const AnimState& state);

    // Sends NAV_STATE of a vessel the host owns (the host is the source of its frequencies) to all
    // clients, and remembers it for clients that join later. false - the message could not be built.
    bool sendNavState(const NavState& state);

    // Takes the NAV_STATE messages accepted from clients since the last call, oldest first. The
    // plugin applies them to its vessels (SetNavChannel, SetTransponderChannel); the host has
    // already sent each one on to the other clients.
    std::vector<NavState> takeNavStates();

    // A vessel appeared or was deleted during the session. The plugin first changes the session
    // table (addVessel, removeVessel), then calls these: clients that join later get the vessel
    // in VESSELS anyway. sendVesselAdded returns false if the message cannot be built (see
    // writeVesselAdded).
    bool sendVesselAdded(const VesselAdded& added);
    void sendVesselRemoved(uint16_t netId);

    // Sends OWNERSHIP with the whole table to all clients that finished the handshake. HostLink
    // calls it itself after client requests; the plugin calls it after it changes the host's own
    // vessel in the table (setOwner with kHostOwner or kNoOwner).
    void broadcastOwnership();

    // The latest INPUT for vessel netId from the client that owns it right now. false - no
    // INPUT yet, or the vessel has another owner now. receivedAt - host real time of the packet;
    // the plugin compares it with kInputTimeout.
    bool latestInput(uint16_t netId, Input& input, double& receivedAt) const;

    // Takes the KEY presses that came since the last call, oldest first. Only presses for the
    // vessel the sender owns are here; the rest are counted in stats().keysRejected.
    std::vector<KeyPress> takeKeys();

    // Takes the TIME_REQUEST messages from the director that came since the last call, oldest
    // first. The plugin applies the last one (oapiSetPause, oapiSetTimeAcceleration).
    std::vector<TimeRequest> takeTimeRequests();

    // Takes the number of SAVE_REQUEST messages from the director that came since the last call.
    // The plugin saves the world once if it is not zero.
    int takeSaveRequests();

    // Sends PLAYERS to all clients that finished the handshake. HostLink calls it itself; the
    // plugin calls it after it changes the host's own vessel (together with broadcastOwnership,
    // which already includes it).
    void broadcastPlayers();

    // How many clients finished the handshake (got WELCOME).
    int clientCount() const;

    // How many of them sent UDP_READY and receive WORLD_STATE.
    int readyClientCount() const;

    const HostStats& stats() const { return stats_; }

    // Takes the accumulated log lines.
    std::vector<std::string> takeLog();

private:
    // The latest accepted INPUT of one vessel.
    struct LastInput {
        Input input;
        double receivedAt = 0.0;  // host real time
    };

    // One connected client.
    struct Client {
        TcpConnection tcp;
        TcpFrameReader frames;
        double connectedAt = 0.0;  // real time of connection
        double lastHeard = 0.0;    // real time of the last TCP message or UDP packet from the client
        bool welcomed = false;     // got WELCOME
        uint8_t clientId = 0;      // number from the table once welcomed
        std::string player;
        bool director = false;     // the flight director (also an observer)
        bool observer = false;     // never takes a vessel
        uint16_t rttMs = kRttUnknown;  // the latest round trip time the client reported in PING
        bool udpReady = false;     // sent UDP_READY
        Endpoint udp;              // where to send UDP
        bool udpFromPing = false;  // address taken from PING, not from UDP_READY (see handleUdp)
        bool haveInputSeq = false; // got at least one INPUT
        uint32_t lastInputSeq = 0; // seq of the last accepted INPUT
        std::map<uint16_t, LastInput> inputs;  // latest INPUT by net_id
        bool scenarioSent = false; // SCENARIO is in the queue: disconnect once it has gone out
        std::string leaveReason;   // why we disconnect; set when handleFrames returned false
    };

    // Parses the client's TCP messages. false - the client must be disconnected.
    bool handleFrames(Client& client);
    bool handleHello(Client& client, Reader& reader);
    // Answers a HELLO with kNoScenario: puts SCENARIO into the queue or sends REJECT.
    bool sendScenario(Client& client, const Hello& hello);
    // Handles KEY of a welcomed client. false - the message is broken.
    bool handleKey(Client& client, Reader& reader);
    // Handles TIME_REQUEST of a welcomed client. false - the message is broken.
    bool handleTimeRequest(Client& client, Reader& reader);
    // Handles SAVE_REQUEST of a welcomed client (the body is empty). false - the message is broken.
    bool handleSaveRequest(Client& client, Reader& reader);
    // Handles NAV_STATE of a welcomed client. false - the message is broken.
    bool handleNavState(Client& client, Reader& reader);
    // Handles OWNER_REQUEST of a welcomed client. false - the message is broken.
    bool handleOwnerRequest(Client& client, Reader& reader);
    // Sends BODIES, VESSELS, OWNERSHIP, DOCK, ANIM_STATE and NAV_STATE to the client. false - a
    // message could not be built.
    bool sendWorldLists(Client& client);
    // Puts a TCP message into the client's queue.
    bool sendMessage(Client& client, TcpType type, const Writer& body);
    // Sends REJECT and remembers the disconnect reason. Always returns false:
    // handleHello returns it as "disconnect the client".
    bool reject(Client& client, RejectReason reason, const std::string& text);
    // Sends a UDP packet to all clients with udpReady.
    void sendUdpToReady(uint8_t type, const std::vector<uint8_t>& body);
    void handleUdp();
    // Handles an INPUT packet (the header is already read) that came from address from.
    void handleInput(Reader& reader, uint32_t seq, const Endpoint& from);
    // Remembers the address the PING of client clientId came from (UDP for it goes there) and the
    // round trip time rttMs from that PING.
    void learnUdpAddress(uint8_t clientId, const Endpoint& from, uint16_t rttMs);
    void dropClient(size_t index, const std::string& reason);
    void log(const std::string& line);

    SessionTable& table_;
    HostConfig config_;
    bool running_ = false;
    TcpListener listener_;
    UdpSocket udp_;
    std::vector<Client> clients_;
    std::vector<BodyEntry> bodies_;
    std::vector<Dock> docks_;     // links that are docked now (see sendDock)
    std::vector<KeyPress> keys_;  // accepted KEY presses, not yet taken by the plugin
    std::map<uint16_t, std::map<uint16_t, float>> anims_;  // latest animations: net_id -> index -> value
    std::map<uint16_t, NavState> navStates_;  // latest NAV_STATE by net_id
    std::vector<NavState> navUpdates_;        // accepted from clients, not yet taken by the plugin
    int saveRequests_ = 0;  // accepted SAVE_REQUEST, not yet taken by the plugin
    std::vector<TimeRequest> timeRequests_;  // accepted TIME_REQUEST, not yet taken by the plugin
    double nextPlayers_ = 0.0;    // real time of the next periodic PLAYERS
    double now_ = 0.0;  // real time of the last poll
    uint32_t seq_ = 0;  // sequence number of outgoing UDP packets
    uint32_t lastScenarioId_ = 0;  // number of the last SCENARIO sent
    HostStats stats_;
    std::vector<std::string> log_;
};

}  // namespace olc
