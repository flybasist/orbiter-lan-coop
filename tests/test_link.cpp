// HostLink and ClientLink tests together: handshake, rejections, state broadcast, fuel,
// landing, vessels added and removed, ownership, INPUT, KEY, DOCK, SCENARIO, PING/PONG, NAT,
// silence and disconnecting. Loopback 127.0.0.1 and port 0 (any free one) only.
// All waiting is a poll + 1 ms pause loop with a time limit, so the test
// cannot hang.

#include <chrono>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <thread>
#include <vector>

#include "check.h"
#include "client_link.h"
#include "header.h"
#include "host_link.h"
#include "messages.h"
#include "protocol.h"
#include "session_table.h"
#include "socket.h"

namespace {

constexpr uint32_t kLoopback = 0x7F000001;  // 127.0.0.1
constexpr uint32_t kOrbiterVersion = 20260101;
constexpr uint32_t kSession = 777;

// Real time in seconds since the first call.
double now() {
    static const auto start = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    return elapsed.count();
}

// A host with a table. The table is declared first: it outlives HostLink.
struct TestHost {
    explicit TestHost(int maxPlayers = olc::kDefaultMaxPlayers)
        : table(maxPlayers), link(table) {}

    // Starts the host on port 0.
    bool start() {
        olc::HostConfig config;
        config.port = 0;
        config.session = kSession;
        config.orbiterVersion = kOrbiterVersion;
        config.system = "Sol";
        config.stateRateHz = 30;
        config.makeScenario = makeScenario;
        config.director = director;
        if (headless) {
            config.hostPlayer.clear();
        }
        return link.start(config);
    }

    std::string director;  // flight director name, set before start if needed
    bool headless = false;  // true - the host does not play and is not in PLAYERS
    std::function<bool(olc::Scenario&)> makeScenario;  // set before start if needed
    olc::SessionTable table;
    olc::HostLink link;
};

olc::ClientConfig makeClientConfig(const TestHost& host) {
    olc::ClientConfig config;
    config.host.ip = kLoopback;
    config.host.port = host.link.port();
    config.player = "Pilot";
    config.orbiterVersion = kOrbiterVersion;
    config.pluginVersion = "test";
    config.system = "Sol";
    return config;
}

// Polls the host and clients every 1 ms until condition is true or time runs out.
bool waitFor(olc::HostLink& host, std::initializer_list<olc::ClientLink*> clients,
             const std::function<bool()>& condition, double seconds = 3.0) {
    double deadline = now() + seconds;
    while (now() < deadline) {
        host.poll(now());
        for (olc::ClientLink* client : clients) {
            client->poll(now());
        }
        if (condition()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return condition();
}

// Three vessels and two bodies: a set for the handshake.
void fillHost(TestHost& host) {
    uint16_t netId = 0;
    host.table.addVessel("GL-01", "DeltaGlider", netId);
    host.table.addVessel("GL-02", "DeltaGlider", netId);
    host.table.addVessel("ISS", "ShuttleA", netId);
    host.table.setOwner(0, olc::kHostOwner);
    host.table.setOwner(1, olc::kHostOwner);
    std::vector<olc::BodyEntry> bodies = {{0, "Sun"}, {1, "Earth"}};
    host.link.setBodies(bodies);
}

bool hasLine(const std::vector<std::string>& lines, const std::string& part) {
    for (const std::string& line : lines) {
        if (line.find(part) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

// Whether two tables have the same vessel lists.
bool sameTables(const olc::SessionTable& a, const olc::SessionTable& b) {
    std::vector<olc::VesselEntry> va = a.vesselEntries();
    std::vector<olc::VesselEntry> vb = b.vesselEntries();
    std::vector<olc::OwnerEntry> oa = a.ownerEntries();
    std::vector<olc::OwnerEntry> ob = b.ownerEntries();
    if (va.size() != vb.size() || oa.size() != ob.size()) {
        return false;
    }
    for (size_t i = 0; i < va.size(); i++) {
        if (va[i].netId != vb[i].netId || va[i].name != vb[i].name ||
            va[i].className != vb[i].className) {
            return false;
        }
    }
    for (size_t i = 0; i < oa.size(); i++) {
        if (oa[i].netId != ob[i].netId || oa[i].owner != ob[i].owner) {
            return false;
        }
    }
    return true;
}

// ---------- Tests ----------

void testHandshake() {
    TestHost host;
    CHECK(host.start());
    CHECK(host.link.port() != 0);
    fillHost(host);

    olc::ClientLink client;
    CHECK(client.start(makeClientConfig(host), now()));
    CHECK(client.state() == olc::ClientLink::State::Connecting);

    // Wait for Ready and OWNERSHIP (it comes after the client's UDP_READY).
    bool done = waitFor(host.link, {&client}, [&]() {
        return client.state() == olc::ClientLink::State::Ready &&
               host.link.readyClientCount() == 1 && sameTables(host.table, client.table());
    });
    CHECK(done);
    CHECK(client.state() == olc::ClientLink::State::Ready);
    CHECK(client.clientId() == 1);
    CHECK(host.link.clientCount() == 1);
    CHECK(host.link.readyClientCount() == 1);
    CHECK(host.table.playerCount() == 2);
    CHECK(client.table().vesselEntries().size() == 3);
    CHECK(sameTables(host.table, client.table()));
    CHECK(client.bodies().size() == 2);
    if (client.bodies().size() == 2) {
        CHECK(client.bodies()[0].name == "Sun");
        CHECK(client.bodies()[1].netId == 1 && client.bodies()[1].name == "Earth");
    }
    CHECK(!client.hasWorld());

    std::vector<std::string> hostLog = host.link.takeLog();
    CHECK(hasLine(hostLog, "OLC host: listening on port " + std::to_string(host.link.port())));
    CHECK(hasLine(hostLog, "OLC host: player 'Pilot' joined as client 1 (own scenario)"));
    CHECK(hasLine(client.takeLog(), "OLC client: connecting to 127.0.0.1:"));
}

// A client that loaded the host's scenario returns its number in HELLO and is accepted as usual.
void testHandshakeWithScenarioId() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);

    olc::ClientConfig config = makeClientConfig(host);
    config.scenarioId = 7;
    olc::ClientLink client;
    CHECK(client.start(config, now()));
    CHECK(waitFor(host.link, {&client},
                  [&]() { return client.state() == olc::ClientLink::State::Ready; }));
    CHECK(hasLine(host.link.takeLog(), "OLC host: player 'Pilot' joined as client 1 (scenario #7)"));
}

void testClassesMissing() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);

    olc::ClientConfig config = makeClientConfig(host);
    config.classExists = [](const std::string& className) { return className == "DeltaGlider"; };
    olc::ClientLink client;
    CHECK(client.start(config, now()));

    CHECK(waitFor(host.link, {&client}, [&]() { return host.link.readyClientCount() == 1; }));
    std::vector<std::string> hostLog = host.link.takeLog();
    CHECK(hasLine(hostLog, "OLC host: client 1 is missing classes: ShuttleA"));
    CHECK(!hasLine(hostLog, "DeltaGlider"));

    // A vessel of a missing class added during the session: the host learns it again,
    // but a class already reported is not repeated.
    olc::VesselAdded added;
    added.name = "Atlantis";
    added.className = "Shuttle";
    CHECK(host.table.addVessel(added.name, added.className, added.state.netId));
    CHECK(host.link.sendVesselAdded(added));
    olc::VesselAdded another;
    another.name = "ISS-2";
    another.className = "ShuttleA";
    CHECK(host.table.addVessel(another.name, another.className, another.state.netId));
    CHECK(host.link.sendVesselAdded(another));
    std::vector<std::string> later;
    size_t changes = 0;
    CHECK(waitFor(host.link, {&client}, [&]() {
        std::vector<std::string> got = host.link.takeLog();
        later.insert(later.end(), got.begin(), got.end());
        changes += client.takeVesselChanges().size();
        return hasLine(later, "missing classes: Shuttle") && changes == 2;
    }));
    CHECK(hasLine(later, "OLC host: client 1 is missing classes: Shuttle"));
    CHECK(!hasLine(later, "missing classes: ShuttleA"));
}

// A large WORLD_STATE: 60 vessels with 20 thrusters each do not fit into one packet.
void testWorldState() {
    TestHost host;
    CHECK(host.start());
    olc::WorldState world;
    world.simTime = 100.5;
    world.mjd = 51982.0;
    world.warp = 2.0;
    world.paused = 1;
    for (int i = 0; i < 60; i++) {
        uint16_t netId = 0;
        host.table.addVessel("V" + std::to_string(i), "DeltaGlider", netId);
        olc::VesselState state;
        state.netId = netId;
        state.refBody = 1;
        state.rpos.x = 1000.0 * netId;
        state.thrusters.assign(20, static_cast<uint8_t>(netId));
        world.vessels.push_back(state);
    }

    olc::ClientLink client;
    CHECK(client.start(makeClientConfig(host), now()));
    CHECK(waitFor(host.link, {&client}, [&]() { return host.link.readyClientCount() == 1; }));

    // Send the state until all vessels arrive (repeats with the same simTime are harmless).
    int iteration = 0;
    auto allArrived = [&]() {
        for (const olc::VesselState& vessel : world.vessels) {
            olc::VesselState sampled;
            bool starved = false;
            if (!client.sampleVessel(vessel.netId, world.simTime, sampled, starved)) {
                return false;
            }
        }
        return true;
    };
    bool ok = waitFor(host.link, {&client}, [&]() {
        if (iteration++ % 20 == 0) {
            host.link.sendWorldState(world);
        }
        return allArrived();
    });
    CHECK(ok);
    CHECK(client.hasWorld());
    CHECK(client.hostWarp() == 2.0);
    CHECK(client.hostPaused());
    CHECK(client.stats().worldStatePackets > 1);  // at least two packets
    CHECK(host.link.stats().udpPacketsSent > 1);

    for (const olc::VesselState& vessel : world.vessels) {
        olc::VesselState sampled;
        bool starved = true;
        CHECK(client.sampleVessel(vessel.netId, world.simTime, sampled, starved));
        CHECK(sampled.rpos.x == vessel.rpos.x);
        CHECK(sampled.refBody == 1);
        CHECK(sampled.thrusters.size() == 20);
        CHECK(!starved);
        olc::VesselState newest;
        CHECK(client.latestVessel(vessel.netId, newest));
        CHECK(newest.rpos.x == vessel.rpos.x);
    }
    double mjd = client.hostMjdAt(100.5);
    CHECK(mjd > 51981.999999 && mjd < 51982.000001);
    CHECK(client.hostMjdAt(100.5 + 86400.0) > 51982.999999);

    olc::VesselState unknown;
    bool starved = false;
    CHECK(!client.sampleVessel(500, 100.5, unknown, starved));  // no such vessel
    CHECK(!client.latestVessel(500, unknown));

    // Without vessels the host still sends a packet: the client needs the time and pause.
    olc::WorldState empty;
    empty.simTime = 200.0;
    empty.warp = 1.0;
    empty.paused = 0;
    int iteration2 = 0;
    CHECK(waitFor(host.link, {&client}, [&]() {
        if (iteration2++ % 20 == 0) {
            host.link.sendWorldState(empty);
        }
        return !client.hostPaused();
    }));
    CHECK(client.hostWarp() == 1.0);
}

void testFuelAndLanded() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);

