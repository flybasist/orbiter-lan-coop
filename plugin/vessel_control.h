// Applying a pilot's INPUT to a vessel on the host (stage 3, task 3.3).
//
// Orbiter applies keyboard and joystick only to the vessel in focus. For a pilot's vessel the host
// does the same through the SDK, repeating what the core does for the focus vessel
// (Src/Orbiter/Orbiter.cpp, KbdInputImmediate_OnRunning; ARCHITECTURE.md, "How input reaches a
// vessel on the host"):
// - RCS: IncThrusterGroupLevel_SingleStep for one step, 1.0 or 0.1 (low power); the rotation
//   or translation bits are used by the vessel's RCS mode, as in the core;
// - full main or retro: also for one step; kill main and retro: levels to 0;
// - throttle and hover: the level changes by 0.2 per second while the key is held;
// - elevator trim: 0.25 per second; wheel brakes: for one step.
// - single key presses (KEY): VESSEL::SendBufferedKey with a key state that holds the modifiers of
//   the press (applyKey). The keys are looked up in the host's own keymap.cfg.
// - NAV and transponder frequencies (readNav, applyNav, task 4A.3): the owner's values are read
//   and put on the other copies of the vessel.
// Called every frame in clbkPreStep, so the "one step" actions last while the key is held.
#pragma once

#include "Orbitersdk.h"

#include "messages.h"

namespace olcplugin {

// simdt - the length of the coming step, seconds of sim time (oapiGetSimStep).
void applyInput(VESSEL* vessel, const olc::Input& input, double simdt);

// Sends one key press of the pilot to the vessel as if the key were pressed with the vessel in focus.
void applyKey(VESSEL* vessel, const olc::KeyPress& press);

// Reads the radio steps of the vessel: the transponder (0 if it has none) and every NAV receiver.
// netId is left as it is.
void readNav(VESSEL* vessel, olc::NavState& state);

// Sets the radio steps from state on this copy of the vessel. Only what differs is set; a
// receiver the vessel does not have and a transponder that is off are skipped.
void applyNav(VESSEL* vessel, const olc::NavState& state);

// Whether two NavState hold the same steps (net_id is not compared).
bool sameNav(const olc::NavState& a, const olc::NavState& b);

}  // namespace olcplugin
