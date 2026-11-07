// Тесты сообщений: туда-обратно, точные байты, обрезанные и лишние байты.

#include <string>
#include <vector>

#include "check.h"
#include "header.h"
#include "messages.h"

namespace {

void testHello() {
    olc::Hello hello;
    hello.protocol = 1;
    hello.orbiterVersion = 241101;
    hello.pluginVersion = "0.0.1";
    hello.player = "Pilot";

    olc::Writer w;
    CHECK(olc::writeHello(w, hello));
    // 2 + 4 + (1 + 5) + (1 + 5) байтов.
    CHECK(w.bytes().size() == 18);

    olc::Reader r(w.bytes().data(), w.bytes().size());
    olc::Hello back;
    CHECK(olc::readHello(r, back));
    CHECK(back.protocol == 1 && back.orbiterVersion == 241101);
    CHECK(back.pluginVersion == "0.0.1" && back.player == "Pilot");

    // Слишком длинное имя игрока не упаковывается.
    hello.player = std::string(256, 'x');
    olc::Writer tooLong;
    CHECK(!olc::writeHello(tooLong, hello));
}

void testWelcomeThroughTcpFrame() {
    // Полный путь: упаковать, обернуть в TCP-кадр, достать из потока, разобрать.
    olc::Welcome welcome;
    welcome.clientId = 2;
    welcome.session = 0xDEADBEEF;
    welcome.stateRateHz = 30;
    welcome.system = "Sol";

    olc::Writer w;
    CHECK(olc::writeWelcome(w, welcome));
    std::vector<uint8_t> frame =
        olc::makeTcpFrame(static_cast<uint8_t>(olc::TcpType::Welcome), w.bytes());

    olc::TcpFrameReader frames;
    frames.append(frame.data(), frame.size());
    uint8_t type = 0;
    std::vector<uint8_t> body;
    CHECK(frames.next(type, body) == olc::TcpFrameReader::Result::Ready);
    CHECK(type == static_cast<uint8_t>(olc::TcpType::Welcome));

    olc::Reader r(body.data(), body.size());
    olc::Welcome back;
    CHECK(olc::readWelcome(r, back));
    CHECK(back.clientId == 2 && back.session == 0xDEADBEEF && back.stateRateHz == 30);
    CHECK(back.system == "Sol");
}

void testReject() {
    olc::Reject reject;
    reject.reason = static_cast<uint8_t>(olc::RejectReason::NoFreeSlots);
    reject.text = "Host is full";

    olc::Writer w;
    CHECK(olc::writeReject(w, reject));
    CHECK(w.bytes()[0] == 4);  // номер причины из PROTOCOL.md

    olc::Reader r(w.bytes().data(), w.bytes().size());
    olc::Reject back;
    CHECK(olc::readReject(r, back));
    CHECK(back.reason == 4 && back.text == "Host is full");
}

void testBrokenMessages() {
    olc::Welcome welcome;
    welcome.system = "Sol";
    olc::Writer w;
    olc::writeWelcome(w, welcome);
    const std::vector<uint8_t>& good = w.bytes();

    // Каждое обрезанное тело - битое.
    for (size_t size = 0; size < good.size(); size++) {
        olc::Reader r(good.data(), size);
        olc::Welcome back;
        CHECK(!olc::readWelcome(r, back));
    }

    // Лишний байт в конце - тоже битое.
    std::vector<uint8_t> extra = good;
    extra.push_back(0);
    olc::Reader r(extra.data(), extra.size());
    olc::Welcome back;
    CHECK(!olc::readWelcome(r, back));
}

void testLists() {
    std::vector<olc::BodyEntry> bodies = {{1, "Earth"}, {2, "Moon"}};
    std::vector<olc::VesselEntry> vessels = {{10, "GL-01", "DeltaGlider"}, {11, "ISS", "ProjectAlpha_ISS"}};
    std::vector<std::string> classes = {"XR2Ravenstar"};
    std::vector<olc::OwnerEntry> owners = {{10, 0}, {11, olc::kNoOwner}};

    olc::Writer wb, wv, wc, wo;
    CHECK(olc::writeBodies(wb, bodies));
    CHECK(olc::writeVessels(wv, vessels));
    CHECK(olc::writeClassesMissing(wc, classes));
    CHECK(olc::writeOwnership(wo, owners));

    std::vector<olc::BodyEntry> bodiesBack;
    olc::Reader rb(wb.bytes().data(), wb.bytes().size());
    CHECK(olc::readBodies(rb, bodiesBack) && bodiesBack.size() == 2);
    CHECK(bodiesBack[1].netId == 2 && bodiesBack[1].name == "Moon");

    std::vector<olc::VesselEntry> vesselsBack;
    olc::Reader rv(wv.bytes().data(), wv.bytes().size());
    CHECK(olc::readVessels(rv, vesselsBack) && vesselsBack.size() == 2);
    CHECK(vesselsBack[0].name == "GL-01" && vesselsBack[0].className == "DeltaGlider");

    std::vector<std::string> classesBack;
    olc::Reader rc(wc.bytes().data(), wc.bytes().size());
    CHECK(olc::readClassesMissing(rc, classesBack) && classesBack == classes);

    std::vector<olc::OwnerEntry> ownersBack;
    olc::Reader ro(wo.bytes().data(), wo.bytes().size());
    CHECK(olc::readOwnership(ro, ownersBack) && ownersBack.size() == 2);
    CHECK(ownersBack[1].netId == 11 && ownersBack[1].owner == olc::kNoOwner);

    // Пустой список - это 2 байта числа элементов.
    olc::Writer empty;
    CHECK(olc::writeClassesMissing(empty, {}));
    CHECK(empty.bytes().size() == 2);

    // Больше 65535 элементов в одно сообщение не помещается.
    olc::Writer tooMany;
    CHECK(!olc::writeOwnership(tooMany, std::vector<olc::OwnerEntry>(65536)));

    // Битый пакет обещает 65535 тел, а данных нет: разбор не удаётся и не виснет.
    const uint8_t liar[2] = {0xFF, 0xFF};
    olc::Reader rl(liar, sizeof(liar));
    CHECK(!olc::readBodies(rl, bodiesBack));

    // Каждое обрезанное тело списка - битое.
    for (size_t size = 0; size < wv.bytes().size(); size++) {
        olc::Reader r(wv.bytes().data(), size);
        CHECK(!olc::readVessels(r, vesselsBack));
    }
}

void testShortMessages() {
    olc::Writer w;
    olc::writeVesselRemoved(w, 77);
    olc::Reader r(w.bytes().data(), w.bytes().size());
    uint16_t netId = 0;
    CHECK(olc::readVesselRemoved(r, netId) && netId == 77);

    olc::Writer wu;
    olc::writeUdpReady(wu, 47801);
    olc::Reader ru(wu.bytes().data(), wu.bytes().size());
    uint16_t port = 0;
    CHECK(olc::readUdpReady(ru, port) && port == 47801);

    olc::KeyPress key;
    key.netId = 3;
    key.key = 0x1E;
    key.mods = 2;  // Ctrl
    olc::Writer wk;
    olc::writeKey(wk, key);
    olc::Reader rk(wk.bytes().data(), wk.bytes().size());
    olc::KeyPress keyBack;
    CHECK(olc::readKey(rk, keyBack));
    CHECK(keyBack.netId == 3 && keyBack.key == 0x1E && keyBack.mods == 2);

    olc::Dock dock;
    dock.a = 10;
    dock.portA = 0;
    dock.b = 11;
    dock.portB = 1;
    dock.docked = 1;
    olc::Writer wd;
    olc::writeDock(wd, dock);
    CHECK(wd.bytes().size() == 7);
    olc::Reader rd(wd.bytes().data(), wd.bytes().size());
    olc::Dock dockBack;
    CHECK(olc::readDock(rd, dockBack));
    CHECK(dockBack.a == 10 && dockBack.b == 11 && dockBack.portB == 1 && dockBack.docked == 1);

    olc::Landed landed;
    landed.netId = 10;
    landed.longitude = -1.4122;
    landed.latitude = 0.4974;
    landed.heading = 2.9;
    olc::Writer wl;
    olc::writeLanded(wl, landed);
    olc::Reader rl(wl.bytes().data(), wl.bytes().size());
    olc::Landed landedBack;
    CHECK(olc::readLanded(rl, landedBack));
    CHECK(landedBack.longitude == -1.4122 && landedBack.latitude == 0.4974 &&
          landedBack.heading == 2.9);

    olc::Writer wbye;
    CHECK(olc::writeBye(wbye, "Host closed the session"));
    olc::Reader rbye(wbye.bytes().data(), wbye.bytes().size());
    std::string reason;
    CHECK(olc::readBye(rbye, reason) && reason == "Host closed the session");

    // Лишний байт после короткого сообщения - битое.
    olc::Writer extra;
    olc::writeUdpReady(extra, 1);
    extra.writeU8(0);
    olc::Reader re(extra.bytes().data(), extra.bytes().size());
    CHECK(!olc::readUdpReady(re, port));
}

}  // namespace

int main() {
    testLists();
    testShortMessages();
    testHello();
    testWelcomeThroughTcpFrame();
    testReject();
    testBrokenMessages();
    return checkResult();
}
