// Message tests: round trip, exact bytes, truncated and extra bytes.

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
    // 2 + 4 + (1 + 5) + (1 + 5) bytes.
    CHECK(w.bytes().size() == 18);

    olc::Reader r(w.bytes().data(), w.bytes().size());
    olc::Hello back;
    CHECK(olc::readHello(r, back));
    CHECK(back.protocol == 1 && back.orbiterVersion == 241101);
    CHECK(back.pluginVersion == "0.0.1" && back.player == "Pilot");

    // A too long player name is not packed.
    hello.player = std::string(256, 'x');
    olc::Writer tooLong;
    CHECK(!olc::writeHello(tooLong, hello));
}

void testWelcomeThroughTcpFrame() {
    // The full path: pack, wrap into a TCP frame, take from the stream, parse.
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
    CHECK(w.bytes()[0] == 4);  // reason number from PROTOCOL.md

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

    // Every truncated body is broken.
    for (size_t size = 0; size < good.size(); size++) {
        olc::Reader r(good.data(), size);
        olc::Welcome back;
        CHECK(!olc::readWelcome(r, back));
    }

    // An extra byte at the end is broken too.
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

    // An empty list is the 2 bytes of the item count.
    olc::Writer empty;
    CHECK(olc::writeClassesMissing(empty, {}));
    CHECK(empty.bytes().size() == 2);

    // More than 65535 items do not fit into one message.
    olc::Writer tooMany;
    CHECK(!olc::writeOwnership(tooMany, std::vector<olc::OwnerEntry>(65536)));

    // A broken packet promises 65535 bodies but has no data: parsing fails and does not hang.
    const uint8_t liar[2] = {0xFF, 0xFF};
    olc::Reader rl(liar, sizeof(liar));
    CHECK(!olc::readBodies(rl, bodiesBack));

    // Every truncated list body is broken.
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

    // An extra byte after a short message is broken.
    olc::Writer extra;
    olc::writeUdpReady(extra, 1);
    extra.writeU8(0);
    olc::Reader re(extra.bytes().data(), extra.bytes().size());
    CHECK(!olc::readUdpReady(re, port));
}

// A WORLD_STATE packet with two vessels: one is flying, the other is landed.
olc::WorldState makeWorldState() {
    olc::VesselState flying;
    flying.netId = 7;
    flying.status = static_cast<uint8_t>(olc::VesselStatus::Flying);
    flying.refBody = 3;
    flying.rpos = {6.771e6, -12.5, 0.25};
    flying.rvel = {0.0, 7672.4, -1.0};
    flying.arot = {0.1, -0.2, 3.1};
    flying.vrot = {0.0, 0.01, 0.0};
    flying.thrusters = {255, 0, 128};

    olc::VesselState landed;
    landed.netId = 8;
    landed.status = static_cast<uint8_t>(olc::VesselStatus::Landed);
    landed.refBody = 4;
    landed.rpos = {1.0, 2.0, 3.0};  // not sent: a landed vessel's position comes from LANDED

    olc::WorldState world;
    world.simTime = 1234.5;
    world.mjd = 51983.5;
    world.warp = 10.0;
    world.paused = 0;
    world.vessels = {flying, landed};
    return world;
}

