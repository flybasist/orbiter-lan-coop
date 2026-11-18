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
// Called every frame in clbkPreStep, so the "one step" actions last while the key is held.
#pragma once

#include "Orbitersdk.h"

#include "messages.h"

namespace olcplugin {

// simdt - the length of the coming step, seconds of sim time (oapiGetSimStep).
void applyInput(VESSEL* vessel, const olc::Input& input, double simdt);

}  // namespace olcplugin