    olc::ClientLink client;
    CHECK(client.start(makeClientConfig(host), now()));
    CHECK(waitFor(host.link, {&client}, [&]() { return host.link.readyClientCount() == 1; }));

    olc::Fuel fuel;
    for (uint16_t id = 0; id < 2; id++) {
        olc::VesselFuel vessel;
        vessel.netId = id;
        vessel.emptyMass = 1000.0 + id;
        vessel.tankLevels = {0.5f, 0.25f};
        fuel.vessels.push_back(vessel);
    }
    olc::Landed landed;
    landed.netId = 2;
    landed.longitude = 0.1;
    landed.latitude = 0.2;
    landed.heading = 0.3;
    host.link.sendLanded(landed);

    std::vector<olc::VesselFuel> gotFuel;
    std::vector<olc::Landed> gotLanded;
    int iteration = 0;
    bool ok = waitFor(host.link, {&client}, [&]() {
        if (iteration++ % 20 == 0 && gotFuel.empty()) {
            host.link.sendFuel(fuel);
        }
        std::vector<olc::VesselFuel> f = client.takeFuel();
        gotFuel.insert(gotFuel.end(), f.begin(), f.end());
        std::vector<olc::Landed> l = client.takeLanded();
        gotLanded.insert(gotLanded.end(), l.begin(), l.end());
        return !gotFuel.empty() && !gotLanded.empty();
    });
    CHECK(ok);
    CHECK(gotFuel.size() == 2);
    if (gotFuel.size() == 2) {
        CHECK(gotFuel[1].netId == 1 && gotFuel[1].emptyMass == 1001.0);
        CHECK(gotFuel[0].tankLevels.size() == 2 && gotFuel[0].tankLevels[1] == 0.25f);
    }
    CHECK(gotLanded.size() == 1);
    if (gotLanded.size() == 1) {
        CHECK(gotLanded[0].netId == 2 && gotLanded[0].heading == 0.3);
    }
    // A second call returns nothing.
    CHECK(client.takeFuel().empty());
    CHECK(client.takeLanded().empty());
}

void testRejectOrbiterVersion() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);

    olc::ClientConfig config = makeClientConfig(host);
    config.orbiterVersion = kOrbiterVersion + 1;
    olc::ClientLink client;
    CHECK(client.start(config, now()));
    CHECK(waitFor(host.link, {&client},
                  [&]() { return client.state() == olc::ClientLink::State::Closed; }));
    CHECK(contains(client.closeReason(), "rejected by host"));
    CHECK(contains(client.closeReason(), "Orbiter version"));
    CHECK(waitFor(host.link, {&client}, [&]() { return host.link.clientCount() == 0; }));
    CHECK(host.table.playerCount() == 1);
}

