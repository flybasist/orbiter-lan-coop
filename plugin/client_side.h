// The "client" role: shows the host's world.
//
// tick is called every frame (clbkPreStep) and by a timer while paused:
// - receive the host's messages (ClientLink::poll);
// - pause and time acceleration follow the host;
// - the client sim time is kept near display time: if the error exceeds
//   TimeSyncThreshold - a jump with oapiSetSimMJD (at most once per kTimeJumpPause seconds);
// - outside pause only: fuel from FUEL and vessel states at display time - via
//   DefSetStateEx in clbkPreStep (ORBITER-INTERNALS.md, section 3). A vessel that is
//   landed at display time is put there once, at the place from LANDED.
// Client vessels are matched with host vessels by name (docs/ARCHITECTURE.md, section 2).
// The client world follows the host's list of vessels:
// - a host vessel missing here is created (oapiCreateVesselEx) as soon as its state is known,
//   but only if its class is installed: without the class file Orbiter would terminate;
// - once, right after the first states are applied, client vessels the host does not have
//   are deleted;
// - VESSEL_REMOVED deletes the vessel here too.
// The last vessel is never deleted: Orbiter would end the session.
// Classes that are not installed are shown to the player on screen for kNoticeSeconds.
#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "Orbitersdk.h"

#include "client_link.h"
#include "config.h"
#include "trace.h"

namespace olcplugin {

class ClientSide {
public:
    // scenarioId goes into HELLO: the number of the host's scenario this session was started
    // from, or kOwnScenario if the player started a scenario by hand.
    ClientSide(const Config& config, uint32_t scenarioId);

    // Session start: connect to the host. false - failed right away (see the log).
    bool start();

    // Session end: BYE to the host. Vessels are already deleted - do not touch them.
    void stop();

    // now - oapiGetSysTime; paused - Orbiter is paused (call from the timer).
    void tick(double now, bool paused);

    void onDeleteVessel(OBJHANDLE vessel);

private:
    void followTime(double now, double displayTime);
    void applyFuel();
    void collectLanded();
    void putOnSurface(VESSEL* vessel, OBJHANDLE body, const olc::Landed& place);
    void applyStates(double displayTime);
    // Deletes client vessels the host does not have (once per session).
    void removeExtraVessels();
    // Deletes vessels the host deleted (VESSEL_REMOVED).
    void applyVesselChanges();
    // Creates the host vessel netId here with the given state. nullptr - not created now
    // (class not installed, or the vessel is landed and its place is not known yet).
    OBJHANDLE createVessel(uint16_t netId, const olc::VesselState& state, OBJHANDLE body);
    // Deletes a client vessel unless it is the last one. false - not deleted.
    bool deleteVessel(OBJHANDLE handle);
    // Adds a class to the on-screen notice about classes that are not installed.
    void noticeMissingClass(const std::string& className);
    void updateNotice(double now);
    // The client vessel for a host net_id: found by name and remembered. nullptr - no such vessel
    // (yet: it may be created later, so a miss is not remembered).
    OBJHANDLE vesselHandle(uint16_t netId);
    OBJHANDLE bodyHandle(uint16_t netId);
    void logStats();
    void flushLog();

    Config config_;
    uint32_t scenarioId_;
    olc::ClientLink link_;
    std::map<uint16_t, OBJHANDLE> vessels_;  // net_id → client vessel (nullptr - none)
    std::map<uint16_t, OBJHANDLE> bodies_;   // body net_id → client body
    std::map<uint16_t, olc::Landed> landedAt_;  // net_id → last landing place from LANDED
    bool extrasRemoved_ = false;            // removeExtraVessels has run
    std::set<uint16_t> cannotCreate_;       // net_id whose class is not installed (logged once)
    int deletesThisFrame_ = 0;              // oapiDeleteVessel calls not yet carried out
    std::vector<std::string> missingClasses_;  // shown in the notice
    NOTEHANDLE notice_ = nullptr;           // on-screen text, nullptr - not shown
    double noticeShownAt_ = -1.0;           // < 0 - the notice is to be shown at the next tick

    Trace trace_;
    uint16_t traceNetId_ = 0;
    bool traceResolved_ = false;

    double requestedWarp_ = 0.0;   // the time acceleration the plugin itself set last
    double nextWarpCheck_ = 0.0;
    double lastPauseRequest_ = -1.0;
    double lastTimeJump_ = -1.0e9;
    int timeJumps_ = 0;

    // Statistics of sim time error against the host for the log interval, seconds.
    double timeErrorMax_ = 0.0;
    double timeErrorSum_ = 0.0;
    int timeErrorCount_ = 0;
    double nextStats_ = 0.0;
};

}  // namespace olcplugin
