// olc-netcheck: проверка сети между двумя машинами без Orbiter.
//
// Использует то же сетевое ядро, что и плагин (olc-core): сокеты, заголовки и сообщения
// протокола. Поэтому если olc-netcheck работает между двумя машинами, то и плагину сеть
// не помешает: адреса, порты, брандмауэр и NAT проверены.
//
//   olc-netcheck server [port]
//   olc-netcheck client <server-ip> [port] [seconds] [rate-hz]
//
// Сервер (как хост OLC) слушает TCP и UDP на одном порту (по умолчанию 47800) и работает,
// пока его не остановят (Ctrl+C). Клиент:
//   1. подключается по TCP и проходит рукопожатие HELLO → WELCOME;
//   2. шлёт UDP PING с частотой rate-hz (по умолчанию 30, как WORLD_STATE) seconds секунд
//      (по умолчанию 10); сервер отвечает PONG с тем же временем;
//   3. прощается сообщением BYE и печатает отчёт: потери, задержку туда-обратно
//      (минимум, среднее, перцентили, максимум), разброс задержки, пакеты не по порядку,
//      и долю ответов позже 100 и 200 мс - с этим сравнивается задержка буфера
//      сглаживания (docs/SMOOTHING.md).
// Клиент завершается с кодом 0, если пришёл хотя бы один PONG, иначе с кодом 1.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "header.h"
#include "messages.h"
#include "protocol.h"
#include "serialize.h"
#include "socket.h"

