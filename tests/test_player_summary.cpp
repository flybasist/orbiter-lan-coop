// Player summary text tests: roles, vessel names, latency and the empty list.

#include <string>
#include <vector>

#include "check.h"
#include "player_summary.h"

namespace {

olc::PlayerEntry makePlayer(uint8_t id, const std::string& name, uint8_t flags, uint16_t netId,
                            uint16_t rttMs) {
    olc::PlayerEntry entry;
    entry.clientId = id;
    entry.player = name;
    entry.flags = flags;
    entry.netId = netId;
    entry.rttMs = rttMs;
    return entry;
}

void testEmpty() {
    olc::SessionTable table;
    CHECK(olc::formatPlayerSummary({}, table).empty());
}

void testSummary() {
    olc::SessionTable table;
    uint16_t netId = 0;
    CHECK(table.addVessel("GL-01", "DeltaGlider", netId));

    std::vector<olc::PlayerEntry> players;
    players.push_back(makePlayer(0, "Host", 0, netId, 0));
    players.push_back(makePlayer(1, "Anna", olc::kPlayerDirector, olc::kNoVessel, 12));
    players.push_back(makePlayer(2, "Boris", olc::kPlayerObserver, olc::kNoVessel, olc::kRttUnknown));
    players.push_back(makePlayer(3, "Ghost", 0, 4242, 40));  // vessel not in the table

    const std::string text = olc::formatPlayerSummary(players, table);
    const std::string expected =
        "OLC players: 4\n"
        "Host - GL-01 - 0 ms\n"
        "Anna [flight director] - no vessel - 12 ms\n"
        "Boris [observer] - no vessel - ? ms\n"
        "Ghost - no vessel - 40 ms";
    CHECK(text == expected);
}

}  // namespace

int main() {
    testEmpty();
    testSummary();
    return checkResult();
}
