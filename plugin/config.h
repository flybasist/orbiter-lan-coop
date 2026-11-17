// Plugin settings from the file Config\OrbiterLanCoop.cfg in the Orbiter folder.
//
// At stage 1 settings come from a file: this way host and client start without a UI,
// including over SSH without graphics. A Launchpad window will come later
// (docs/ARCHITECTURE.md, section "Interface"). Example file - plugin/OrbiterLanCoop.cfg.
#pragma once

#include <string>

namespace olcplugin {

enum class Role {
    Off,     // the plugin is loaded but does nothing
    Host,
    Client,
};

struct Config {
    Role role = Role::Off;
    std::string hostAddress = "127.0.0.1";  // client: host address
    int port = 47800;                       // host and client
    std::string player = "player";          // client: player name
    int maxPlayers = 4;                     // host: player limit, the host is a player too
    int stateRateHz = 30;                   // host: WORLD_STATE rate
    double displayDelay = 0.1;              // client: display delay, seconds
    // Client: sim time error against the host (seconds × warp) after which
    // the client time catches up with a jump.
    double timeSyncThreshold = 0.1;
    // If not empty - write this vessel's position to the trace file (CSV) every frame to
    // compare host and client after a run.
    std::string traceVessel;
    std::string traceFile = "olc-trace.csv";
};

// Reads Config\OrbiterLanCoop.cfg. No file - Role::Off. Errors in values
// are written to Orbiter.log, and such a field keeps its default value.
Config loadConfig();

const char* roleName(Role role);

}  // namespace olcplugin
