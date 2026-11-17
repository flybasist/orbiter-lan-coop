// Тесты HostLink и ClientLink вместе: рукопожатие, отказы, рассылка состояния, топливо,
// посадка, PING/PONG и отключение. Только loopback 127.0.0.1 и порт 0 (любой свободный).
// Всё ожидание идёт циклом poll + пауза 1 мс и ограничено по времени, так что тест
// не может зависнуть.

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

// Реальное время в секундах от первого вызова.
double now() {
    static const auto start = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    return elapsed.count();
}

// Хост с таблицей. Таблица объявлена первой: она живёт дольше HostLink.
struct TestHost {
    explicit TestHost(int maxPlayers = olc::kDefaultMaxPlayers)
        : table(maxPlayers), link(table) {}

    // Запускает хост на порту 0.
    bool start() {
        olc::HostConfig config;
        config.port = 0;
        config.session = kSession;
        config.orbiterVersion = kOrbiterVersion;
        config.system = "Sol";
        config.stateRateHz = 30;
        return link.start(config);
    }

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

// Опрашивает хост и клиентов раз в 1 мс, пока condition не станет true или не выйдет время.
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

// Три корабля и два тела: набор для рукопожатия.
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

// Одинаковы ли списки кораблей двух таблиц.
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

// ---------- Тесты ----------

void testHandshake() {
    TestHost host;
    CHECK(host.start());
    CHECK(host.link.port() != 0);
    fillHost(host);

    olc::ClientLink client;
    CHECK(client.start(makeClientConfig(host), now()));
    CHECK(client.state() == olc::ClientLink::State::Connecting);

    // Ждём Ready и OWNERSHIP (он приходит уже после UDP_READY клиента).
    bool done = waitFor(host.link, {&client}, [&]() {
        return client.state() == olc::ClientLink::State::Ready &&
               host.link.readyClientCount() == 1 && sameTables(host.table, client.table());
    });
    CHECK(done);
    CHECK(client.state() == olc::ClientLink::State::Ready);
    CHECK(client.clientId() == 1);
    CHECK(client.stateRateHz() == 30);
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
    CHECK(hasLine(hostLog, "OLC host: player 'Pilot' joined as client 1"));
    CHECK(hasLine(client.takeLog(), "OLC client: connecting to 127.0.0.1:"));
}

void testClassesMissing() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);

    olc::ClientConfig config = makeClientConfig(host);
    config.classExists = [](const std::string& className) { return className != "ShuttleA"; };
    olc::ClientLink client;
    CHECK(client.start(config, now()));

    CHECK(waitFor(host.link, {&client}, [&]() { return host.link.readyClientCount() == 1; }));
    std::vector<std::string> hostLog = host.link.takeLog();
    CHECK(hasLine(hostLog, "OLC host: client 1 is missing classes: ShuttleA"));
    CHECK(!hasLine(hostLog, "DeltaGlider"));
}

// Большой WORLD_STATE: 60 кораблей по 20 двигателей не помещаются в один пакет.
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

    // Шлём состояние, пока не придут все корабли (повторы с тем же simTime безвредны).
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
    CHECK(client.stats().worldStatePackets > 1);  // не меньше двух пакетов
    CHECK(host.link.stats().udpPacketsSent > 1);

    for (const olc::VesselState& vessel : world.vessels) {
        olc::VesselState sampled;
        bool starved = true;
        CHECK(client.sampleVessel(vessel.netId, world.simTime, sampled, starved));
        CHECK(sampled.rpos.x == vessel.rpos.x);
        CHECK(sampled.refBody == 1);
        CHECK(sampled.thrusters.size() == 20);
        CHECK(!starved);
    }
    double mjd = client.hostMjdAt(100.5);
    CHECK(mjd > 51981.999999 && mjd < 51982.000001);
    CHECK(client.hostMjdAt(100.5 + 86400.0) > 51982.999999);

    olc::VesselState unknown;
    bool starved = false;
    CHECK(!client.sampleVessel(500, 100.5, unknown, starved));  // такого корабля нет

    // Без кораблей хост всё равно шлёт пакет: время и пауза нужны клиенту.
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
    // Второй вызов ничего не возвращает.
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

// Версию протокола ClientLink всегда пишет верную, поэтому HELLO шлём вручную.
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
        raw.flush();  // HELLO мог остаться в очереди, пока шло подключение
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

