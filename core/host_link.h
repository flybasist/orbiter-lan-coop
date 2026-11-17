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
// - sends TCP messages (LANDED, VESSEL_ADDED, VESSEL_REMOVED) to all clients that finished
//   the handshake;
// - removes a client that left (BYE, broken connection, no HELLO within kHelloTimeout) from the session table.
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

// Host settings.
struct HostConfig {
    uint16_t port = 0;            // TCP and UDP; 0 - any free port (for tests)
    uint32_t session = 0;         // session number, written into every UDP packet
    uint32_t orbiterVersion = 0;  // oapiGetOrbiterVersion; a different client version gets REJECT
    std::string system;           // planetary system name, goes into WELCOME
    uint8_t stateRateHz = 30;     // WORLD_STATE rate, goes into WELCOME
    // Saves the current world and fills scenario.name and scenario.data (the number is set by
    // HostLink). false - the world could not be saved. If not set, clients that ask for a
    // scenario get REJECT.
    std::function<bool(Scenario& scenario)> makeScenario;
};

// Statistics for the log.
struct HostStats {
    uint64_t udpPacketsSent = 0;
    uint64_t udpBadPackets = 0;   // foreign and broken incoming UDP packets
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

    // A vessel appeared or was deleted during the session. The plugin first changes the session
    // table (addVessel, removeVessel), then calls these: clients that join later get the vessel
    // in VESSELS anyway. sendVesselAdded returns false if the message cannot be built (see
    // writeVesselAdded).
    bool sendVesselAdded(const VesselAdded& added);
    void sendVesselRemoved(uint16_t netId);

    // How many clients finished the handshake (got WELCOME).
    int clientCount() const;

    // How many of them sent UDP_READY and receive WORLD_STATE.
    int readyClientCount() const;

    const HostStats& stats() const { return stats_; }

    // Takes the accumulated log lines.
    std::vector<std::string> takeLog();

private:
    // One connected client.
    struct Client {
        TcpConnection tcp;
        TcpFrameReader frames;
        double connectedAt = 0.0;  // real time of connection
        bool welcomed = false;     // got WELCOME
        uint8_t clientId = 0;      // number from the table once welcomed
        std::string player;
        bool udpReady = false;     // sent UDP_READY
        Endpoint udp;              // where to send UDP
        bool udpFromPing = false;  // address taken from PING, not from UDP_READY (see handleUdp)
        bool scenarioSent = false; // SCENARIO is in the queue: disconnect once it has gone out
        std::string leaveReason;   // why we disconnect; set when handleFrames returned false
    };

    // Parses the client's TCP messages. false - the client must be disconnected.
    bool handleFrames(Client& client);
    bool handleHello(Client& client, Reader& reader);
    // Answers a HELLO with kNoScenario: puts SCENARIO into the queue or sends REJECT.
    bool sendScenario(Client& client, const Hello& hello);
    // Sends BODIES, VESSELS and OWNERSHIP to the client. false - a message could not be built.
    bool sendWorldLists(Client& client);
    // Puts a TCP message into the client's queue.
    bool sendMessage(Client& client, TcpType type, const Writer& body);
    // Sends REJECT and remembers the disconnect reason. Always returns false:
    // handleHello returns it as "disconnect the client".
    bool reject(Client& client, RejectReason reason, const std::string& text);
    // Sends a UDP packet to all clients with udpReady.
    void sendUdpToReady(uint8_t type, const std::vector<uint8_t>& body);
    void handleUdp();
    // Remembers the address the PING of client clientId came from: UDP for it goes there.
    void learnUdpAddress(uint8_t clientId, const Endpoint& from);
    void dropClient(size_t index, const std::string& reason);
    void log(const std::string& line);

    SessionTable& table_;
    HostConfig config_;
    bool running_ = false;
    TcpListener listener_;
    UdpSocket udp_;
    std::vector<Client> clients_;
    std::vector<BodyEntry> bodies_;
    uint32_t seq_ = 0;  // sequence number of outgoing UDP packets
    uint32_t lastScenarioId_ = 0;  // number of the last SCENARIO sent
    HostStats stats_;
    std::vector<std::string> log_;
};

}  // namespace olc
