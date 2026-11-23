// Plugin settings from the file Config\OrbiterLanCoop.cfg in the Orbiter folder.
//
// Settings come from a file: this way host and client also start without a UI, over SSH
// without graphics. The "OLC" item in Launchpad (launchpad_item.h) edits the main fields
// and writes the file back with saveConfig. Example file - plugin/OrbiterLanCoop.cfg.
#pragma once

#include <string>

#include "client_clock.h"
#include "protocol.h"
#include "session_table.h"

namespace olcplugin {

// Plugin version: goes into HELLO.
constexpr char kPluginVersion[] = "0.1";

enum class Role {
    Off,     // the plugin is loaded but does nothing
    Host,
    Client,
};

struct Config {
    Role role = Role::Off;
    std::string hostAddress = "127.0.0.1";  // client: host address
    int port = olc::kDefaultPort;           // host and client
    std::string player = "player";          // player name (the host's one goes into PLAYERS)
    int maxPlayers = olc::kDefaultMaxPlayers;  // host: player limit, the host is a player too
    int stateRateHz = 60;                   // host: WORLD_STATE rate (decision 36)
    // Host: name of the flight director (decision 46). A client with this player name may pause and
    // warp time for everybody and never takes a vessel. Empty - there is no director.
    std::string director;
    // Host: the world is saved to Scenarios\OLC\Saves\autosave.scn every this many minutes of real
    // time (task 5.4). 0 - no autosave. The director's key (Ctrl+Alt+S) saves a separate file.
    int autosaveMinutes = 10;
    double displayDelay = olc::kDefaultDisplayDelay;  // client: display delay, seconds (decision 36)
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
