// OlcRigTest - a helper plugin for checks on test rigs (docs/STANDS.md), not part of OLC.
//
// Creates and deletes vessels at given sim times, so that a host session can be checked
// without a person: OLC must send these changes to clients (VESSEL_ADDED, VESSEL_REMOVED).
// On a client it can switch the focus, as F3 would (the F3 dialog is drawn by ImGui and cannot be
// clicked by the rig scripts).
// A Lua scenario script would do the same, but in a session without graphics started over SSH
// Orbiter hangs when such a script creates and deletes vessels (checked 2026-11-17, without OLC
// too), while a plugin works.
//
// Actions are read from Config\OlcRigTest.cfg at session start, one per line:
//   <sim time, s> create <name> <class> <existing vessel>
//   <sim time, s> delete <name>
//   <sim time, s> dock <vessel> <its port> <other vessel> <its port>
//   <sim time, s> undock <vessel> <its port>
//   <sim time, s> focus <vessel>
// "dock" brings the other vessel to the first one and docks them (mode 1), so the vessels may start
// far apart; port numbers start from 0.
// A created vessel takes the state of the existing vessel, shifted 200 m along x so that the
// two do not collide. Empty lines and lines starting with ; are skipped.
// Load it only on rigs: Orbiter.exe ... -p OlcRigTest

#define ORBITER_MODULE
#include "Orbitersdk.h"

#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Offset of a created vessel from the vessel it is copied from, metres.
constexpr double kCreateOffset = 200.0;

struct Action {
    double simTime = 0.0;
    std::string command;  // create, delete, dock, undock or focus
    std::string name;
    std::string className;  // create only
    std::string nearVessel;  // create: vessel to copy; dock: the other vessel
    int port = 0;            // dock, undock: port of the vessel `name`
    int otherPort = 0;       // dock: port of the other vessel
    bool done = false;
};

class RigTestModule : public oapi::Module {
public:
    explicit RigTestModule(HINSTANCE hDLL) : oapi::Module(hDLL) {}

    void clbkSimulationStart(RenderMode mode) override {
        (void)mode;
        actions_.clear();
        std::ifstream file("Config\\OlcRigTest.cfg");
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == ';' || line[0] == '\r') {
                continue;
            }
            std::istringstream words(line);
            Action action;
            words >> action.simTime >> action.command >> action.name;
            if (action.command == "create") {
                words >> action.className >> action.nearVessel;
            }
            if (action.command == "dock") {
                words >> action.port >> action.nearVessel >> action.otherPort;
            } else if (action.command == "undock") {
                words >> action.port;
            }
            const bool known = action.command == "create" || action.command == "delete" ||
                               action.command == "dock" || action.command == "undock" ||
                               action.command == "focus";
            if (words.fail() || !known) {
                oapiWriteLogV("OLC rig test: bad line '%s'", line.c_str());
                continue;
            }
            actions_.push_back(action);
        }
        oapiWriteLogV("OLC rig test: %d action(s) loaded", static_cast<int>(actions_.size()));
    }

    void clbkPreStep(double simt, double simdt, double mjd) override {
        (void)simdt;
        (void)mjd;
        for (Action& action : actions_) {
            if (action.done || simt < action.simTime) {
                continue;
            }
            action.done = true;
            if (action.command == "create") {
                create(action);
            } else if (action.command == "delete") {
                remove(action);
            } else if (action.command == "dock") {
                dock(action);
            } else if (action.command == "undock") {
                undock(action);
            } else {
                focus(action);
            }
        }
    }

private:
    void create(const Action& action) {
        OBJHANDLE nearHandle = oapiGetVesselByName(const_cast<char*>(action.nearVessel.c_str()));
        if (!nearHandle) {
            oapiWriteLogV("OLC rig test: no vessel '%s' to copy", action.nearVessel.c_str());
            return;
        }
        VESSELSTATUS2 status;
        std::memset(&status, 0, sizeof(status));
        status.version = 2;
        oapiGetVesselInterface(nearHandle)->GetStatusEx(&status);
        status.rpos.x += kCreateOffset;
        // Without the class file Orbiter terminates; the rig scenario must use installed classes.
        if (oapiCreateVesselEx(action.name.c_str(), action.className.c_str(), &status)) {
            oapiWriteLogV("OLC rig test: created '%s'", action.name.c_str());
        }
    }

    void dock(const Action& action) {
        OBJHANDLE first = oapiGetVesselByName(const_cast<char*>(action.name.c_str()));
        OBJHANDLE second = oapiGetVesselByName(const_cast<char*>(action.nearVessel.c_str()));
        if (!first || !second) {
            oapiWriteLogV("OLC rig test: dock: no vessel '%s' or '%s'", action.name.c_str(),
                          action.nearVessel.c_str());
            return;
        }
        const int result = oapiGetVesselInterface(first)->Dock(second, action.port, action.otherPort, 1);
        oapiWriteLogV("OLC rig test: dock '%s' and '%s': code %d", action.name.c_str(),
                      action.nearVessel.c_str(), result);
    }

    void undock(const Action& action) {
        OBJHANDLE handle = oapiGetVesselByName(const_cast<char*>(action.name.c_str()));
        const bool done = handle && oapiGetVesselInterface(handle)->Undock(action.port);
        oapiWriteLogV("OLC rig test: undock '%s' port %d: %s", action.name.c_str(), action.port,
                      done ? "done" : "failed");
    }

    void focus(const Action& action) {
        OBJHANDLE handle = oapiGetVesselByName(const_cast<char*>(action.name.c_str()));
        const bool done = handle && oapiSetFocusObject(handle) != nullptr;
        oapiWriteLogV("OLC rig test: focus '%s': %s", action.name.c_str(), done ? "done" : "failed");
    }

    void remove(const Action& action) {
        OBJHANDLE handle = oapiGetVesselByName(const_cast<char*>(action.name.c_str()));
        if (handle && oapiDeleteVessel(handle)) {
            oapiWriteLogV("OLC rig test: deleted '%s'", action.name.c_str());
        } else {
            oapiWriteLogV("OLC rig test: cannot delete '%s'", action.name.c_str());
        }
    }

    std::vector<Action> actions_;
};

RigTestModule* g_module = nullptr;

}  // namespace

DLLCLBK void InitModule(HINSTANCE hDLL) {
    g_module = new RigTestModule(hDLL);
    oapiRegisterModule(g_module);
}

// The core does not delete a module registered with oapiRegisterModule, so it is deleted here.
DLLCLBK void ExitModule(HINSTANCE hDLL) {
    (void)hDLL;
    delete g_module;
    g_module = nullptr;
}
