// The "host" role: captures vessel states and sends them to clients.
//
// tick is called every frame (clbkPreStep) and by a timer while paused:
// - accept client connections and messages (HostLink::poll);
// - at StateRate capture the states of all vessels (GetStatusEx) and send
//   WORLD_STATE; while paused - at most kPausedStateRate times per second;
// - send FUEL once per second;
// - a vessel came to rest on a surface - LANDED at once; landed vessels are repeated in LANDED
//   every kLandedRepeat seconds, so that clients that connect later learn about them too.
// Vessels and bodies are taken once at session start (stage 1). Vessels that appear during
// the session are not sent at stage 1 - only a log line (VESSEL_ADDED is stage 2).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Orbitersdk.h"

#include "config.h"
#include "host_link.h"
#include "session_table.h"
#include "trace.h"

namespace olcplugin {

class HostSide {
public:
    explicit HostSide(const Config& config);

    // Session start: vessel and body tables, network start. false - did not start (see the log).
    bool start();

    // Session end: BYE to clients, close the network. Vessels are already deleted - do not touch them.
    void stop();

    // now - oapiGetSysTime; paused - Orbiter is paused (call from the timer).
    void tick(double now, bool paused);

    // A vessel was deleted: do not touch its handle any more.
    void onDeleteVessel(OBJHANDLE vessel);

    // A vessel was created during the session.
    void onNewVessel(OBJHANDLE vessel);

private:
    // A host vessel: number in the table and handle in Orbiter.
    struct Ship {
        uint16_t netId = 0;
        OBJHANDLE handle = nullptr;  // nullptr - the vessel is deleted
        bool landed = false;         // was landed at the last capture
    };

    void buildBodies();
    void buildShips();
    // Body number by handle; false - the body is not in the list.
    bool bodyId(OBJHANDLE body, uint16_t& id) const;
    void sendWorld(bool paused);
    void sendFuel();
    void repeatLanded();
    void logStats();
    void flushLog();

    Config config_;
    olc::SessionTable table_;
    olc::HostLink link_;
    std::vector<OBJHANDLE> bodies_;  // index - body net_id
    std::vector<Ship> ships_;
    Trace trace_;
    OBJHANDLE traceHandle_ = nullptr;

    double nextState_ = 0.0;
    double nextFuel_ = 0.0;
    double nextLanded_ = 0.0;
    double nextStats_ = 0.0;
    uint64_t worldStatesSent_ = 0;
};

}  // namespace olcplugin