namespace {

// Номер сессии, который сервер выдаёт в WELCOME. Сервер один, сессия тоже одна.
constexpr uint32_t kSession = 1;

// Сколько ждать подключения и ответа на HELLO, секунд.
constexpr double kHandshakeTimeout = 5.0;

// Сколько после последнего PING ждать опоздавшие PONG, секунд.
constexpr double kLateWait = 1.0;

// Секунды от запуска программы.
double now() {
    static const auto start = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    return elapsed.count();
}

void pause1ms() {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

// Собирает UDP-пакет PING или PONG: заголовок и время.
std::vector<uint8_t> makePingPacket(olc::UdpType type, uint32_t seq, double time) {
    olc::Writer writer;
    olc::UdpHeader header;
    header.type = static_cast<uint8_t>(type);
    header.session = kSession;
    header.seq = seq;
    olc::writeUdpHeader(writer, header);
    olc::writePingTime(writer, time);
    return writer.bytes();
}

// Разбирает PING или PONG нужного типа. false - чужой или битый пакет.
bool readPingPacket(const std::vector<uint8_t>& packet, olc::UdpType type, double& time) {
    olc::Reader reader(packet.data(), packet.size());
    olc::UdpHeader header;
    if (olc::readUdpHeader(reader, header) != olc::UdpHeaderResult::Ok) {
        return false;
    }
    if (header.type != static_cast<uint8_t>(type) || header.session != kSession) {
        return false;
    }
    return olc::readPingTime(reader, time);
}

// ---------- Сервер ----------

// Подключённый по TCP клиент.
struct ServerClient {
    uint8_t id = 0;      // номер клиента для WELCOME
    olc::TcpConnection connection;
    olc::TcpFrameReader frames;
    std::string player;  // имя из HELLO; пусто, пока HELLO не пришёл
};

// Разбирает принятые от клиента TCP-сообщения. false - соединение надо закрыть.
bool handleClientFrames(ServerClient& client) {
    uint8_t type = 0;
    std::vector<uint8_t> body;
    olc::TcpFrameReader::Result result;
    while ((result = client.frames.next(type, body)) == olc::TcpFrameReader::Result::Ready) {
        olc::Reader reader(body.data(), body.size());
        if (type == static_cast<uint8_t>(olc::TcpType::Hello)) {
            olc::Hello hello;
            if (!olc::readHello(reader, hello)) {
                std::printf("client %s: broken HELLO\n",
                            olc::endpointToString(client.connection.remote()).c_str());
                return false;
            }
            client.player = hello.player;
            olc::Welcome welcome;
            welcome.clientId = client.id;
            welcome.session = kSession;
            welcome.stateRateHz = 30;
            welcome.system = "netcheck";
            olc::Writer writer;
            olc::writeWelcome(writer, welcome);
            client.connection.send(
                olc::makeTcpFrame(static_cast<uint8_t>(olc::TcpType::Welcome), writer.bytes()));
            std::printf("client %s: HELLO from '%s' (protocol %u), WELCOME sent\n",
                        olc::endpointToString(client.connection.remote()).c_str(),
                        hello.player.c_str(), static_cast<unsigned>(hello.protocol));
        } else if (type == static_cast<uint8_t>(olc::TcpType::Bye)) {
            std::string reason;
            olc::readBye(reader, reason);
            std::printf("client %s: BYE (%s)\n",
                        olc::endpointToString(client.connection.remote()).c_str(),
                        reason.c_str());
            return false;
        }
        // Остальные сообщения olc-netcheck не нужны - пропускаем.
    }
    return result != olc::TcpFrameReader::Result::Broken;
}

int runServer(uint16_t port) {
    olc::TcpListener listener;
    olc::UdpSocket udp;
    if (!listener.open(port) || !udp.open(port)) {
        std::printf("error: cannot open TCP and UDP port %u (busy or not allowed)\n",
                    static_cast<unsigned>(port));
        return 1;
    }
    std::printf("server: listening on TCP and UDP port %u, press Ctrl+C to stop\n",
                static_cast<unsigned>(port));

    std::vector<ServerClient> clients;
    uint8_t nextClientId = 1;
    uint32_t pongSeq = 0;
    uint64_t pongsSent = 0;
    double nextReport = now() + 10.0;

    while (true) {
        // Новые подключения.
        ServerClient incoming;
        while (listener.accept(incoming.connection)) {
            std::printf("client %s: connected\n",
                        olc::endpointToString(incoming.connection.remote()).c_str());
            incoming.id = nextClientId;
            nextClientId = nextClientId == 254 ? 1 : nextClientId + 1;
            clients.push_back(std::move(incoming));
            incoming = ServerClient();
        }

        // TCP-сообщения клиентов. Закрытые соединения удаляются из списка.
        for (size_t i = 0; i < clients.size();) {
            ServerClient& client = clients[i];
            bool alive = client.connection.receive(client.frames);
            alive = handleClientFrames(client) && alive;
            alive = client.connection.flush() && alive;
            if (alive) {
                i++;
            } else {
                std::printf("client %s: disconnected\n",
                            olc::endpointToString(client.connection.remote()).c_str());
                clients.erase(clients.begin() + static_cast<std::ptrdiff_t>(i));
            }
        }
        // UDP: на каждый PING - PONG с тем же временем тому, кто прислал.
        std::vector<uint8_t> packet;
        olc::Endpoint from;
        while (udp.receiveFrom(packet, from)) {
            double time = 0.0;
            if (readPingPacket(packet, olc::UdpType::Ping, time)) {
                udp.sendTo(from, makePingPacket(olc::UdpType::Pong, pongSeq++, time));
                pongsSent++;
            }
        }

        if (now() >= nextReport) {
            std::printf("server: %zu TCP clients, %llu PONG sent\n", clients.size(),
                        static_cast<unsigned long long>(pongsSent));
            nextReport += 10.0;
        }
        pause1ms();
    }
}

// ---------- Клиент ----------

// Ждёт подключения и WELCOME. Возвращает false и печатает причину, если не дождался.
bool handshake(olc::TcpConnection& tcp, const olc::Endpoint& server) {
    if (!tcp.startConnect(server)) {
        std::printf("error: cannot start TCP connection to %s\n",
                    olc::endpointToString(server).c_str());
        return false;
    }

    olc::Hello hello;
    hello.protocol = olc::kProtocolVersion;
    hello.pluginVersion = "netcheck";
    hello.player = "netcheck";
    olc::Writer writer;
    olc::writeHello(writer, hello);
    // Пока идёт подключение, кадр ждёт в очереди и уйдёт сразу после него.
    tcp.send(olc::makeTcpFrame(static_cast<uint8_t>(olc::TcpType::Hello), writer.bytes()));

    const double start = now();
    olc::TcpFrameReader frames;
    while (now() - start < kHandshakeTimeout) {
        if (tcp.state() == olc::TcpConnection::State::Closed) {
            std::printf("error: TCP connection to %s refused or lost "
                        "(server not running, wrong address or firewall)\n",
                        olc::endpointToString(server).c_str());
            return false;
        }
        tcp.flush();
        tcp.receive(frames);
        uint8_t type = 0;
        std::vector<uint8_t> body;
        if (frames.next(type, body) == olc::TcpFrameReader::Result::Ready) {
            olc::Reader reader(body.data(), body.size());
            olc::Welcome welcome;
            if (type != static_cast<uint8_t>(olc::TcpType::Welcome) ||
                !olc::readWelcome(reader, welcome)) {
                std::printf("error: expected WELCOME, got message type %u\n",
                            static_cast<unsigned>(type));
                return false;
            }
            std::printf("TCP: connected, HELLO -> WELCOME in %.1f ms\n",
                        (now() - start) * 1000.0);
            return true;
        }
        pause1ms();
    }
    std::printf("error: no WELCOME from %s within %.0f s\n",
                olc::endpointToString(server).c_str(), kHandshakeTimeout);
    return false;
}

// Итоги обмена PING и PONG.
struct PingStats {
    uint64_t sent = 0;
    uint64_t received = 0;
    uint64_t outOfOrder = 0;  // PONG пришёл раньше ответа на более ранний PING
    std::vector<double> rtts;  // задержки туда-обратно всех PONG, секунды
    double jitterSum = 0.0;   // сумма |разница соседних задержек|
    double lastRtt = -1.0;
    double lastPingTime = -1.0;

    void addPong(double pingTime, double rtt) {
        rtts.push_back(rtt);
        if (lastRtt >= 0.0) {
            jitterSum += std::fabs(rtt - lastRtt);
        }
        lastRtt = rtt;
        if (pingTime < lastPingTime) {
            outOfOrder++;
        } else {
            lastPingTime = pingTime;
        }
        received++;
    }
};

// Значение, которого не превышает доля part задержек (0.95 - перцентиль 95).
// sorted - задержки по возрастанию, не пустой.
double percentile(const std::vector<double>& sorted, double part) {
    const size_t index = static_cast<size_t>(part * static_cast<double>(sorted.size() - 1));
    return sorted[index];
}

// Доля задержек больше limit, в процентах.
double percentAbove(const std::vector<double>& rtts, double limit) {
    const auto count = std::count_if(rtts.begin(), rtts.end(),
                                     [limit](double rtt) { return rtt > limit; });
    return 100.0 * static_cast<double>(count) / static_cast<double>(rtts.size());
}

void printReport(const PingStats& stats) {
    const uint64_t lost = stats.sent > stats.received ? stats.sent - stats.received : 0;
    const double lostPercent =
        stats.sent > 0 ? 100.0 * static_cast<double>(lost) / static_cast<double>(stats.sent) : 0.0;
    std::printf("UDP: sent %llu PING, received %llu PONG, lost %llu (%.2f%%), "
                "out of order %llu\n",
                static_cast<unsigned long long>(stats.sent),
                static_cast<unsigned long long>(stats.received),
                static_cast<unsigned long long>(lost), lostPercent,
                static_cast<unsigned long long>(stats.outOfOrder));
    if (stats.received == 0) {
        return;
    }
    std::vector<double> sorted = stats.rtts;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (double rtt : sorted) {
        sum += rtt;
    }
    const double jitter =
        stats.received > 1 ? stats.jitterSum / static_cast<double>(stats.received - 1) : 0.0;
    std::printf("RTT ms: min %.2f, avg %.2f, p50 %.2f, p95 %.2f, p99 %.2f, max %.2f, "
                "jitter %.2f\n",
                sorted.front() * 1000.0, sum / static_cast<double>(sorted.size()) * 1000.0,
                percentile(sorted, 0.50) * 1000.0, percentile(sorted, 0.95) * 1000.0,
                percentile(sorted, 0.99) * 1000.0, sorted.back() * 1000.0, jitter * 1000.0);
    std::printf("RTT above 100 ms: %.2f%%, above 200 ms: %.2f%%\n",
                percentAbove(stats.rtts, 0.100), percentAbove(stats.rtts, 0.200));
}

int runClient(const olc::Endpoint& server, double seconds, double rateHz) {
    olc::TcpConnection tcp;
    if (!handshake(tcp, server)) {
        return 1;
    }

    olc::UdpSocket udp;
    if (!udp.open(0)) {
        std::printf("error: cannot open UDP socket\n");
        return 1;
    }
    std::printf("UDP: %.0f PING per second for %.0f s from local port %u\n", rateHz, seconds,
                static_cast<unsigned>(udp.localPort()));

    PingStats stats;
    uint32_t seq = 0;
    const double start = now();
    const double stop = start + seconds;
    double nextPing = start;
    std::vector<uint8_t> packet;
    olc::Endpoint from;
    while (now() < stop + kLateWait) {
        if (now() < stop && now() >= nextPing) {
            udp.sendTo(server, makePingPacket(olc::UdpType::Ping, seq++, now()));
            stats.sent++;
            nextPing += 1.0 / rateHz;
        }
        while (udp.receiveFrom(packet, from)) {
            double pingTime = 0.0;
            if (from == server && readPingPacket(packet, olc::UdpType::Pong, pingTime)) {
                stats.addPong(pingTime, now() - pingTime);
            }
        }
        // TCP держим живым: обрыв во время замера - тоже результат.
        if (!tcp.flush() || tcp.state() == olc::TcpConnection::State::Closed) {
            std::printf("error: TCP connection lost during the test\n");
            break;
        }
        pause1ms();
    }

    olc::Writer writer;
    olc::writeBye(writer, "netcheck done");
    tcp.send(olc::makeTcpFrame(static_cast<uint8_t>(olc::TcpType::Bye), writer.bytes()));
    tcp.flush();

    printReport(stats);
    return stats.received > 0 ? 0 : 1;
}

// ---------- Разбор командной строки ----------

void printUsage() {
    std::printf("usage:\n"
                "  olc-netcheck server [port]\n"
                "  olc-netcheck client <server-ip> [port] [seconds] [rate-hz]\n"
                "defaults: port %u, 10 seconds, 30 Hz\n",
                static_cast<unsigned>(olc::kDefaultPort));
}

// Читает целое число в пределах [low, high]. false - не число или вне пределов.
bool parseNumber(const char* text, long low, long high, long& value) {
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < low || parsed > high) {
        return false;
    }
    value = parsed;
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 2;
    }
    const std::string mode = argv[1];
    long port = olc::kDefaultPort;
    long seconds = 10;
    long rateHz = 30;
    olc::Endpoint server;

    bool argsOk = false;
    if (mode == "server" && argc <= 3) {
        argsOk = argc < 3 || parseNumber(argv[2], 1, 65535, port);
    } else if (mode == "client" && argc >= 3 && argc <= 6) {
        argsOk = olc::parseIpv4(argv[2], server.ip) &&
                 (argc < 4 || parseNumber(argv[3], 1, 65535, port)) &&
                 (argc < 5 || parseNumber(argv[4], 1, 3600, seconds)) &&
                 (argc < 6 || parseNumber(argv[5], 1, 1000, rateHz));
    }
    if (!argsOk) {
        printUsage();
        return 2;
    }

    // Без буфера: вывод сервера сразу виден, даже если он идёт в файл.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    if (!olc::startNetwork()) {
        std::printf("error: cannot start network\n");
        return 1;
    }
    int result = 0;
    if (mode == "server") {
        result = runServer(static_cast<uint16_t>(port));
    } else {
        server.port = static_cast<uint16_t>(port);
        result = runClient(server, static_cast<double>(seconds), static_cast<double>(rateHz));
    }
    olc::stopNetwork();
    return result;
}
