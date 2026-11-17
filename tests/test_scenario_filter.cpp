// Tests of filterScenario: vessels of missing classes are removed from the host's scenario.
#include <string>

#include "check.h"
#include "scenario_filter.h"

namespace {

const std::string kScenario =
    "BEGIN_ENVIRONMENT\r\n"
    "  System Sol\r\n"
    "END_ENVIRONMENT\r\n"
    "\r\n"
    "BEGIN_FOCUS\r\n"
    "  Ship Ghost\r\n"
    "END_FOCUS\r\n"
    "\r\n"
    "BEGIN_CAMERA\r\n"
    "  TARGET Ghost\r\n"
    "  MODE Extern\r\n"
    "END_CAMERA\r\n"
    "\r\n"
    "BEGIN_SHIPS\r\n"
    "Ghost:AddonShip\r\n"
    "  STATUS Orbiting Earth\r\n"
    "END\r\n"
    "GL-01:DeltaGlider\r\n"
    "  STATUS Landed Earth\r\n"
    "END\r\n"
    "Ghost-2:AddonShip\r\n"
    "  STATUS Orbiting Earth\r\n"
    "end\r\n"
    "ShuttleA\r\n"
    "  STATUS Orbiting Earth\r\n"
    "END\r\n"
    "END_SHIPS\r\n";

bool installed(const std::string& className) {
    return className == "DeltaGlider" || className == "ShuttleA";
}

void testRemovesMissingClasses() {
    olc::ScenarioFilterResult result = olc::filterScenario(kScenario, installed);
    CHECK(result.hasVessels);
    CHECK(result.removedVessels.size() == 2);
    if (result.removedVessels.size() == 2) {
        CHECK(result.removedVessels[0] == "Ghost" && result.removedVessels[1] == "Ghost-2");
    }
    // Each missing class once.
    CHECK(result.missingClasses.size() == 1);
    if (result.missingClasses.size() == 1) {
        CHECK(result.missingClasses[0] == "AddonShip");
    }
    // The removed blocks are gone, the others are kept as they are, with \r\n.
    CHECK(result.text.find("AddonShip") == std::string::npos);
    CHECK(result.text.find("GL-01:DeltaGlider\r\n  STATUS Landed Earth\r\nEND\r\n") !=
          std::string::npos);
    // "ShuttleA" without a colon: the class is the name.
    CHECK(result.text.find("ShuttleA\r\n  STATUS Orbiting Earth\r\nEND\r\nEND_SHIPS\r\n") !=
          std::string::npos);
    // Focus and camera moved to the first remaining vessel, the indent kept.
    CHECK(result.text.find("BEGIN_FOCUS\r\n  Ship GL-01\r\nEND_FOCUS") != std::string::npos);
    CHECK(result.text.find("  TARGET GL-01\r\n  MODE Extern") != std::string::npos);
}

void testNothingMissing() {
    olc::ScenarioFilterResult result =
        olc::filterScenario(kScenario, [](const std::string&) { return true; });
    CHECK(result.text == kScenario);
    CHECK(result.removedVessels.empty() && result.missingClasses.empty());
    CHECK(result.hasVessels);
}

void testNoVesselLeft() {
    olc::ScenarioFilterResult result =
        olc::filterScenario(kScenario, [](const std::string&) { return false; });
    CHECK(!result.hasVessels);
    CHECK(result.removedVessels.size() == 4);
    CHECK(result.text.find("BEGIN_SHIPS\r\nEND_SHIPS\r\n") != std::string::npos);
}

}  // namespace

int main() {
    testRemovesMissingClasses();
    testNothingMissing();
    testNoVesselLeft();
    return checkResult();
}
