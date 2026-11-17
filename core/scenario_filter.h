// Preparing the host's scenario for a client that may lack some vessel classes.
//
// Orbiter terminates abnormally if a scenario has a vessel whose class file is missing
// (Vessel.cpp, TerminateOnError). So before starting the host's world the client removes such
// vessels from the text. They are skipped later too: the client reports their classes in
// CLASSES_MISSING and does not create them (docs/ORBITER-INTERNALS.md, section 18).
//
// The scenario format (Psys.cpp, InitState): between BEGIN_SHIPS and END_SHIPS every vessel
// starts with a line "name:class" (or just "name" - then the class is the name) and ends with
// a line "END". If the focus vessel (BEGIN_FOCUS, "Ship name") is removed, the focus and the
// camera target (BEGIN_CAMERA, "TARGET name") move to the first remaining vessel.
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace olc {

struct ScenarioFilterResult {
    std::string text;                       // the scenario without the removed vessels
    std::vector<std::string> removedVessels;
    std::vector<std::string> missingClasses;  // each class once
    bool hasVessels = false;                // at least one vessel remains
};

// classInstalled tells whether the client has the class. Line endings are kept as they are.
ScenarioFilterResult filterScenario(const std::string& text,
                                    const std::function<bool(const std::string&)>& classInstalled);

}  // namespace olc
