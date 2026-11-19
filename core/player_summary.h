// Player summary text for the flight director's screen (task 5.3). Pure text formatting, no
// Orbiter calls, so it is covered by tests.
#pragma once

#include <string>
#include <vector>

#include "messages.h"
#include "session_table.h"

namespace olc {

// Builds a multi-line text from the PLAYERS list: one line per player with the role, the vessel
// name (looked up in the session table by net_id) and the round trip time to the host.
// Returns an empty string if the list is empty.
std::string formatPlayerSummary(const std::vector<PlayerEntry>& players, const SessionTable& table);

}  // namespace olc