// ClientLink always writes the correct protocol version, so HELLO is sent by hand.
void testRejectProtocolVersion() {
    TestHost host;
    CHECK(host.start());

    olc::TcpConnection raw;
    olc::Endpoint endpoint;
    endpoint.ip = kLoopback;
    endpoint.port = host.link.port();
    CHECK(raw.startConnect(endpoint));

    olc::Hello hello;
    hello.protocol = static_cast<uint16_t>(olc::kProtocolVersion + 1);
    hello.orbiterVersion = kOrbiterVersion;
    hello.pluginVersion = "test";
    hello.player = "Old";
    olc::Writer body;
    CHECK(olc::writeHello(body, hello));
    raw.send(olc::makeTcpFrame(static_cast<uint8_t>(olc::TcpType::Hello), body.bytes()));

    olc::TcpFrameReader frames;
    uint8_t type = 0;
    std::vector<uint8_t> message;
    bool gotReject = waitFor(host.link, {}, [&]() {
        raw.flush();  // HELLO may have stayed in the queue while connecting
        raw.receive(frames);
        return frames.next(type, message) == olc::TcpFrameReader::Result::Ready;
    });
    CHECK(gotReject);
    CHECK(type == static_cast<uint8_t>(olc::TcpType::Reject));
    olc::Reader reader(message.data(), message.size());
    olc::Reject reject;
    CHECK(olc::readReject(reader, reject));
    CHECK(reject.reason == static_cast<uint8_t>(olc::RejectReason::ProtocolVersion));
    CHECK(host.link.clientCount() == 0);
    CHECK(host.table.playerCount() == 1);
}

// A vessel created and deleted on the host during the session appears and disappears on the client.
void testVesselAddedRemoved() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);
    olc::ClientLink client;
    CHECK(client.start(makeClientConfig(host), now()));
    CHECK(waitFor(host.link, {&client}, [&]() { return host.link.readyClientCount() == 1; }));
    CHECK(client.takeVesselChanges().empty());  // VESSELS at join is not a change

    olc::VesselAdded added;
    added.name = "GL-NEW";
    added.className = "DeltaGlider";
    CHECK(host.table.addVessel(added.name, added.className, added.state.netId));
    added.state.refBody = 1;
    added.state.rpos.x = 6.5e6;
    CHECK(host.link.sendVesselAdded(added));

    std::vector<olc::VesselChange> changes;
    CHECK(waitFor(host.link, {&client}, [&]() {
        std::vector<olc::VesselChange> got = client.takeVesselChanges();
        changes.insert(changes.end(), got.begin(), got.end());
        return !changes.empty();
    }));
    CHECK(changes.size() == 1);
    if (changes.size() == 1) {
        CHECK(changes[0].added);
        CHECK(changes[0].vessel.netId == 3 && changes[0].vessel.name == "GL-NEW");
        CHECK(changes[0].vessel.className == "DeltaGlider");
    }
    CHECK(sameTables(host.table, client.table()));

    // The state from VESSEL_ADDED is already in the buffer.
    olc::VesselState sampled;
    bool starved = false;
    CHECK(client.sampleVessel(3, 0.0, sampled, starved));
    CHECK(sampled.rpos.x == 6.5e6);

    CHECK(host.table.removeVessel(added.state.netId));
    host.link.sendVesselRemoved(added.state.netId);
    changes.clear();
    CHECK(waitFor(host.link, {&client}, [&]() {
        std::vector<olc::VesselChange> got = client.takeVesselChanges();
        changes.insert(changes.end(), got.begin(), got.end());
        return !changes.empty();
    }));
    CHECK(changes.size() == 1);
    if (changes.size() == 1) {
        CHECK(!changes[0].added);
        CHECK(changes[0].vessel.netId == 3 && changes[0].vessel.name == "GL-NEW");
    }
    CHECK(sameTables(host.table, client.table()));
    CHECK(!client.sampleVessel(3, 0.0, sampled, starved));

    std::vector<std::string> hostLog = host.link.takeLog();
    CHECK(hasLine(hostLog, "OLC host: vessel 'GL-NEW' (DeltaGlider) added as 3"));
    CHECK(hasLine(hostLog, "OLC host: vessel 3 removed"));
}

// Takes and releases vessels: two pilots, the rules of SessionTable over the network, a dropout and a return.
void testOwnership() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);  // ISS (net_id 2) is free, the two gliders belong to the host

    olc::ClientConfig annConfig = makeClientConfig(host);
    annConfig.player = "Ann";
    olc::ClientConfig bobConfig = makeClientConfig(host);
    bobConfig.player = "Bob";
    olc::ClientLink ann;
    olc::ClientLink bob;
    CHECK(ann.start(annConfig, now()));
    CHECK(bob.start(bobConfig, now()));
    CHECK(waitFor(host.link, {&ann, &bob}, [&]() { return host.link.readyClientCount() == 2; }));

    // Ann takes the free vessel: everybody sees the new owner.
    ann.requestTake(2);
    CHECK(waitFor(host.link, {&ann, &bob}, [&]() {
        return sameTables(host.table, ann.table()) && sameTables(host.table, bob.table()) &&
               host.table.findById(2)->owner == ann.clientId();
    }));
    CHECK(bob.table().findById(2)->owner == ann.clientId());

    // Bob cannot take Ann's vessel or the host's one; both refusals reach him, nothing changes.
    bob.requestTake(2);
    bob.requestTake(0);
    std::vector<olc::OwnerDenied> denials;
    CHECK(waitFor(host.link, {&ann, &bob}, [&]() {
        std::vector<olc::OwnerDenied> got = bob.takeOwnerDenials();
        denials.insert(denials.end(), got.begin(), got.end());
        return denials.size() == 2;
    }));
    if (denials.size() == 2) {
        CHECK(denials[0].netId == 2 && denials[0].reason == static_cast<uint8_t>(olc::OwnerResult::Busy));
        CHECK(denials[1].netId == 0 && denials[1].reason == static_cast<uint8_t>(olc::OwnerResult::Busy));
    }
    CHECK(host.table.findById(2)->owner == ann.clientId());
    CHECK(ann.takeOwnerDenials().empty());

    // Bob cannot release somebody else's vessel.
    bob.requestRelease(2);
    denials.clear();
    CHECK(waitFor(host.link, {&ann, &bob}, [&]() {
        std::vector<olc::OwnerDenied> got = bob.takeOwnerDenials();
        denials.insert(denials.end(), got.begin(), got.end());
        return !denials.empty();
    }));
    CHECK(denials.size() == 1 && denials[0].reason == static_cast<uint8_t>(olc::OwnerResult::NotOwner));

    // Ann releases, Bob takes it.
    ann.requestRelease(2);
    CHECK(waitFor(host.link, {&ann, &bob}, [&]() { return bob.table().findById(2)->owner == olc::kNoOwner; }));
    bob.requestTake(2);
    CHECK(waitFor(host.link, {&ann, &bob}, [&]() { return ann.table().findById(2)->owner == bob.clientId(); }));

    // Bob drops out: the vessel has no owner, but Ann cannot take it during the hold.
    uint8_t bobId = bob.clientId();
    bob.stop("test");
    CHECK(waitFor(host.link, {&ann}, [&]() { return ann.table().findById(2)->owner == olc::kNoOwner; }));
    ann.requestTake(2);
    denials.clear();
    CHECK(waitFor(host.link, {&ann}, [&]() {
        std::vector<olc::OwnerDenied> got = ann.takeOwnerDenials();
        denials.insert(denials.end(), got.begin(), got.end());
        return !denials.empty();
    }));
    CHECK(denials.size() == 1 && denials[0].reason == static_cast<uint8_t>(olc::OwnerResult::Busy));

    // Bob returns under the same name and gets the vessel back at once, Ann sees it.
    olc::ClientLink bobAgain;
    CHECK(bobAgain.start(bobConfig, now()));
    CHECK(waitFor(host.link, {&ann, &bobAgain}, [&]() {
        return bobAgain.state() == olc::ClientLink::State::Ready &&
               bobAgain.table().findById(2)->owner == bobAgain.clientId() &&
               ann.table().findById(2)->owner == bobAgain.clientId();
    }));
    CHECK(bobAgain.clientId() == bobId);  // the lowest free number is Bob's old one

    std::vector<std::string> hostLog = host.link.takeLog();
    CHECK(hasLine(hostLog, "client " + std::to_string(ann.clientId()) + " ('Ann') takes vessel 'ISS'"));
    CHECK(hasLine(hostLog, "player 'Bob' got his vessel back"));
}

