// The "client" role: shows the host's world.
//
// tick is called every frame (clbkPreStep) and by a timer while paused:
// - receive the host's messages (ClientLink::poll);
// - pause and time acceleration follow the host; for the flight director they go to the host instead
//   (TIME_REQUEST), and the director's vessel focus is view only;
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
// Docking follows the host (DOCK): Dock in mode 0 and Undock after the display delay; vessels that are
// docked are flown through the one with the smallest net_id.
// The last vessel is never deleted: Orbiter would end the session.
// The pilot's own vessel (decision 35): focusing a free vessel while having none asks the host
// for it; the vessel stays the pilot's until Ctrl+Alt+U or leaving. Someone else's vessel in
// focus is view only.
// Messages for the pilot (missing classes, ownership) are shown on screen for kNoticeSeconds.
#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "Orbitersdk.h"

#include "client_link.h"
#include "config.h"
#include "keymap.h"
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


    // The focus changed (F3): take the new vessel if the pilot has none and it is free.
    void onFocusChanged(OBJHANDLE vessel);

    // Ctrl+Alt+U: release the pilot's vessel.
    void releaseOwnVessel();

    // The keyboard of this frame (clbkProcessKeyboardImmediate): remembers the held control keys
    // for INPUT and clears them in kstate, so Orbiter does not fly the local copy - the host flies
    // it. Camera and other keys stay.
    void onKeyboard(char kstate[256]);

    // A single key press (clbkProcessKeyboardBuffered): while the pilot's own vessel is in focus,
    // the press goes to the host in KEY (decision 35: it is carried out on the client too, so
    // Orbiter is not told to skip it). The modifier keys themselves are not sent.
    void onKeyPressed(DWORD key, const char kstate[256]);

private:
    void followTime(double now, double displayTime);
    // The flight director's pause and time acceleration: a change made in this Orbiter goes to the
    // host (TIME_REQUEST); changes the host makes itself (its console) are followed.
    void directorTime(double now);
    // Sends INPUT for the pilot's vessel: keys only while it is in focus, plus the joystick.
    void sendInput(double now);
    // Repeats the host's docking and undocking: a DOCK is carried out after the display delay, so the
    // client vessels have reached the places they had when the host docked them.
    void applyDocks(double now);
    // Docked vessels are flown through one vessel of the bundle (the one with the smallest net_id):
    // net_id -> leader of its bundle, for vessels that are docked now.
    std::map<uint16_t, uint16_t> dockLeaders() const;
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
    // Shows text on screen for kNoticeSeconds (replaces the previous notice).
    void showNotice(const std::string& text);
    void updateNotice(double now);
    // The number of the pilot's own vessel; false - the pilot has none.
    bool ownVessel(uint16_t& netId) const;
    // Shows ownership news: a vessel taken or lost, refusals from the host.
    void followOwnership();
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
    std::vector<std::string> missingClasses_;  // already named in a notice
    std::string noticeText_;                // the notice to show; empty - nothing
    NOTEHANDLE notice_ = nullptr;           // on-screen text, nullptr - not shown
    double noticeShownAt_ = -1.0;           // < 0 - the notice is to be shown at the next tick
    bool focusChecked_ = false;             // the start focus was checked (onFocusChanged)
    bool hadOwnVessel_ = false;             // the pilot had a vessel at the last check
    uint16_t lastOwnVessel_ = 0;
    struct PendingDock {
        olc::Dock dock;
        double applyAt;  // real time
    };
    std::vector<PendingDock> pendingDocks_;  // DOCK messages waiting for their time
    std::vector<olc::Dock> dockedLinks_;     // links docked on this client now
    olc::Keymap keymap_;                    // Orbiter's key layout (keymap.cfg)
    uint32_t heldKeys_ = 0;                 // INPUT.keys from the last onKeyboard
    uint64_t keyFrames_ = 0;                // frames with a control key held (for the log)
    // Control latency (tasks 3.6 and 3.10): from pressing the full main engine key to thrust on the
    // displayed own vessel, with the steps of the path. Wall clock (wall_clock.h).
    // keyDownAt_ < 0 - no press is being timed; inputSentAt_, stateAt_ < 0 - the step is not reached yet.
    double keyDownAt_ = -1.0;
    double inputSentAt_ = -1.0;    // the first INPUT with the key went out
    double stateAt_ = -1.0;        // the first received state of the own vessel with thrust
    double longestFrame_ = 0.0;    // the longest client frame while timing, seconds
    double lastFrameAt_ = -1.0;    // wall clock of the previous tick
    bool ownThrusting_ = false;             // the displayed own vessel had thrust at the last frame
    uint64_t keyPresses_ = 0;               // KEY messages sent (for the log)

    Trace trace_;
    uint16_t traceNetId_ = 0;
    bool traceResolved_ = false;

    double requestedWarp_ = 0.0;   // the time acceleration the plugin itself set last
    double nextWarpCheck_ = 0.0;
    double lastPauseRequest_ = -1.0;
    // The flight director (directorTime): the pause and warp the director has now, the host's answer
    // is waited for until directorRequestAt_ + kDirectorAnswerWait.
    bool directorSynced_ = false;          // the first sync with the host values is done
    bool directorPaused_ = false;
    double directorWarp_ = 1.0;
    double directorRequestAt_ = -1.0e9;
    double directorSettleUntil_ = 0.0;     // oapiSetPause works from the next frame: no checks before
    double lastTimeJump_ = -1.0e9;
    int timeJumps_ = 0;

    // Statistics of sim time error against the host for the log interval, seconds.
    double timeErrorMax_ = 0.0;
    double timeErrorSum_ = 0.0;
    int timeErrorCount_ = 0;
    double nextStats_ = 0.0;
};

}  // namespace olcplugin
