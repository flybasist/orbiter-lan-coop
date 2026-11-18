// Client network side: connecting to the host and receiving the world state.
//
// What it does (docs/PROTOCOL.md, section 3):
// - connects over TCP and sends HELLO;
// - with scenarioId == kNoScenario only receives SCENARIO (the host's world as a file) and
//   closes: the plugin then starts the scenario and connects again with its number;
// - receives WELCOME (or REJECT), BODIES, VESSELS, OWNERSHIP and fills the session table;
// - after VESSELS answers CLASSES_MISSING (which classes exist is decided by the plugin via
//   classExists), opens a UDP socket and sends UDP_READY;
// - receives WORLD_STATE: checks the session number and packet order, puts the
//   states into the vessels' interpolation buffers (InterpolationBuffer) and adjusts the
//   display clock (ClientClock);
// - receives FUEL, LANDED and DOCK and hands them to the plugin (takeFuel, takeLanded, takeDocks);
// - receives VESSEL_ADDED and VESSEL_REMOVED, changes the session table and tells the plugin
//   which vessels to create and delete (takeVesselChanges); a class from VESSEL_ADDED that
//   the client lacks goes to the host in one more CLASSES_MISSING;
// - asks the host to take or release a vessel (requestTake, requestRelease); the answer is a new
//   OWNERSHIP (the table changes) or OWNER_DENIED (takeOwnerDenials);
// - sends KEY (sendKey) for single key presses;
// - sends INPUT (sendInput) to the host while the client controls a vessel, at most 60 per second;
// - sends PING once per second and measures the round trip time from PONG.
//
// Orbiter is not needed here: the plugin asks for a vessel state at the needed moment
// (sampleVessel) and imposes it on the vessel itself. So ClientLink is tested
// on loopback together with HostLink.
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

#include "client_clock.h"
#include "header.h"
#include "interpolation.h"
#include "messages.h"
#include "session_table.h"
#include "socket.h"

namespace olc {

// How many seconds the client waits for the connection and WELCOME.
constexpr double kConnectTimeout = 5.0;

// How many seconds the client waits for SCENARIO: the host saves the world first, and the file
// may be up to 1 MB.
constexpr double kScenarioTimeout = 30.0;

// How often the client sends PING, seconds.
constexpr double kPingInterval = 1.0;

// The most INPUT packets per second. PROTOCOL.md: "every frame, at most 60 Hz".
constexpr double kMaxInputRate = 60.0;

// Client settings.
struct ClientConfig {
    Endpoint host;                // host address and port
    std::string player;           // player name for HELLO
    uint32_t orbiterVersion = 0;  // oapiGetOrbiterVersion, goes into HELLO
    std::string pluginVersion;    // plugin version, goes into HELLO
    // Goes into HELLO: kOwnScenario - the player started the scenario by hand (stage 1),
    // kNoScenario - ask the host for its scenario, otherwise the number from SCENARIO.
    uint32_t scenarioId = kOwnScenario;
    std::string system;           // own planetary system; a different one on the host - disconnect
    double displayDelay = kDefaultDisplayDelay;
    // Whether the client has a vessel class. Classes for which it returns false go to the host in
    // CLASSES_MISSING. If not set, all classes are assumed to exist.
    std::function<bool(const std::string& className)> classExists;
};

// A vessel that appeared on the host or was deleted there during the session.
struct VesselChange {
    bool added = false;   // true - create the vessel, false - delete it
    VesselEntry vessel;   // number, name and class
};

// Statistics for the log.
struct ClientStats {
    uint64_t worldStatePackets = 0;  // received WORLD_STATE
    uint64_t droppedPackets = 0;     // foreign session, old seq, broken
    uint64_t unknownVessels = 0;     // vessel entries in WORLD_STATE and FUEL with an unknown net_id
    uint64_t starvedSamples = 0;     // sampleVessel when the buffer ran out
    uint64_t inputPackets = 0;       // sent INPUT
};

class ClientLink {
public:
    enum class State {
        Idle,        // start has not been called yet
        Connecting,  // TCP connection and handshake in progress
        Ready,       // UDP_READY sent, states are being exchanged
        Closed,      // disconnected; the reason is closeReason
    };

    ClientLink() = default;
    ~ClientLink();
    ClientLink(const ClientLink&) = delete;
    ClientLink& operator=(const ClientLink&) = delete;

    // Starts connecting. false - immediate error (the reason is in closeReason and takeLog).
    // The network must already be started (startNetwork). now - real time, seconds.
    // Called once: a new connection needs a new ClientLink (a second call returns false).
    bool start(const ClientConfig& config, double now);

    // Sends BYE with a reason to the host and closes the sockets.
    void stop(const std::string& reason);

    // Receives and sends everything needed. now - real time (oapiGetSysTime).
    void poll(double now);

    State state() const { return state_; }
    const std::string& closeReason() const { return closeReason_; }

    // ----- What the host sent -----

    const SessionTable& table() const { return table_; }
    const std::vector<BodyEntry>& bodies() const { return bodies_; }
    uint8_t clientId() const { return clientId_; }
    uint8_t stateRateHz() const { return stateRateHz_; }

    // Whether at least one WORLD_STATE has arrived. Before that time and states are unknown.
    bool hasWorld() const { return clock_.started(); }

    // The host sim time to display now (ClientClock).
    double displayTime(double now) { return clock_.displayTime(now); }