// Input with recognisable values; keys makes packets distinguishable.
olc::Input makeInput(uint16_t netId, uint32_t keys) {
    olc::Input input;
    input.netId = netId;
    input.keys = keys;
    input.rot[0] = 0.5f;
    input.rot[1] = -0.25f;
    input.rot[2] = 1.0f;
    input.lin[0] = -1.0f;
    input.lin[1] = 0.75f;
    input.lin[2] = 0.125f;
    input.throttle = 0.5f;
    return input;
}

// A client with a ready link takes vessel 2 (ISS) and waits until it owns it.
bool takeIss(TestHost& host, olc::ClientLink& client) {
    if (!client.start(makeClientConfig(host), now()) ||
        !waitFor(host.link, {&client}, [&]() { return host.link.readyClientCount() == 1; })) {
        return false;
    }
    client.requestTake(2);
    return waitFor(host.link, {&client}, [&]() {
        return client.table().findById(2)->owner == client.clientId();
    });
}

// INPUT of the owner reaches the host with exactly the same values.
void testInputDelivery() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);
    olc::ClientLink client;
    CHECK(takeIss(host, client));

    olc::Input sent = makeInput(2, olc::kInputPitchUp | olc::kInputFullMain | olc::kInputWheelBrakeRight);
    olc::Input got;
    double receivedAt = -1.0;
    CHECK(!host.link.latestInput(2, got, receivedAt));  // nothing yet
    const double sentAt = now();
    CHECK(client.sendInput(sent, sentAt));
    CHECK(!client.sendInput(sent, sentAt));  // too soon after the previous INPUT
    CHECK(waitFor(host.link, {&client}, [&]() { return host.link.latestInput(2, got, receivedAt); }));
    CHECK(got.netId == 2 && got.keys == sent.keys);
    CHECK(got.rot[0] == 0.5f && got.rot[1] == -0.25f && got.rot[2] == 1.0f);
    CHECK(got.lin[0] == -1.0f && got.lin[1] == 0.75f && got.lin[2] == 0.125f);
    CHECK(got.throttle == 0.5f);
    CHECK(receivedAt > 0.0 && receivedAt <= now());
    CHECK(host.link.stats().inputRejected == 0);

    // The newer packet replaces the older one.
    client.sendInput(makeInput(2, olc::kInputKillMainRetro), now() + 1.0);
    CHECK(waitFor(host.link, {&client}, [&]() {
        return host.link.latestInput(2, got, receivedAt) && got.keys == olc::kInputKillMainRetro;
    }));
}

// INPUT for a vessel the client does not own is dropped and counted.
void testInputNotOwned() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);
    olc::ClientLink client;
    CHECK(takeIss(host, client));

    client.sendInput(makeInput(1, olc::kInputPitchUp), now());  // vessel 1 belongs to the host
    CHECK(waitFor(host.link, {&client}, [&]() { return host.link.stats().inputRejected == 1; }));
    olc::Input got;
    double receivedAt = 0.0;
    CHECK(!host.link.latestInput(1, got, receivedAt));
    CHECK(!host.link.latestInput(2, got, receivedAt));
}

// DOCK reaches the clients in order; a client that joins later gets the links docked now,
// and an undocked link is not sent to it.
void testDock() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);
    olc::ClientLink first;
    CHECK(first.start(makeClientConfig(host), now()));
    CHECK(waitFor(host.link, {&first}, [&]() { return host.link.readyClientCount() == 1; }));

    olc::Dock one;
    one.a = 1;
    one.portA = 0;
    one.b = 2;
    one.portB = 1;
    one.docked = 1;
    olc::Dock two = one;
    two.a = 3;
    two.b = 4;
    olc::Dock oneUndocked = one;
    oneUndocked.a = 2;  // the same link, named from the other side
    oneUndocked.portA = 1;
    oneUndocked.b = 1;
    oneUndocked.portB = 0;
    oneUndocked.docked = 0;
    host.link.sendDock(one);
    host.link.sendDock(two);
    host.link.sendDock(oneUndocked);

    std::vector<olc::Dock> got;
    CHECK(waitFor(host.link, {&first}, [&]() {
        std::vector<olc::Dock> more = first.takeDocks();
        got.insert(got.end(), more.begin(), more.end());
        return got.size() >= 3;
    }));
    CHECK(got.size() == 3);
    CHECK(got[0].a == 1 && got[0].portB == 1 && got[0].docked == 1);
    CHECK(got[1].a == 3 && got[1].b == 4 && got[1].docked == 1);
    CHECK(got[2].a == 2 && got[2].docked == 0);

    // The late client gets only link two.
    olc::ClientLink late;
    CHECK(late.start(makeClientConfig(host), now()));
    std::vector<olc::Dock> lateGot;
    CHECK(waitFor(host.link, {&first, &late}, [&]() {
        std::vector<olc::Dock> more = late.takeDocks();
        lateGot.insert(lateGot.end(), more.begin(), more.end());
        return !lateGot.empty();
    }));
    CHECK(lateGot.size() == 1 && lateGot[0].a == 3 && lateGot[0].b == 4);
}

// KEY of the owner reaches the host in order; KEY for a foreign vessel is dropped and counted;
// takeKeys empties the queue.
void testKeyDelivery() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);
    olc::ClientLink client;
    CHECK(takeIss(host, client));

    client.sendKey(2, 0x13, 0x02);  // Ctrl+R
    client.sendKey(1, 0x14, 0);     // vessel 1 belongs to the host
    client.sendKey(2, 0x1E, 0);
    std::vector<olc::KeyPress> keys;
    CHECK(waitFor(host.link, {&client}, [&]() {
        std::vector<olc::KeyPress> more = host.link.takeKeys();
        keys.insert(keys.end(), more.begin(), more.end());
        return keys.size() >= 2 && host.link.stats().keysRejected == 1;
    }));
    CHECK(keys.size() == 2);
    CHECK(keys[0].netId == 2 && keys[0].key == 0x13 && keys[0].mods == 0x02);
    CHECK(keys[1].netId == 2 && keys[1].key == 0x1E && keys[1].mods == 0);
    CHECK(host.link.stats().keysAccepted == 2);
    CHECK(host.link.takeKeys().empty());
}

