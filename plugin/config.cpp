#include "config.h"

#include "Orbitersdk.h"

namespace olcplugin {

namespace {

// Файловые функции SDK принимают имя поля как char*, а не const char*: передаём копию.
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
    // FILE_IN_ZEROONFAIL: если файла нет, вернёт 0 (FILE_IN вернул бы пустой файл).
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
    readDouble(file, "DisplayDelay", 0.0, 2.0, config.displayDelay);
    readDouble(file, "TimeSyncThreshold", 0.01, 60.0, config.timeSyncThreshold);
    readString(file, "TraceVessel", config.traceVessel);
    readString(file, "TraceFile", config.traceFile);

    oapiCloseFile(file, FILE_IN_ZEROONFAIL);
    return config;
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