    // Host time acceleration and pause from the last WORLD_STATE.
    double hostWarp() const { return hostWarp_; }
    bool hostPaused() const { return hostPaused_; }

    // MJD corresponding to the host sim time simTime (from the last WORLD_STATE:
    // mjd = packet_mjd + (simTime − packet_sim_time) / 86400).
    double hostMjdAt(double simTime) const;

    // State of vessel netId at host sim time simTime (InterpolationBuffer::sample).
    // false - no packet has arrived for this vessel yet.
    bool sampleVessel(uint16_t netId, double simTime, VesselState& result, bool& starved);

    // The newest received state of vessel netId, without the display delay
    // (InterpolationBuffer::latest). false - no packet has arrived for this vessel yet.
    bool latestVessel(uint16_t netId, VesselState& result) const;

    // Takes fuel from FUEL messages received since the last call (the latest per vessel).
    std::vector<VesselFuel> takeFuel();

    // Whether the host's scenario has arrived (only with scenarioId == kNoScenario). After it
    // the link is Closed with the reason "scenario received".
    bool hasScenario() const { return hasScenario_; }
    const Scenario& scenario() const { return scenario_; }

    // Takes LANDED messages received since the last call, in arrival order.
    std::vector<Landed> takeLanded();

    // Takes the DOCK messages that came since the last call, oldest first.
    std::vector<Dock> takeDocks();

    // Takes vessel additions and deletions received since the last call, in arrival order.
    // The session table is already changed; the plugin creates and deletes the vessels itself.
    std::vector<VesselChange> takeVesselChanges();

    // Asks the host to give a free vessel to this client. The result comes later: the table
    // changes by OWNERSHIP or the refusal arrives (takeOwnerDenials). Does nothing before the handshake is done.
    void requestTake(uint16_t netId);

    // Asks the host to release this client's vessel.
    void requestRelease(uint16_t netId);

    // Takes refusals (OWNER_DENIED) received since the last call, in arrival order.
    std::vector<OwnerDenied> takeOwnerDenials();

    // Sends the current state of the controls to the host in one UDP INPUT packet. Does nothing
    // before the link is Ready, and if the previous INPUT went out less than 1 / kMaxInputRate
    // seconds ago. now - real time, seconds. Returns true if the packet went out.
    bool sendInput(const Input& input, double now);

    // Sends KEY: one key press for the client's own vessel netId (TCP, so it is not lost). mods -
    // bits of KeyPress::mods. Does nothing before the handshake is over.
    void sendKey(uint16_t netId, uint8_t key, uint8_t mods);

    // The latest round trip time, seconds; below 0 - not measured yet.
    double rtt() const { return rtt_; }

    const ClientStats& stats() const { return stats_; }

    // Takes the accumulated log lines.
    std::vector<std::string> takeLog();

private:
    // Parses the host's TCP messages. false - the connection must be closed (the reason is in closeReason_).
    bool handleFrames(double now);
    // Parsing of individual messages. On error or rejection they call close.
    void handleWelcome(Reader& reader);
    void handleReject(Reader& reader);
    void handleScenario(Reader& reader);
    void handleVessels(Reader& reader, double now);
    void handleVesselAdded(Reader& reader);
    void handleVesselRemoved(Reader& reader);
    // true if the client lacks this class and has not reported it yet (then remembers it).
    bool isNewMissingClass(const std::string& className);
    void sendOwnerRequest(uint16_t netId, uint8_t action);
    // Puts a TCP message into the queue and sends as much as it can.
    void sendMessage(TcpType type, const Writer& body);
    void handleUdp(double now);
    void sendPing(double now);
    void close(const std::string& reason);
    void log(const std::string& line);

    ClientConfig config_;
    State state_ = State::Idle;
    std::string closeReason_;
    double startedAt_ = 0.0;
    double lastHeard_ = 0.0;  // real time of the last TCP message or UDP packet from the host

    TcpConnection tcp_;
    TcpFrameReader frames_;
    UdpSocket udp_;
    bool welcomed_ = false;
    uint32_t session_ = 0;
    uint8_t clientId_ = 0;
    uint8_t stateRateHz_ = 0;

    SessionTable table_;
    std::vector<BodyEntry> bodies_;

    ClientClock clock_;
    std::map<uint16_t, InterpolationBuffer> buffers_;  // by net_id
    bool haveSeq_ = false;
    uint32_t lastSeq_ = 0;  // last accepted seq of WORLD_STATE and FUEL
    double hostWarp_ = 1.0;
    bool hostPaused_ = false;
    double lastPacketSimTime_ = 0.0;
    double lastPacketMjd_ = 0.0;

    std::map<uint16_t, VesselFuel> fuel_;  // by net_id, not yet taken
    std::vector<Landed> landed_;
    std::vector<Dock> docks_;
    std::vector<VesselChange> vesselChanges_;
    std::vector<OwnerDenied> ownerDenials_;
    std::vector<std::string> missingClasses_;  // already reported in CLASSES_MISSING
    bool hasScenario_ = false;
    Scenario scenario_;

    uint32_t pingSeq_ = 0;
    double nextPing_ = 0.0;
    uint32_t inputSeq_ = 0;
    double nextInput_ = 0.0;  // the earliest time of the next INPUT
    double rtt_ = -1.0;

    ClientStats stats_;
    std::vector<std::string> log_;
};

}  // namespace olc