// No more than kMaxInputRate packets per second: of ten calls with one time only the first goes out.
void testInputRateLimit() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);
    olc::ClientLink client;
    CHECK(takeIss(host, client));

    double t = now();
    for (uint32_t i = 1; i <= 10; i++) {
        client.sendInput(makeInput(2, i), t);
    }
    olc::Input got;
    double receivedAt = 0.0;
    CHECK(waitFor(host.link, {&client}, [&]() { return host.link.latestInput(2, got, receivedAt); }));
    // Wait a little more: later packets, if any, would have arrived by now.
    waitFor(host.link, {&client}, []() { return false; }, 0.1);
    CHECK(host.link.latestInput(2, got, receivedAt));
    CHECK(got.keys == 1);

    // After 1 / kMaxInputRate the next packet goes out.
    client.sendInput(makeInput(2, 11), t + 1.0 / olc::kMaxInputRate + 0.001);
    CHECK(waitFor(host.link, {&client}, [&]() {
        return host.link.latestInput(2, got, receivedAt) && got.keys == 11;
    }));
}

// INPUT before the link is Ready is not sent; after the vessel is released it is not returned.
void testInputAfterRelease() {
    olc::ClientLink idle;
    idle.sendInput(makeInput(2, 1), now());  // must do nothing and not crash

    TestHost host;
    CHECK(host.start());
    fillHost(host);
    olc::ClientLink client;
    CHECK(takeIss(host, client));

    olc::Input got;
    double receivedAt = 0.0;
    client.sendInput(makeInput(2, olc::kInputFullMain), now());
    CHECK(waitFor(host.link, {&client}, [&]() { return host.link.latestInput(2, got, receivedAt); }));

    client.requestRelease(2);
    CHECK(waitFor(host.link, {&client}, [&]() {
        return client.table().findById(2)->owner == olc::kNoOwner;
    }));
    CHECK(!host.link.latestInput(2, got, receivedAt));
}

// The key bits do not overlap and fit into 24 bits (a check of the constants against typos).
void testInputKeyBits() {
    const uint32_t bits[] = {
        olc::kInputPitchUp, olc::kInputPitchDown, olc::kInputYawLeft, olc::kInputYawRight,
        olc::kInputBankLeft, olc::kInputBankRight, olc::kInputUp, olc::kInputDown,
        olc::kInputLeft, olc::kInputRight, olc::kInputForward, olc::kInputBack,
        olc::kInputLowPowerRcs, olc::kInputFullMain, olc::kInputFullRetro,
        olc::kInputKillMainRetro, olc::kInputThrottleUp, olc::kInputThrottleDown,
        olc::kInputHoverUp, olc::kInputHoverDown, olc::kInputTrimUp, olc::kInputTrimDown,
        olc::kInputWheelBrakeLeft, olc::kInputWheelBrakeRight};
    uint32_t all = 0;
    for (size_t i = 0; i < sizeof(bits) / sizeof(bits[0]); i++) {
        CHECK(bits[i] == (1u << i));  // bit i, in the order of PROTOCOL.md section 7
        CHECK((all & bits[i]) == 0);
        all |= bits[i];
    }
    CHECK(all == 0x00FFFFFFu);
}

// An OWNER_REQUEST with an unknown action: the host drops the client.
void testBrokenOwnerRequest() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);

    olc::TcpConnection raw;
    olc::Endpoint endpoint;
    endpoint.ip = kLoopback;
    endpoint.port = host.link.port();
    CHECK(raw.startConnect(endpoint));

    olc::Hello hello;
    hello.protocol = olc::kProtocolVersion;
    hello.orbiterVersion = kOrbiterVersion;
    hello.scenarioId = olc::kOwnScenario;
    hello.pluginVersion = "test";
    hello.player = "Raw";
    olc::Writer helloBody;
    CHECK(olc::writeHello(helloBody, hello));
    raw.send(olc::makeTcpFrame(static_cast<uint8_t>(olc::TcpType::Hello), helloBody.bytes()));

    olc::OwnerRequest request;
    request.netId = 2;
    request.action = 7;  // neither take nor release
    olc::Writer requestBody;
    olc::writeOwnerRequest(requestBody, request);
    raw.send(olc::makeTcpFrame(static_cast<uint8_t>(olc::TcpType::OwnerRequest),
                               requestBody.bytes()));

    olc::TcpFrameReader frames;
    bool closed = waitFor(host.link, {}, [&]() {
        raw.flush();
        return !raw.receive(frames) && raw.state() == olc::TcpConnection::State::Closed;
    });
    CHECK(closed);
    CHECK(hasLine(host.link.takeLog(), "broken OWNER_REQUEST"));
}

// Starts a client that asks for the host's scenario and waits until it is closed.
void requestScenario(TestHost& host, olc::ClientLink& client) {
    olc::ClientConfig config = makeClientConfig(host);
    config.scenarioId = olc::kNoScenario;
    CHECK(client.start(config, now()));
    CHECK(waitFor(host.link, {&client},
                  [&]() { return client.state() == olc::ClientLink::State::Closed; }));
}

// HELLO without a scenario number: the host sends its world as a file and closes the connection.
// The file is close to the limit, so it goes out over many polls.
// Finds a player in a PLAYERS list by name; nullptr if there is none.
const olc::PlayerEntry* findPlayer(const std::vector<olc::PlayerEntry>& players,
                                   const std::string& name) {
    for (const olc::PlayerEntry& entry : players) {
        if (entry.player == name) {
            return &entry;
        }
    }
    return nullptr;
}

