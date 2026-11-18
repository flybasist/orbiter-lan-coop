// The "host" role: captures vessel states and sends them to clients.
//
// tick is called every frame (clbkPreStep) and by a timer while paused:
// - accept client connections and messages (HostLink::poll);
// - at StateRate capture the states of all vessels (GetStatusEx) and send
//   WORLD_STATE; while paused - at most kPausedStateRate times per second;
// - send FUEL once per second;
// - a vessel came to rest on a surface - LANDED at once; landed vessels are repeated in LANDED
//   every kLandedRepeat seconds, so that clients that connect later learn about them too.
// Vessels and bodies are taken at session start. A vessel created during the session gets a
// number at the next state capture and goes to clients in VESSEL_ADDED together with its
// state; a deleted vessel goes to clients in VESSEL_REMOVED at once.
// Vessels start free. A host with graphics flies the vessel in its focus, by the same rules as the
// pilots (decision 35): a free vessel is taken by focusing it, Ctrl+Alt+U releases it. A server
// without graphics flies nothing.
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
    // headless - Orbiter runs without graphics (a server): the host then flies no vessel.
    HostSide(const Config& config, bool headless);

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

    // The host-player's focus changed: it takes the new vessel if it has none and the vessel is free.
    void onFocusChanged(OBJHANDLE vessel);

    // Ctrl+Alt+U on the host: release the host's vessel so a pilot can take it.
    void releaseHostVessel();

private:
    // A host vessel: number in the table and handle in Orbiter.
    struct Ship {
        uint16_t netId = 0;
        OBJHANDLE handle = nullptr;  // a deleted vessel is removed from the list at once
        bool landed = false;         // was landed at the last capture
        bool announce = false;       // created during the session: VESSEL_ADDED not sent yet
        uint32_t keys = 0;           // INPUT.keys of the last applied pilot's INPUT
        // Latency probe (task 3.10): the full main engine key of a pilot's INPUT was applied at
        // probeAt (wall clock) and no state with thrust has gone out yet. < 0 - no probe.
        double probeAt = -1.0;
        double probeLongestFrame = 0.0;  // the longest host frame since probeAt, seconds
    };

    void buildBodies();
    void buildShips();
    // The number of the vessel the host flies; kNoVessel - none.
    uint16_t hostVessel() const;
    // Takes this vessel for the host if the host has none and the vessel is free.
    void takeHostVessel(OBJHANDLE vessel);
    // Gives numbers to vessels created since the last capture (onNewVessel).
    void addNewShips();
    // Body number by handle; false - the body is not in the list.
    bool bodyId(OBJHANDLE body, uint16_t& id) const;
    void sendWorld(bool paused);
    // Applies the pilots' latest INPUT to their vessels (every frame outside pause).
    void applyInputs(double now);
    // Finds vessels that docked or undocked since the last call and sends DOCK. Called every frame
    // outside pause; a link is named from the vessel with the smaller net_id.
    void detectDocking();
    // Passes the pilots' key presses (KEY) to their vessels.
    void applyKeys();
    void sendFuel();
    void repeatLanded();
    void logStats();
    void flushLog();

    // hostVessel: "no vessel". net_id are handed out from 0, 65535 is never reached in practice.
    static constexpr uint16_t kNoVessel = 0xFFFF;

    Config config_;
    bool headless_;
    olc::SessionTable table_;
    olc::HostLink link_;
    std::vector<OBJHANDLE> bodies_;  // index - body net_id
    std::vector<Ship> ships_;
    std::vector<olc::Dock> docked_;  // links docked at the last detectDocking
    std::vector<OBJHANDLE> newVessels_;  // created during the session, no number yet
    Trace trace_;
    OBJHANDLE traceHandle_ = nullptr;

    double nextState_ = 0.0;
    double nextFuel_ = 0.0;
    double nextLanded_ = 0.0;
    double nextStats_ = 0.0;
    uint64_t worldStatesSent_ = 0;
    uint64_t inputFrames_ = 0;     // frames in which a pilot's INPUT was applied (for the log)
    uint64_t inputKeyFrames_ = 0;  // of them with a control key held
    uint64_t keysApplied_ = 0;     // KEY presses passed to vessels (for the log)
    double lastFrameAt_ = -1.0;    // wall clock of the previous tick
    double longestFrame_ = 0.0;    // the longest frame of the log interval, seconds (for the log)
};

}  // namespace olcplugin