void testWorldState() {
    olc::WorldState world = makeWorldState();

    // The full path over UDP: header, body, header check, body parsing.
    olc::Writer w;
    olc::UdpHeader header;
    header.type = static_cast<uint8_t>(olc::UdpType::WorldState);
    header.session = 42;
    header.seq = 1000;
    olc::writeUdpHeader(w, header);
    CHECK(olc::writeWorldState(w, world));
    // Header 15; common part 8*3 + 1 + 1 = 26; flying 2 + 1 + 2 + 4*24 + 1 + 3 = 105;
    // landed 2 + 1 + 2 + 1 = 6.
    CHECK(w.bytes().size() == 15 + 26 + 105 + 6);

    olc::Reader r(w.bytes().data(), w.bytes().size());
    olc::UdpHeader headerBack;
    CHECK(olc::readUdpHeader(r, headerBack) == olc::UdpHeaderResult::Ok);
    CHECK(headerBack.type == static_cast<uint8_t>(olc::UdpType::WorldState));

    olc::WorldState back;
    back.vessels.resize(5);  // the old contents must be replaced
    CHECK(olc::readWorldState(r, back));
    CHECK(back.simTime == 1234.5 && back.mjd == 51983.5 && back.warp == 10.0);
    CHECK(back.paused == 0);
    CHECK(back.vessels.size() == 2);
    if (back.vessels.size() == 2) {
        const olc::VesselState& a = back.vessels[0];
        CHECK(a.netId == 7 && a.refBody == 3);
        CHECK(a.rpos.x == 6.771e6 && a.rpos.y == -12.5 && a.rpos.z == 0.25);
        CHECK(a.rvel.y == 7672.4 && a.arot.z == 3.1 && a.vrot.y == 0.01);
        CHECK(a.thrusters == std::vector<uint8_t>({255, 0, 128}));

        const olc::VesselState& b = back.vessels[1];
        CHECK(b.netId == 8 && b.refBody == 4);
        CHECK(b.status == static_cast<uint8_t>(olc::VesselStatus::Landed));
        CHECK(b.rpos.x == 0.0 && b.rpos.y == 0.0 && b.rpos.z == 0.0);
        CHECK(b.thrusters.empty());
    }

    // An empty packet is allowed too: for example, a pause without vessels.
    olc::WorldState empty;
    empty.paused = 1;
    olc::Writer we;
    CHECK(olc::writeWorldState(we, empty));
    olc::Reader rempty(we.bytes().data(), we.bytes().size());
    olc::WorldState emptyBack;
    CHECK(olc::readWorldState(rempty, emptyBack));
    CHECK(emptyBack.paused == 1 && emptyBack.vessels.empty());

    // Limits: 256 thrusters or 256 vessels are not packed.
    olc::WorldState tooManyThrusters = makeWorldState();
    tooManyThrusters.vessels[0].thrusters.assign(256, 1);
    olc::Writer wt;
    CHECK(!olc::writeWorldState(wt, tooManyThrusters));

    olc::WorldState tooManyVessels;
    tooManyVessels.vessels.resize(256);
    olc::Writer wv;
    CHECK(!olc::writeWorldState(wv, tooManyVessels));
}

void testBrokenWorldState() {
    olc::Writer w;
    CHECK(olc::writeWorldState(w, makeWorldState()));
    const std::vector<uint8_t>& good = w.bytes();

    // A truncated packet of any length is broken.
    bool anyTruncatedAccepted = false;
    for (size_t size = 0; size < good.size(); size++) {
        olc::Reader r(good.data(), size);
        olc::WorldState back;
        if (olc::readWorldState(r, back)) {
            anyTruncatedAccepted = true;
        }
    }
    CHECK(!anyTruncatedAccepted);

    // An extra byte at the end is broken.
    std::vector<uint8_t> extra = good;
    extra.push_back(0);
    olc::Reader re(extra.data(), extra.size());
    olc::WorldState back;
    CHECK(!olc::readWorldState(re, back));

    // An unknown status is broken: unclear which fields follow.
    // The first vessel's status comes after the common part (26 bytes) and net_id (2 bytes).
    std::vector<uint8_t> badStatus = good;
    badStatus[26 + 2] = 2;
    olc::Reader rs(badStatus.data(), badStatus.size());
    CHECK(!olc::readWorldState(rs, back));

    // count larger than the vessels in the packet is broken.
    std::vector<uint8_t> badCount = good;
    badCount[25] = 3;
    olc::Reader rc(badCount.data(), badCount.size());
    CHECK(!olc::readWorldState(rc, back));
}

void testVesselAdded() {
    olc::VesselAdded added;
    added.name = "GL-02";
    added.className = "DeltaGlider";
    added.state = makeWorldState().vessels[0];  // a flying vessel with net_id 7

    olc::Writer w;
    CHECK(olc::writeVesselAdded(w, added));
    // (1 + 5) + (1 + 11) + the flying vessel state 105.
    CHECK(w.bytes().size() == 6 + 12 + 105);

    olc::Reader r(w.bytes().data(), w.bytes().size());
    olc::VesselAdded back;
    CHECK(olc::readVesselAdded(r, back));
    CHECK(back.name == "GL-02" && back.className == "DeltaGlider");
    CHECK(back.state.netId == 7 && back.state.rpos.x == 6.771e6);
    CHECK(back.state.thrusters == std::vector<uint8_t>({255, 0, 128}));

    // Truncated and with an extra byte - broken.
    const std::vector<uint8_t>& good = w.bytes();
    bool anyTruncatedAccepted = false;
    for (size_t size = 0; size < good.size(); size++) {
        olc::Reader rt(good.data(), size);
        if (olc::readVesselAdded(rt, back)) {
            anyTruncatedAccepted = true;
        }
    }
    CHECK(!anyTruncatedAccepted);
    std::vector<uint8_t> extra = good;
    extra.push_back(0);
    olc::Reader re(extra.data(), extra.size());
    CHECK(!olc::readVesselAdded(re, back));

    // A too long class is not packed.
    added.className = std::string(256, 'x');
    olc::Writer tooLong;
    CHECK(!olc::writeVesselAdded(tooLong, added));
}