void testDirectorAndTimeRequests() {
    TestHost host;
    host.director = "Boss";
    CHECK(host.start());
    fillHost(host);

    olc::ClientConfig bossConfig = makeClientConfig(host);
    bossConfig.player = "Boss";
    olc::ClientConfig pilotConfig = makeClientConfig(host);
    pilotConfig.player = "Pilot";
    olc::ClientLink boss;
    olc::ClientLink pilot;
    CHECK(boss.start(bossConfig, now()));
    CHECK(pilot.start(pilotConfig, now()));
    CHECK(waitFor(host.link, {&boss, &pilot}, [&]() { return host.link.readyClientCount() == 2; }));

    // WELCOME tells who is the director; the director is an observer too.
    CHECK(boss.isDirector() && boss.isObserver());
    CHECK(!pilot.isDirector() && !pilot.isObserver());

    // A request from the pilot is ignored, the director's one reaches the plugin.
    pilot.sendTimeRequest(true, 5.0);
    boss.sendTimeRequest(true, 10.0);
    CHECK(waitFor(host.link, {&boss, &pilot}, [&]() {
        return host.link.stats().timeRequestsAccepted == 1 &&
               host.link.stats().timeRequestsRejected == 1;
    }));
    std::vector<olc::TimeRequest> requests = host.link.takeTimeRequests();
    CHECK(requests.size() == 1);
    if (requests.size() == 1) {
        CHECK(requests[0].paused == 1 && requests[0].warp == 10.0);
    }
    CHECK(host.link.takeTimeRequests().empty());

    // The director cannot take a vessel: OWNER_DENIED, reason Observer.
    boss.requestTake(2);
    std::vector<olc::OwnerDenied> denials;
    CHECK(waitFor(host.link, {&boss, &pilot}, [&]() {
        std::vector<olc::OwnerDenied> got = boss.takeOwnerDenials();
        denials.insert(denials.end(), got.begin(), got.end());
        return !denials.empty();
    }));
    if (!denials.empty()) {
        CHECK(denials[0].netId == 2 &&
              denials[0].reason == static_cast<uint8_t>(olc::OwnerResult::Observer));
    }
    CHECK(host.table.findById(2)->owner == olc::kNoOwner);

    // SAVE_REQUEST: the pilot's is ignored, the director's is counted once.
    pilot.sendSaveRequest();
    boss.sendSaveRequest();
    CHECK(waitFor(host.link, {&boss, &pilot}, [&]() {
        return host.link.stats().saveRequestsAccepted == 1 &&
               host.link.stats().saveRequestsRejected == 1;
    }));
    CHECK(host.link.takeSaveRequests() == 1);
    CHECK(host.link.takeSaveRequests() == 0);

    // A broken request (warp 0) drops the sender.
    pilot.sendTimeRequest(false, 0.0);
    CHECK(waitFor(host.link, {&boss, &pilot}, [&]() {
        return pilot.state() == olc::ClientLink::State::Closed;
    }));
}

void testObserverFlag() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);

    olc::ClientConfig config = makeClientConfig(host);
    config.observer = true;
    olc::ClientLink watcher;
    CHECK(watcher.start(config, now()));
    CHECK(waitFor(host.link, {&watcher}, [&]() { return host.link.readyClientCount() == 1; }));
    CHECK(watcher.isObserver() && !watcher.isDirector());

    watcher.requestTake(2);
    std::vector<olc::OwnerDenied> denials;
    CHECK(waitFor(host.link, {&watcher}, [&]() {
        std::vector<olc::OwnerDenied> got = watcher.takeOwnerDenials();
        denials.insert(denials.end(), got.begin(), got.end());
        return !denials.empty();
    }));
    if (!denials.empty()) {
        CHECK(denials[0].reason == static_cast<uint8_t>(olc::OwnerResult::Observer));
    }

    // Without a director nobody may pause: even the observer's request is ignored.
    watcher.sendTimeRequest(true, 2.0);
    CHECK(waitFor(host.link, {&watcher}, [&]() {
        return host.link.stats().timeRequestsRejected == 1;
    }));
    CHECK(host.link.takeTimeRequests().empty());
}

void testPlayersList() {
    TestHost host;
    host.director = "Boss";
    CHECK(host.start());
    fillHost(host);  // vessels 0 and 1 belong to the host, 2 is free

    olc::ClientConfig bossConfig = makeClientConfig(host);
    bossConfig.player = "Boss";
    olc::ClientConfig annConfig = makeClientConfig(host);
    annConfig.player = "Ann";
    olc::ClientLink boss;
    olc::ClientLink ann;
    CHECK(boss.start(bossConfig, now()));
    CHECK(ann.start(annConfig, now()));
    CHECK(waitFor(host.link, {&boss, &ann}, [&]() {
        return boss.players().size() == 3 && ann.players().size() == 3;
    }));

    // The host is client 0 with its first vessel; the director is marked; Ann has no vessel yet.
    const olc::PlayerEntry* hostEntry = findPlayer(ann.players(), "Host");
    const olc::PlayerEntry* bossEntry = findPlayer(ann.players(), "Boss");
    const olc::PlayerEntry* annEntry = findPlayer(ann.players(), "Ann");
    CHECK(hostEntry != nullptr && bossEntry != nullptr && annEntry != nullptr);
    if (hostEntry && bossEntry && annEntry) {
        CHECK(hostEntry->clientId == olc::kHostOwner && hostEntry->netId == 0);
        CHECK(bossEntry->flags == (olc::kPlayerDirector | olc::kPlayerObserver));
        CHECK(bossEntry->netId == olc::kNoVessel);
        CHECK(annEntry->clientId == ann.clientId() && annEntry->flags == 0 &&
              annEntry->netId == olc::kNoVessel);
    }

    // A host without graphics does not play: only the clients are listed.
    TestHost headlessHost;
    headlessHost.headless = true;
    CHECK(headlessHost.start());
    olc::ClientConfig soloConfig = makeClientConfig(headlessHost);
    soloConfig.player = "Solo";
    olc::ClientLink solo;
    CHECK(solo.start(soloConfig, now()));
    CHECK(waitFor(headlessHost.link, {&solo}, [&]() { return solo.players().size() == 1; }));
    CHECK(findPlayer(solo.players(), "Host") == nullptr && findPlayer(solo.players(), "Solo") != nullptr);

    // Ann takes a vessel: the list changes for everybody.
    ann.requestTake(2);
    CHECK(waitFor(host.link, {&boss, &ann}, [&]() {
        const olc::PlayerEntry* entry = findPlayer(boss.players(), "Ann");
        return entry != nullptr && entry->netId == 2;
    }));

    // The periodic PLAYERS carries Ann's round trip time (UDP PING reports it). The wait is
    // longer than kPlayersInterval plus a few ping periods.
    CHECK(waitFor(host.link, {&boss, &ann}, [&]() {
        const olc::PlayerEntry* entry = findPlayer(boss.players(), "Ann");
        return entry != nullptr && entry->rttMs != olc::kRttUnknown;
    }, 6.0));

    // Ann leaves: she disappears from the list and her vessel is free.
    ann.stop("test");
    CHECK(waitFor(host.link, {&boss}, [&]() { return boss.players().size() == 2; }));
    CHECK(findPlayer(boss.players(), "Ann") == nullptr);
}

void testScenarioRequest() {
    std::vector<uint8_t> world(olc::kMaxScenarioSize);
    for (size_t i = 0; i < world.size(); i++) {
        world[i] = static_cast<uint8_t>(i * 7 + i / 251);
    }
    TestHost host;
    int saves = 0;
    host.makeScenario = [&](olc::Scenario& scenario) {
        saves++;
        scenario.name = "Host world";
        scenario.data = world;
        return true;
    };
    CHECK(host.start());
    fillHost(host);

    olc::ClientLink client;
    requestScenario(host, client);
    CHECK(client.closeReason() == "scenario received");
    CHECK(client.hasScenario());
    CHECK(client.scenario().scenarioId == 1);
    CHECK(client.scenario().name == "Host world");
    CHECK(client.scenario().data == world);
    CHECK(saves == 1);

    // The host lets the connection go and does not count it as a player.
    CHECK(waitFor(host.link, {}, [&]() { return host.link.clientCount() == 0; }));
    CHECK(host.table.playerCount() == 1);
    std::vector<std::string> hostLog = host.link.takeLog();
    CHECK(hasLine(hostLog, "OLC host: sent scenario #1 ("));
    CHECK(hasLine(hostLog, "closed (scenario sent)"));

    // The next save gets the next number.
    olc::ClientLink again;
    requestScenario(host, again);
    CHECK(again.hasScenario() && again.scenario().scenarioId == 2);
}

