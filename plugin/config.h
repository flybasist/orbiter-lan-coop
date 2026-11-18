// Plugin settings from the file Config\OrbiterLanCoop.cfg in the Orbiter folder.
//
// Settings come from a file: this way host and client also start without a UI, over SSH
// without graphics. The "OLC" item in Launchpad (launchpad_item.h) edits the main fields
// and writes the file back with saveConfig. Example file - plugin/OrbiterLanCoop.cfg.
#pragma once

#include <string>

namespace olcplugin {

// Plugin version: goes into HELLO and is shown in the host's log.
constexpr char kPluginVersion[] = "0.1-stage3";

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
    int stateRateHz = 60;                   // host: WORLD_STATE rate (decision 36)
    double displayDelay = 0.03;             // client: display delay, seconds (decision 36)
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

// Writes the settings to Config\OrbiterLanCoop.cfg, TraceVessel and TraceFile only if set
// (comments in the old file are replaced by the standard ones). false - the file could not be written.
bool saveConfig(const Config& config);

const char* roleName(Role role);

}  // namespace olcplugin
