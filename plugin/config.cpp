#include "config.h"

#include "Orbitersdk.h"

#include <fstream>

namespace olcplugin {

namespace {

// SDK file functions take the item name as char*, not const char*: we pass a copy.
bool readString(FILEHANDLE file, const char* item, std::string& value) {
    std::string name = item;
    char buffer[1024] = {};
    if (!oapiReadItem_string(file, &name[0], buffer)) {
        return false;
    }
    value = buffer;
    return true;
}

void readInt(FILEHANDLE file, const char* item, int low, int high, int& value) {
    std::string name = item;
    int parsed = 0;
    if (!oapiReadItem_int(file, &name[0], parsed)) {
        return;
    }
    if (parsed < low || parsed > high) {
        oapiWriteLogV("OLC: config %s = %d is out of range %d..%d, using %d", item, parsed, low,
                      high, value);
        return;
    }
    value = parsed;
}

void readDouble(FILEHANDLE file, const char* item, double low, double high, double& value) {
    std::string name = item;
    double parsed = 0.0;
    if (!oapiReadItem_float(file, &name[0], parsed)) {
        return;
    }
    if (parsed < low || parsed > high) {
        oapiWriteLogV("OLC: config %s = %g is out of range %g..%g, using %g", item, parsed, low,
                      high, value);
        return;
    }
    value = parsed;
}

}  // namespace

Config loadConfig() {
    Config config;
    // FILE_IN_ZEROONFAIL: if there is no file, returns 0 (FILE_IN would return an empty file).
    FILEHANDLE file = oapiOpenFile("OrbiterLanCoop.cfg", FILE_IN_ZEROONFAIL, CONFIG);
    if (!file) {
        oapiWriteLogV("OLC: Config\\OrbiterLanCoop.cfg not found, plugin is off");
        return config;
    }

    std::string role;
    if (readString(file, "Role", role)) {
        if (_stricmp(role.c_str(), "host") == 0) {
            config.role = Role::Host;
        } else if (_stricmp(role.c_str(), "client") == 0) {
            config.role = Role::Client;
        } else if (_stricmp(role.c_str(), "off") != 0) {
            oapiWriteLogV("OLC: config Role = %s is unknown (host, client, off), plugin is off",
                          role.c_str());
        }
    }
    readString(file, "HostAddress", config.hostAddress);
    readString(file, "Player", config.player);
    readInt(file, "Port", 1, 65535, config.port);
    readInt(file, "MaxPlayers", 2, 8, config.maxPlayers);
    readInt(file, "StateRate", 1, 60, config.stateRateHz);
    readString(file, "Director", config.director);
    readDouble(file, "DisplayDelay", 0.0, 2.0, config.displayDelay);
    readDouble(file, "TimeSyncThreshold", 0.01, 60.0, config.timeSyncThreshold);
    readString(file, "TraceVessel", config.traceVessel);
    readString(file, "TraceFile", config.traceFile);

    oapiCloseFile(file, FILE_IN_ZEROONFAIL);
    return config;
}

bool saveConfig(const Config& config) {
    // The Config folder is relative to the Orbiter folder, the current folder of the process.
    std::ofstream file("Config\\OrbiterLanCoop.cfg");
    if (!file) {
        oapiWriteLogV("OLC: cannot write Config\\OrbiterLanCoop.cfg");
        return false;
    }
    file << "; OrbiterLanCoop plugin settings. Written by the OLC item in Launchpad,\n"
         << "; can also be edited by hand (example - plugin/OrbiterLanCoop.cfg in the OLC sources).\n\n"
         << "; Role: host, client or off\n"
         << "Role = " << roleName(config.role) << "\n\n"
         << "; Client: host address (IPv4 digits only) and player name\n"
         << "HostAddress = " << config.hostAddress << "\n"
         << "Player = " << config.player << "\n\n"
         << "; TCP and UDP port, the same on host and client\n"
         << "Port = " << config.port << "\n\n"
         << "; Host: player limit (the host is a player too, 2...8) and state rate per second\n"
         << "MaxPlayers = " << config.maxPlayers << "\n"
         << "StateRate = " << config.stateRateHz << "\n\n"
         << "; Host: player name of the flight director (empty - none)\n"
         << "Director = " << config.director << "\n\n"
         << "; Client: display delay, seconds, and the sim time error threshold against the host\n"
         << "DisplayDelay = " << config.displayDelay << "\n"
         << "TimeSyncThreshold = " << config.timeSyncThreshold << "\n";
    if (!config.traceVessel.empty()) {
        file << "\n; For measurements: position of this vessel goes to a CSV file\n"
             << "TraceVessel = " << config.traceVessel << "\n"
             << "TraceFile = " << config.traceFile << "\n";
    }
    return static_cast<bool>(file);
}

const char* roleName(Role role) {
    switch (role) {
    case Role::Host:
        return "host";
    case Role::Client:
        return "client";
    default:
        return "off";
    }
}

}  // namespace olcplugin