// The host cannot give a scenario: REJECT with a clear reason, and the client has no file.
void testScenarioUnavailable() {
    TestHost noSave;  // makeScenario is not set
    CHECK(noSave.start());
    olc::ClientLink first;
    requestScenario(noSave, first);
    CHECK(!first.hasScenario());
    CHECK(contains(first.closeReason(), "rejected by host"));
    CHECK(contains(first.closeReason(), "could not save"));

    TestHost tooLarge;
    tooLarge.makeScenario = [](olc::Scenario& scenario) {
        scenario.name = "Huge";
        scenario.data.assign(olc::kMaxScenarioSize + 1, 'x');
        return true;
    };
    CHECK(tooLarge.start());
    olc::ClientLink second;
    requestScenario(tooLarge, second);
    CHECK(!second.hasScenario());
    CHECK(contains(second.closeReason(), "too large"));

    // No free place: the scenario is not even saved.
    TestHost full(2);
    int saves = 0;
    full.makeScenario = [&](olc::Scenario& scenario) {
        saves++;
        scenario.name = "World";
        return true;
    };
    CHECK(full.start());
    fillHost(full);
    olc::ClientLink player;
    CHECK(player.start(makeClientConfig(full), now()));
    CHECK(waitFor(full.link, {&player},
                  [&]() { return player.state() == olc::ClientLink::State::Ready; }));
    olc::ClientLink third;
    olc::ClientConfig config = makeClientConfig(full);
    config.scenarioId = olc::kNoScenario;
    CHECK(third.start(config, now()));
    CHECK(waitFor(full.link, {&player, &third},
                  [&]() { return third.state() == olc::ClientLink::State::Closed; }));
    CHECK(contains(third.closeReason(), "no free player slots"));
    CHECK(saves == 0);
    CHECK(player.state() == olc::ClientLink::State::Ready);
}

// A message before HELLO - the client is disconnected.
void testMessageBeforeHello() {
    TestHost host;
    CHECK(host.start());

    olc::TcpConnection raw;
    olc::Endpoint endpoint;
    endpoint.ip = kLoopback;
    endpoint.port = host.link.port();
    CHECK(raw.startConnect(endpoint));
    olc::Writer body;
    olc::writeUdpReady(body, 1234);
    raw.send(olc::makeTcpFrame(static_cast<uint8_t>(olc::TcpType::UdpReady), body.bytes()));

    olc::TcpFrameReader frames;
    bool closed = waitFor(host.link, {}, [&]() {
        raw.flush();  // the message may have stayed in the queue while connecting
        return !raw.receive(frames) && raw.state() == olc::TcpConnection::State::Closed;
    });
    CHECK(closed);
    CHECK(host.link.readyClientCount() == 0);
    CHECK(hasLine(host.link.takeLog(), "message received before HELLO"));
}

void testPlayerLimit() {
    TestHost host(2);  // the host and one client
    CHECK(host.start());
    fillHost(host);

    olc::ClientLink first;
    CHECK(first.start(makeClientConfig(host), now()));
    CHECK(waitFor(host.link, {&first},
                  [&]() { return first.state() == olc::ClientLink::State::Ready; }));

    olc::ClientConfig config = makeClientConfig(host);
    config.player = "Second";
    olc::ClientLink second;
    CHECK(second.start(config, now()));
    CHECK(waitFor(host.link, {&first, &second},
                  [&]() { return second.state() == olc::ClientLink::State::Closed; }));
    CHECK(contains(second.closeReason(), "no free player slots"));
    CHECK(first.state() == olc::ClientLink::State::Ready);
    CHECK(host.link.clientCount() == 1);
    CHECK(host.table.playerCount() == 2);
}

void testOtherSystem() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);

    olc::ClientConfig config = makeClientConfig(host);
    config.system = "Mars";
    olc::ClientLink client;
    CHECK(client.start(config, now()));
    CHECK(waitFor(host.link, {&client},
                  [&]() { return client.state() == olc::ClientLink::State::Closed; }));
    CHECK(contains(client.closeReason(), "system"));
    // The host gets BYE and frees the slot.
    CHECK(waitFor(host.link, {&client}, [&]() { return host.link.clientCount() == 0; }));
    CHECK(host.table.playerCount() == 1);

    // The system name is compared ignoring case.
    config.system = "SOL";
    olc::ClientLink same;
    CHECK(same.start(config, now()));
    CHECK(waitFor(host.link, {&same},
                  [&]() { return same.state() == olc::ClientLink::State::Ready; }));
}

void testPingPong() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);

    olc::ClientLink client;
    CHECK(client.start(makeClientConfig(host), now()));
    CHECK(client.rtt() < 0.0);
    CHECK(waitFor(host.link, {&client}, [&]() { return client.rtt() >= 0.0; }, 5.0));
    // Keep working a bit longer: the next PING goes out after kPingInterval.
    waitFor(host.link, {&client}, []() { return false; }, 1.2);
    CHECK(client.rtt() >= 0.0);
    CHECK(client.rtt() < 1.0);
    CHECK(client.state() == olc::ClientLink::State::Ready);
    CHECK(host.link.stats().udpPacketsSent >= 2);  // at least two PONGs
}

// A client behind NAT: UDP_READY has the port on the client machine, but the host gets packets from another one.
// The host must send WORLD_STATE where the client's PING came from, not to the port in UDP_READY.
void testUdpAddressFromPing() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);

    // A client without ClientLink: the handshake by hand.
    olc::TcpConnection raw;
    olc::Endpoint hostTcp;
    hostTcp.ip = kLoopback;
    hostTcp.port = host.link.port();
    CHECK(raw.startConnect(hostTcp));
    olc::Hello hello;
    hello.protocol = olc::kProtocolVersion;
    hello.orbiterVersion = kOrbiterVersion;
    hello.scenarioId = olc::kOwnScenario;
    hello.pluginVersion = "test";
    hello.player = "Behind NAT";
    olc::Writer helloBody;
    CHECK(olc::writeHello(helloBody, hello));
    raw.send(olc::makeTcpFrame(static_cast<uint8_t>(olc::TcpType::Hello), helloBody.bytes()));

    // Wait for WELCOME and take the client number from it.
    olc::TcpFrameReader frames;
    uint8_t clientId = 0;
    bool welcomed = waitFor(host.link, {}, [&]() {
        raw.flush();
        raw.receive(frames);
        uint8_t type = 0;
        std::vector<uint8_t> message;
        while (frames.next(type, message) == olc::TcpFrameReader::Result::Ready) {
            if (type == static_cast<uint8_t>(olc::TcpType::Welcome)) {
                olc::Reader reader(message.data(), message.size());
                olc::Welcome welcome;
                if (olc::readWelcome(reader, welcome)) {
                    clientId = welcome.clientId;
                    return true;
                }
            }
        }
        return false;
    });
    CHECK(welcomed);

    // UDP_READY with a "wrong" port: that is how a client behind NAT sees its port.
    olc::UdpSocket natPort;  // the client's real address as the host sees it
    CHECK(natPort.open(0));
    olc::Writer readyBody;
    olc::writeUdpReady(readyBody, static_cast<uint16_t>(natPort.localPort() + 1));
    raw.send(olc::makeTcpFrame(static_cast<uint8_t>(olc::TcpType::UdpReady), readyBody.bytes()));
    CHECK(waitFor(host.link, {}, [&]() {
        raw.flush();
        return host.link.readyClientCount() == 1;
    }));

    // PING with the client number from the real address.
    olc::Writer pingPacket;
    olc::UdpHeader header;
    header.type = static_cast<uint8_t>(olc::UdpType::Ping);
    header.session = kSession;
    olc::writeUdpHeader(pingPacket, header);
    olc::Ping ping;
    ping.time = 1.0;
    ping.clientId = clientId;
    olc::writePing(pingPacket, ping);
    olc::Endpoint hostUdp = hostTcp;
    CHECK(natPort.sendTo(hostUdp, pingPacket.bytes()));

    // After PING the host sends to the real address: PONG and WORLD_STATE.
    bool gotWorld = false;
    bool learned = waitFor(host.link, {}, [&]() {
        olc::WorldState world;
        world.simTime = 5.0;
        host.link.sendWorldState(world);
        std::vector<uint8_t> packet;
        olc::Endpoint from;
        while (natPort.receiveFrom(packet, from)) {
            olc::Reader reader(packet.data(), packet.size());
            olc::UdpHeader received;
            if (olc::readUdpHeader(reader, received) == olc::UdpHeaderResult::Ok &&
                received.type == static_cast<uint8_t>(olc::UdpType::WorldState)) {
                gotWorld = true;
            }
        }
        return gotWorld;
    });
    CHECK(learned);
    CHECK(hasLine(host.link.takeLog(), "(NAT?)"));
}

