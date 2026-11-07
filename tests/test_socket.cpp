// Тесты обёртки над сокетами: разбор адреса, TCP (кадры, большое сообщение, обрыв,
// отказ в подключении) и UDP. Только loopback 127.0.0.1 и порт 0 (любой свободный):
// фиксированных портов нет, поэтому тест не мешает другим программам.
// Ожидание ограничено 2 секундами, так что тест не может зависнуть.

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "check.h"
#include "header.h"
#include "messages.h"
#include "protocol.h"
#include "socket.h"

namespace {

constexpr uint32_t kLoopback = 0x7F000001;  // 127.0.0.1

// Повторяет проверку condition раз в 1 мс, пока она не вернёт true.
// Возвращает false, если за 2 секунды не дождались.
bool waitFor(const std::function<bool()>& condition) {
    for (int i = 0; i < 2000; i++) {
        if (condition()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return condition();
}

olc::Endpoint loopbackEndpoint(uint16_t port) {
    olc::Endpoint endpoint;
    endpoint.ip = kLoopback;
    endpoint.port = port;
    return endpoint;
}

void testParseIpv4() {
    uint32_t ip = 0;
    CHECK(olc::parseIpv4("127.0.0.1", ip));
    CHECK(ip == 0x7F000001);
    CHECK(olc::parseIpv4("192.168.1.10", ip));
    CHECK(ip == 0xC0A8010A);
    CHECK(olc::parseIpv4("0.0.0.0", ip));
    CHECK(ip == 0);
    CHECK(olc::parseIpv4("255.255.255.255", ip));
    CHECK(ip == 0xFFFFFFFF);

    // Ошибки: ip остаётся прежним.
    ip = 0x12345678;
    const char* bad[] = {"256.1.1.1", "1.2.3",   "1.2.3.4.5", "a.b.c.d", "",
                         " 1.2.3.4",  "1..2.3",  "01234.1.1.1", "1.2.3.4 ", "1.2.3.",
                         ".1.2.3",    "-1.2.3.4"};
    for (const char* text : bad) {
        CHECK(!olc::parseIpv4(text, ip));
        CHECK(ip == 0x12345678);
    }

    olc::Endpoint endpoint;
    endpoint.ip = 0xC0A8010A;
    endpoint.port = 47800;
    CHECK(olc::endpointToString(endpoint) == "192.168.1.10:47800");
    CHECK(olc::endpointToString(olc::Endpoint()) == "0.0.0.0:0");

    CHECK(endpoint == endpoint);
    olc::Endpoint other = endpoint;
    other.port = 1;
    CHECK(!(endpoint == other));
}

// Принимает входящее подключение и ждёт, пока клиент станет Connected.
bool connectPair(olc::TcpListener& listener, olc::TcpConnection& client,
                 olc::TcpConnection& host) {
    if (!client.startConnect(loopbackEndpoint(listener.localPort()))) {
        return false;
    }
    bool accepted = waitFor([&]() { return listener.accept(host); });
    bool connected = waitFor(
        [&]() { return client.state() == olc::TcpConnection::State::Connected; });
    return accepted && connected && host.state() == olc::TcpConnection::State::Connected;
}

// Крутит receive, пока не придёт целое сообщение. Обе стороны обслуживаются в одном
// цикле: sender досылает очередь (flush), receiver читает.
bool receiveMessage(olc::TcpConnection& sender, olc::TcpConnection& receiver,
                    olc::TcpFrameReader& reader, uint8_t& type, std::vector<uint8_t>& body) {
    return waitFor([&]() {
        sender.flush();
        receiver.receive(reader);
        return reader.next(type, body) == olc::TcpFrameReader::Result::Ready;
    });
}

void testTcpMessages() {
    olc::TcpListener listener;
    CHECK(!listener.isOpen());
    CHECK(listener.localPort() == 0);
    CHECK(listener.open(0));
    CHECK(listener.isOpen());
    CHECK(listener.localPort() != 0);

    olc::TcpConnection client;
    olc::TcpConnection host;
    CHECK(connectPair(listener, client, host));
    CHECK(host.remote().ip == kLoopback);
    CHECK(client.remote().port == listener.localPort());

    olc::TcpFrameReader hostReader;
    olc::TcpFrameReader clientReader;
    uint8_t type = 0;
    std::vector<uint8_t> body;

    // HELLO от клиента к хосту.
    olc::Hello hello;
    hello.protocol = olc::kProtocolVersion;
    hello.orbiterVersion = 260101;
    hello.pluginVersion = "0.1";
    hello.player = "Alex";
    olc::Writer helloWriter;
    CHECK(olc::writeHello(helloWriter, hello));
    CHECK(client.send(
        olc::makeTcpFrame(static_cast<uint8_t>(olc::TcpType::Hello), helloWriter.bytes())));
    CHECK(receiveMessage(client, host, hostReader, type, body));
    CHECK(type == static_cast<uint8_t>(olc::TcpType::Hello));
    olc::Reader helloReader(body.data(), body.size());
    olc::Hello gotHello;
    CHECK(olc::readHello(helloReader, gotHello));
    CHECK(gotHello.protocol == hello.protocol);
    CHECK(gotHello.orbiterVersion == hello.orbiterVersion);
    CHECK(gotHello.pluginVersion == "0.1");
    CHECK(gotHello.player == "Alex");

    // WELCOME от хоста к клиенту.
    olc::Welcome welcome;
    welcome.clientId = 1;
    welcome.session = 0xABCDEF01;
    welcome.stateRateHz = 30;
    welcome.system = "Sol";
    olc::Writer welcomeWriter;
    CHECK(olc::writeWelcome(welcomeWriter, welcome));
    CHECK(host.send(
        olc::makeTcpFrame(static_cast<uint8_t>(olc::TcpType::Welcome), welcomeWriter.bytes())));
    CHECK(receiveMessage(host, client, clientReader, type, body));
    CHECK(type == static_cast<uint8_t>(olc::TcpType::Welcome));
    olc::Reader welcomeReader(body.data(), body.size());
    olc::Welcome gotWelcome;
    CHECK(olc::readWelcome(welcomeReader, gotWelcome));
    CHECK(gotWelcome.clientId == 1);
    CHECK(gotWelcome.session == 0xABCDEF01);
    CHECK(gotWelcome.stateRateHz == 30);
    CHECK(gotWelcome.system == "Sol");

    // Большой SCENARIO (~600 КБ): отправка уходит частями, остаток досылает flush.
    olc::Scenario scenario;
    scenario.name = "big.scn";
    scenario.data.resize(600 * 1000);
    for (size_t i = 0; i < scenario.data.size(); i++) {
        scenario.data[i] = static_cast<uint8_t>((i * 7 + i / 251) & 0xFF);
    }
    olc::Writer scenarioWriter;
    CHECK(olc::writeScenario(scenarioWriter, scenario));
    CHECK(host.send(
        olc::makeTcpFrame(static_cast<uint8_t>(olc::TcpType::Scenario), scenarioWriter.bytes())));
    CHECK(receiveMessage(host, client, clientReader, type, body));
    CHECK(host.pendingBytes() == 0);
    CHECK(type == static_cast<uint8_t>(olc::TcpType::Scenario));
    olc::Reader scenarioReader(body.data(), body.size());
    olc::Scenario gotScenario;
    CHECK(olc::readScenario(scenarioReader, gotScenario));
    CHECK(gotScenario.name == "big.scn");
    CHECK(gotScenario.data.size() == scenario.data.size());
    CHECK(gotScenario.data == scenario.data);  // побайтное сравнение

    // Клиент закрылся: хост узнаёт об этом при чтении.
    client.close();
    CHECK(client.state() == olc::TcpConnection::State::Closed);
    CHECK(!client.send(std::vector<uint8_t>(1, 0)));
    CHECK(waitFor([&]() { return !host.receive(hostReader); }));
    CHECK(host.state() == olc::TcpConnection::State::Closed);

    // Перемещение: сокет переходит к новому объекту, старый пуст.
    olc::TcpConnection client2;
    olc::TcpConnection host2;
    CHECK(connectPair(listener, client2, host2));
    olc::TcpConnection moved(std::move(client2));
    CHECK(client2.state() == olc::TcpConnection::State::Closed);
    CHECK(moved.state() == olc::TcpConnection::State::Connected);
    std::vector<olc::TcpConnection> list;
    list.push_back(std::move(host2));
    CHECK(list[0].state() == olc::TcpConnection::State::Connected);
}

void testConnectRefused() {
    // Порт, на котором точно никто не слушает: открыли слушатель и сразу закрыли.
    uint16_t port = 0;
    {
        olc::TcpListener listener;
        CHECK(listener.open(0));
        port = listener.localPort();
        listener.close();
    }

    olc::TcpConnection client;
    if (client.startConnect(loopbackEndpoint(port))) {
        CHECK(waitFor([&]() { return client.state() == olc::TcpConnection::State::Closed; }));
    }
    CHECK(client.state() == olc::TcpConnection::State::Closed);
}

void testUdp() {
    olc::UdpSocket a;
    olc::UdpSocket b;
    CHECK(!a.isOpen());
    CHECK(a.localPort() == 0);
    CHECK(a.open(0));
    CHECK(b.open(0));
    CHECK(a.isOpen());
    CHECK(a.localPort() != 0);
    CHECK(b.localPort() != 0);
    CHECK(a.localPort() != b.localPort());

    // На пустом сокете receiveFrom сразу возвращает false.
    std::vector<uint8_t> packet;
    olc::Endpoint from;
    CHECK(!b.receiveFrom(packet, from));

    // WORLD_STATE с UDP-заголовком от a к b.
    olc::UdpHeader header;
    header.type = static_cast<uint8_t>(olc::UdpType::WorldState);
    header.session = 77;
    header.seq = 5;
    olc::WorldState state;
    state.simTime = 12.5;
    state.mjd = 60000.25;
    state.warp = 2.0;
    olc::VesselState vessel;
    vessel.netId = 3;
    vessel.refBody = 1;
    vessel.rpos.x = 6.5e6;
    vessel.thrusters = {0, 128, 255};
    state.vessels.push_back(vessel);

    olc::Writer writer;
    olc::writeUdpHeader(writer, header);
    CHECK(olc::writeWorldState(writer, state));
    CHECK(a.sendTo(loopbackEndpoint(b.localPort()), writer.bytes()));

    CHECK(waitFor([&]() { return b.receiveFrom(packet, from); }));
    CHECK(from.port == a.localPort());
    CHECK(from.ip == kLoopback);
    CHECK(packet == writer.bytes());

    olc::Reader reader(packet.data(), packet.size());
    olc::UdpHeader gotHeader;
    CHECK(olc::readUdpHeader(reader, gotHeader) == olc::UdpHeaderResult::Ok);
    CHECK(gotHeader.type == header.type);
    CHECK(gotHeader.session == 77);
    CHECK(gotHeader.seq == 5);
    olc::WorldState gotState;
    CHECK(olc::readWorldState(reader, gotState));
    CHECK(gotState.simTime == 12.5);
    CHECK(gotState.vessels.size() == 1);
    CHECK(gotState.vessels[0].netId == 3);
    CHECK(gotState.vessels[0].rpos.x == 6.5e6);
    CHECK(gotState.vessels[0].thrusters == vessel.thrusters);

    // Пакет ровно предельного размера проходит, на байт больше - отказ.
    CHECK(a.sendTo(loopbackEndpoint(b.localPort()),
                   std::vector<uint8_t>(olc::kMaxUdpPacketSize, 1)));
    CHECK(waitFor([&]() { return b.receiveFrom(packet, from); }));
    CHECK(packet.size() == olc::kMaxUdpPacketSize);
    CHECK(!a.sendTo(loopbackEndpoint(b.localPort()),
                    std::vector<uint8_t>(olc::kMaxUdpPacketSize + 1, 1)));
    CHECK(!b.receiveFrom(packet, from));

    // Закрытый сокет ничего не принимает и не отправляет.
    a.close();
    CHECK(!a.isOpen());
    CHECK(!a.sendTo(loopbackEndpoint(b.localPort()), std::vector<uint8_t>(10, 1)));
    CHECK(!a.receiveFrom(packet, from));
}

}  // namespace

int main() {
    CHECK(olc::startNetwork());

    testParseIpv4();
    testTcpMessages();
    testConnectRefused();
    testUdp();

    olc::stopNetwork();
    return checkResult();
}