// Сообщение до HELLO - клиент отключается.
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
        raw.flush();  // сообщение могло остаться в очереди, пока шло подключение
        return !raw.receive(frames) && raw.state() == olc::TcpConnection::State::Closed;
    });
    CHECK(closed);
    CHECK(host.link.readyClientCount() == 0);
    CHECK(hasLine(host.link.takeLog(), "message received before HELLO"));
}

void testPlayerLimit() {
    TestHost host(2);  // хост и один клиент
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
    // Хост получает BYE и освобождает место.
    CHECK(waitFor(host.link, {&client}, [&]() { return host.link.clientCount() == 0; }));
    CHECK(host.table.playerCount() == 1);

    // Имя системы сравнивается без учёта регистра.
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
    // Работаем ещё немного: следующий PING уходит через kPingInterval.
    waitFor(host.link, {&client}, []() { return false; }, 1.2);
    CHECK(client.rtt() >= 0.0);
    CHECK(client.rtt() < 1.0);
    CHECK(client.state() == olc::ClientLink::State::Ready);
    CHECK(host.link.stats().udpPacketsSent >= 2);  // не меньше двух PONG
}

// Клиент за NAT: в UDP_READY - порт на машине клиента, а хосту пакеты приходят с другого.
// Хост должен слать WORLD_STATE туда, откуда пришёл PING клиента, а не на порт из UDP_READY.
void testUdpAddressFromPing() {
    TestHost host;
    CHECK(host.start());
    fillHost(host);

    // Клиент без ClientLink: рукопожатие вручную.
    olc::TcpConnection raw;
    olc::Endpoint hostTcp;
    hostTcp.ip = kLoopback;
    hostTcp.port = host.link.port();
    CHECK(raw.startConnect(hostTcp));
    olc::Hello hello;
    hello.protocol = olc::kProtocolVersion;
    hello.orbiterVersion = kOrbiterVersion;
    hello.pluginVersion = "test";
    hello.player = "Behind NAT";
    olc::Writer helloBody;
    CHECK(olc::writeHello(helloBody, hello));
    raw.send(olc::makeTcpFrame(static_cast<uint8_t>(olc::TcpType::Hello), helloBody.bytes()));

    // Ждём WELCOME и берём из него номер клиента.
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

    // UDP_READY с «чужим» портом: так видит свой порт клиент за NAT.
    olc::UdpSocket natPort;  // настоящий адрес клиента, каким его видит хост
    CHECK(natPort.open(0));
    olc::Writer readyBody;
    olc::writeUdpReady(readyBody, static_cast<uint16_t>(natPort.localPort() + 1));
    raw.send(olc::makeTcpFrame(static_cast<uint8_t>(olc::TcpType::UdpReady), readyBody.bytes()));
    CHECK(waitFor(host.link, {}, [&]() {
        raw.flush();
        return host.link.readyClientCount() == 1;
    }));

    // PING с номером клиента с настоящего адреса.
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

    // После PING хост шлёт состояние на настоящий адрес: PONG и WORLD_STATE.
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

// Чужие и битые UDP-пакеты хост считает в статистике.
void testBadUdp() {
    TestHost host;
    CHECK(host.start());

    olc::UdpSocket udp;
    CHECK(udp.open(0));
    olc::Endpoint to;
    to.ip = kLoopback;
    to.port = host.link.port();
    CHECK(udp.sendTo(to, {1, 2, 3}));  // короче заголовка

    olc::Writer writer;  // верный заголовок, но чужая сессия
    olc::UdpHeader header;
    header.type = static_cast<uint8_t>(olc::UdpType::Ping);
    header.session = kSession + 1;
    olc::writeUdpHeader(writer, header);
    olc::writePing(writer, olc::Ping());
    CHECK(udp.sendTo(to, writer.bytes()));

    CHECK(waitFor(host.link, {}, [&]() { return host.link.stats().udpBadPackets == 2; }));
}

// Хост, которого нет: клиент не зависает, а получает понятную причину.
void testConnectionRefused() {
    // Занимаем порт, узнаём номер и закрываем: на нём никто не слушает.
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

int main() {
    CHECK(olc::startNetwork());
    testHandshake();
    testClassesMissing();
    testWorldState();
    testFuelAndLanded();
    testRejectOrbiterVersion();
    testRejectProtocolVersion();
    testMessageBeforeHello();
    testPlayerLimit();
    testOtherSystem();
    testPingPong();
    testUdpAddressFromPing();
    testClientStop();
    testHostStop();
    testBadUdp();
    testConnectionRefused();
    olc::stopNetwork();
    return checkResult();
}