void testInputAndPing() {
    olc::Input input;
    input.netId = 7;
    input.keys = (1u << 13) | (1u << 0);  // full main thrust and RCS pitch +
    input.rot[0] = -1.0f;
    input.rot[2] = 0.5f;
    input.lin[1] = 0.25f;
    input.throttle = 0.75f;

    // The full path through the UDP header.
    olc::Writer w;
    olc::UdpHeader header;
    header.type = static_cast<uint8_t>(olc::UdpType::Input);
    header.session = 42;
    header.seq = 5;
    olc::writeUdpHeader(w, header);
    olc::writeInput(w, input);
    // Header 15; body 2 + 4 + 4 * 7 = 34.
    CHECK(w.bytes().size() == 15 + 34);

    olc::Reader r(w.bytes().data(), w.bytes().size());
    olc::UdpHeader headerBack;
    CHECK(olc::readUdpHeader(r, headerBack) == olc::UdpHeaderResult::Ok);
    CHECK(headerBack.type == static_cast<uint8_t>(olc::UdpType::Input));
    olc::Input back;
    CHECK(olc::readInput(r, back));
    CHECK(back.netId == 7 && back.keys == input.keys);
    CHECK(back.rot[0] == -1.0f && back.rot[1] == 0.0f && back.rot[2] == 0.5f);
    CHECK(back.lin[1] == 0.25f && back.throttle == 0.75f);

    // By default there is no throttle lever.
    CHECK(olc::Input().throttle == olc::kNoThrottle);

    // A truncated INPUT is broken.
    olc::Writer body;
    olc::writeInput(body, input);
    olc::Reader rt(body.bytes().data(), body.bytes().size() - 1);
    CHECK(!olc::readInput(rt, back));

    // PING and PONG: the same 9-byte body - time and client number.
    olc::Ping ping;
    ping.time = 12.625;
    ping.clientId = 3;
    olc::Writer wp;
    olc::writePing(wp, ping);
    CHECK(wp.bytes().size() == 9);
    olc::Reader rp(wp.bytes().data(), wp.bytes().size());
    olc::Ping pingBack;
    CHECK(olc::readPing(rp, pingBack) && pingBack.time == 12.625 && pingBack.clientId == 3);

    std::vector<uint8_t> extra = wp.bytes();
    extra.push_back(0);
    olc::Reader rpe(extra.data(), extra.size());
    CHECK(!olc::readPing(rpe, pingBack));

    olc::Reader rps(wp.bytes().data(), wp.bytes().size() - 1);  // without the client number
    CHECK(!olc::readPing(rps, pingBack));
}

