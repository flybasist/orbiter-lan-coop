// Helpers and constants shared by the host side (host_side.h) and the client side (client_side.h).
#pragma once

#include <string>

#include "Orbitersdk.h"

namespace olcplugin {

// NAV_STATE (decision 58): how often a side checks the frequencies of its own vessel, seconds.
constexpr double kNavScan = 0.5;

// How often to write statistics to the log, seconds.
constexpr double kStatsInterval = 10.0;

// Name of an Orbiter object (vessel, planet) by its handle.
inline std::string objectName(OBJHANDLE object) {
    char name[256] = {};
    oapiGetObjectName(object, name, sizeof(name));
    return name;
}

}  // namespace olcplugin
