#include "player_summary.h"

namespace olc {

std::string formatPlayerSummary(const std::vector<PlayerEntry>& players, const SessionTable& table) {
    if (players.empty()) {
        return std::string();
    }
    std::string text = "OLC players: " + std::to_string(players.size());
    for (const PlayerEntry& entry : players) {
        text += "\n" + entry.player;
        if (entry.flags & kPlayerDirector) {
            text += " [flight director]";
        } else if (entry.flags & kPlayerObserver) {
            text += " [observer]";
        }
        text += " - ";
        const VesselRecord* vessel = entry.netId == kNoVessel ? nullptr : table.findById(entry.netId);
        text += vessel ? vessel->name : "no vessel";
        text += " - ";
        // The host measures no round trip to itself (PLAYERS reports 0 for it).
        text += entry.rttMs == kRttUnknown ? "? ms" : std::to_string(entry.rttMs) + " ms";
    }
    return text;
}

}  // namespace olc
