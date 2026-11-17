// Starting the scenario received from the host (stage 2, docs/ORBITER-INTERNALS.md, section 18).
//
// The client writes the host's world to Scenarios\OLC\Host world.scn, selects it in the
// Launchpad scenario list and presses "Launch" itself - the same as the player would.
// Launchpad is an ordinary Win32 dialog of Orbiter, but its element numbers are not part of the
// SDK: they are taken from Src/Orbiter/resource.h of the base commit and must be checked when
// the base commit changes (ORBITER-INTERNALS.md, section 13).
#pragma once

#include <cstdint>
#include <vector>

#include "Orbitersdk.h"

namespace olcplugin {

// Writes the file. false - it could not be written (the reason is in Orbiter.log).
bool writeReceivedScenario(const std::vector<uint8_t>& data);

// Selects the written scenario in the Launchpad list and presses "Launch".
// launchpad - the Launchpad window (LaunchpadItem::clbkOpen gets it).
// false - the list was not found (the reason is in Orbiter.log).
bool launchReceivedScenario(HWND launchpad);

}  // namespace olcplugin