void testScenario() {
    olc::Scenario scenario;
    scenario.name = "OLC session";
    scenario.data = {'B', 'E', 'G', 'I', 'N', '\n'};

    // The full path through a TCP frame.
    olc::Writer w;
    CHECK(olc::writeScenario(w, scenario));
    // (1 + 11) + 4 + 6 bytes.
    CHECK(w.bytes().size() == 12 + 4 + 6);
    std::vector<uint8_t> frame =
        olc::makeTcpFrame(static_cast<uint8_t>(olc::TcpType::Scenario), w.bytes());
    olc::TcpFrameReader frames;
    frames.append(frame.data(), frame.size());
    uint8_t type = 0;
    std::vector<uint8_t> body;
    CHECK(frames.next(type, body) == olc::TcpFrameReader::Result::Ready);
    CHECK(type == static_cast<uint8_t>(olc::TcpType::Scenario));

    olc::Reader r(body.data(), body.size());
    olc::Scenario back;
    CHECK(olc::readScenario(r, back));
    CHECK(back.name == "OLC session" && back.data == scenario.data);

    // An empty file is allowed.
    olc::Scenario empty;
    empty.name = "x";
    olc::Writer we;
    CHECK(olc::writeScenario(we, empty));
    olc::Reader rempty(we.bytes().data(), we.bytes().size());
    CHECK(olc::readScenario(rempty, back) && back.data.empty());

    // size does not match the remaining bytes - broken. The size field comes after
    // the name (12 bytes).
    std::vector<uint8_t> bigger = w.bytes();
    bigger[12] = 7;
    olc::Reader rb(bigger.data(), bigger.size());
    CHECK(!olc::readScenario(rb, back));
    std::vector<uint8_t> smaller = w.bytes();
    smaller[12] = 5;
    olc::Reader rs(smaller.data(), smaller.size());
    CHECK(!olc::readScenario(rs, back));

    // Truncated - broken.
    bool anyTruncatedAccepted = false;
    for (size_t size = 0; size < w.bytes().size(); size++) {
        olc::Reader rt(w.bytes().data(), size);
        if (olc::readScenario(rt, back)) {
            anyTruncatedAccepted = true;
        }
    }
    CHECK(!anyTruncatedAccepted);

    // The largest scenario with the longest name fits into a TCP frame,
    // and one byte more is no longer packed.
    olc::Scenario largest;
    largest.name = std::string(olc::kMaxStringLength, 'n');
    largest.data.assign(olc::kMaxScenarioSize, 'a');
    olc::Writer wl;
    CHECK(olc::writeScenario(wl, largest));
    CHECK(1 + wl.bytes().size() == olc::kMaxTcpMessageSize);

    largest.data.push_back('a');
    olc::Writer wtoo;
    CHECK(!olc::writeScenario(wtoo, largest));
}

void testFuel() {
    olc::Fuel fuel;
    olc::VesselFuel a;
    a.netId = 7;
    a.emptyMass = 11000.0;
    a.tankLevels = {1.0f, 0.25f, 0.0f};
    olc::VesselFuel b;  // a vessel without tanks
    b.netId = 8;
    b.emptyMass = 500.0;
    fuel.vessels = {a, b};

    olc::Writer w;
    CHECK(olc::writeFuel(w, fuel));
    // count 1; vessel: 2 + 8 + 1 + 4 * tanks.
    CHECK(w.bytes().size() == 1 + (11 + 12) + 11);

    olc::Reader r(w.bytes().data(), w.bytes().size());
    olc::Fuel back;
    back.vessels.resize(5);  // the old contents must be replaced
    CHECK(olc::readFuel(r, back));
    CHECK(back.vessels.size() == 2);
    if (back.vessels.size() == 2) {
        CHECK(back.vessels[0].netId == 7 && back.vessels[0].emptyMass == 11000.0);
        CHECK(back.vessels[0].tankLevels == std::vector<float>({1.0f, 0.25f, 0.0f}));
        CHECK(back.vessels[1].netId == 8 && back.vessels[1].tankLevels.empty());
    }

    // A truncated packet of any length and an extra byte at the end are broken.
    const std::vector<uint8_t>& good = w.bytes();
    bool anyTruncatedAccepted = false;
    for (size_t size = 0; size < good.size(); size++) {
        olc::Reader rt(good.data(), size);
        olc::Fuel broken;
        if (olc::readFuel(rt, broken)) {
            anyTruncatedAccepted = true;
        }
    }
    CHECK(!anyTruncatedAccepted);
    std::vector<uint8_t> extra = good;
    extra.push_back(0);
    olc::Reader re(extra.data(), extra.size());
    CHECK(!olc::readFuel(re, back));

    // Limits: 256 tanks or 256 vessels are not packed.
    olc::Fuel tooManyTanks = fuel;
    tooManyTanks.vessels[0].tankLevels.assign(256, 0.5f);
    olc::Writer wt;
    CHECK(!olc::writeFuel(wt, tooManyTanks));
    olc::Fuel tooManyVessels;
    tooManyVessels.vessels.resize(256);
    olc::Writer wv;
    CHECK(!olc::writeFuel(wv, tooManyVessels));
}
}  // namespace

int main() {
    testLists();
    testShortMessages();
    testHello();
    testWelcomeThroughTcpFrame();
    testReject();
    testBrokenMessages();
    testWorldState();
    testBrokenWorldState();
    testVesselAdded();
    testInputAndPing();
    testScenario();
    testFuel();
    return checkResult();
}