void testClientStop() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);

    olc::ClientLink client;
    CHECK(client.start(makeClientConfig(host), now()));
    CHECK(waitFor(host.link, {&client}, [&]() { return host.link.readyClientCount() == 1; }));
    CHECK(host.table.playerCount() == 2);

    client.stop("test finished");
    CHECK(client.state() == olc::ClientLink::State::Closed);
    CHECK(waitFor(host.link, {}, [&]() { return host.link.clientCount() == 0; }));
    CHECK(host.table.playerCount() == 1);
    CHECK(hasLine(host.link.takeLog(), "OLC host: client 1 left (test finished)"));
}

void testHostStop() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);

    olc::ClientLink client;
    CHECK(client.start(makeClientConfig(host), now()));
    CHECK(waitFor(host.link, {&client}, [&]() { return host.link.readyClientCount() == 1; }));

    host.link.stop("host shut down");
    CHECK(host.link.port() == 0);
    CHECK(waitFor(host.link, {&client},
                  [&]() { return client.state() == olc::ClientLink::State::Closed; }));
    CHECK(contains(client.closeReason(), "host left"));
    CHECK(contains(client.closeReason(), "host shut down"));
}

// The host counts foreign and broken UDP packets in its statistics.
void testBadUdp() {
    TestHost host;
    CHECK(host.start());

    olc::UdpSocket udp;
    CHECK(udp.open(0));
    olc::Endpoint to;
    to.ip = kLoopback;
    to.port = host.link.port();
    CHECK(udp.sendTo(to, {1, 2, 3}));  // shorter than the header

    olc::Writer writer;  // a correct header but a foreign session
    olc::UdpHeader header;
    header.type = static_cast<uint8_t>(olc::UdpType::Ping);
    header.session = kSession + 1;
    olc::writeUdpHeader(writer, header);
    olc::writePing(writer, olc::Ping());
    CHECK(udp.sendTo(to, writer.bytes()));

    CHECK(waitFor(host.link, {}, [&]() { return host.link.stats().udpBadPackets == 2; }));
}

// A host that does not exist: the client does not hang but gets a clear reason.
void testConnectionRefused() {
    // Take a port, learn its number and close it: nobody listens on it.
    olc::TcpListener listener;
    CHECK(listener.open(0));
    olc::Endpoint endpoint;
    endpoint.ip = kLoopback;
    endpoint.port = listener.localPort();
    listener.close();

    olc::ClientConfig config;
    config.host = endpoint;
    config.player = "Lonely";
    config.system = "Sol";
    olc::ClientLink client;
    client.start(config, now());
    bool closed = false;
    double deadline = now() + 3.0;
    while (now() < deadline && !closed) {
        client.poll(now());
        closed = client.state() == olc::ClientLink::State::Closed;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(closed);
    CHECK(contains(client.closeReason(), "connection refused"));
}

}  // namespace

// A side that hears nothing for kSilenceTimeout drops the other one: a frozen computer or a pulled
// cable leaves the TCP connection open. The test polls with a time kSilenceTimeout ahead instead of
// waiting.
void testSilence() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);
    olc::ClientLink client;
    CHECK(client.start(makeClientConfig(host), now()));
    CHECK(waitFor(host.link, {&client}, [&]() { return host.link.readyClientCount() == 1; }));
    // A ClientLink is started once.
    CHECK(!client.start(makeClientConfig(host), now()));

    // The client stops talking: the host reads what is left and then drops it.
    for (int i = 0; i < 3; i++) {
        host.link.poll(now());
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK(host.link.clientCount() == 1);
    host.link.poll(now() + olc::kSilenceTimeout + 1.0);
    CHECK(host.link.clientCount() == 0);
    CHECK(hasLine(host.link.takeLog(), "OLC host: client 1 left (nothing heard for 15 s)"));

    // The host stops talking to a second client: the client closes by itself.
    olc::ClientLink second;
    CHECK(second.start(makeClientConfig(host), now()));
    CHECK(waitFor(host.link, {&second}, [&]() { return host.link.readyClientCount() == 1; }));
    for (int i = 0; i < 3; i++) {
        second.poll(now());
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK(second.state() == olc::ClientLink::State::Ready);
    second.poll(now() + olc::kSilenceTimeout + 1.0);
    CHECK(second.state() == olc::ClientLink::State::Closed);
    CHECK(contains(second.closeReason(), "nothing heard from the host"));
}

int main() {
    CHECK(olc::startNetwork());
    testHandshake();
    testClassesMissing();
    testWorldState();
    testFuelAndLanded();
    testRejectOrbiterVersion();
    testRejectProtocolVersion();
    testHandshakeWithScenarioId();
    testVesselAddedRemoved();
    testOwnership();
    testInputDelivery();
    testInputNotOwned();
    testInputRateLimit();
    testInputAfterRelease();
    testInputKeyBits();
    testKeyDelivery();
    testDock();
    testBrokenOwnerRequest();
    testDirectorAndTimeRequests();
    testObserverFlag();
    testPlayersList();
    testScenarioRequest();
    testScenarioUnavailable();
    testMessageBeforeHello();
    testPlayerLimit();
    testOtherSystem();
    testPingPong();
    testUdpAddressFromPing();
    testClientStop();
    testHostStop();
    testBadUdp();
    testSilence();
    testConnectionRefused();
    olc::stopNetwork();
    return checkResult();
}
