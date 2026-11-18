// OrbiterLanCoop - a multiplayer plugin for Orbiter (stage 2: the host sends the world, the client
// gets it with one button in Launchpad and shows it; docs/PLAN.md).
//
// The role (host, client or off) is set by the file Config\OrbiterLanCoop.cfg (config.h).
// The role's work is in HostSide and ClientSide; this file only connects them to Orbiter:
// - clbkSimulationStart - start the network and the role. Not InitModule: InitModule is called from
//   DllMain, where Winsock must not be started (docs/ORBITER-INTERNALS.md, section 14);
// - clbkPreStep - work every frame (capture or impose states);
// - clbkPreStep is not called while paused, so the plugin has a hidden Windows window with
//   a timer: its messages always arrive, and while paused the role works by the timer
//   (decision 23 in docs/DECISIONS.md);
// - clbkSimulationEnd - stop;
// - clbkProcessKeyboardBuffered - single key presses of the client go to the host (KEY);
// - clbkFocusChanged and Ctrl+Alt+U (clbkProcessKeyboardBuffered) - take and release the
//   player's own vessel (decision 35).
// The "OLC" item in Launchpad (LaunchpadPanel) lives as long as the DLL: it is registered in
// InitModule and removed in ExitModule.

#define ORBITER_MODULE
#include "Orbitersdk.h"

#include <memory>

#include "client_side.h"
#include "config.h"
#include "host_side.h"
#include "launchpad_item.h"
#include "protocol.h"
#include "socket.h"

namespace {

// Hidden window timer: how often to work while paused, milliseconds.
constexpr UINT kTimerIntervalMs = 50;
constexpr UINT_PTR kTimerId = 1;
const char kWindowClass[] = "OrbiterLanCoopTimer";

class LanCoopModule;
LanCoopModule* g_module = nullptr;
olcplugin::LaunchpadPanel* g_panel = nullptr;

LRESULT CALLBACK timerWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

// The plugin module: Orbiter creates sessions, and this object lives between them.
class LanCoopModule : public oapi::Module {
public:
    LanCoopModule(HINSTANCE hDLL, olcplugin::LaunchpadPanel* panel)
        : oapi::Module(hDLL), instance_(hDLL), panel_(panel) {}

    void clbkSimulationStart(RenderMode mode) override {
        // Log lines are in English (project rules).
        oapiWriteLogV("OLC: session started (protocol v%u, render mode %d)",
                      static_cast<unsigned>(olc::kProtocolVersion), static_cast<int>(mode));
        config_ = olcplugin::loadConfig();
        if (config_.role == olcplugin::Role::Off) {
            return;
        }
        if (!olc::startNetwork()) {
            oapiWriteLogV("OLC: cannot start network, plugin is off");
            return;
        }
        networkStarted_ = true;

        bool started = false;
        if (config_.role == olcplugin::Role::Host) {
            host_.reset(new olcplugin::HostSide(config_, mode == RENDER_NONE));
            started = host_->start();
        } else {
            client_.reset(new olcplugin::ClientSide(config_, panel_->takeLaunchedScenarioId()));
            started = client_->start();
        }
        if (!started) {
            oapiWriteLogV("OLC: %s did not start, plugin is off", olcplugin::roleName(config_.role));
            stopRole();
            return;
        }
        oapiWriteLogV("OLC: running as %s", olcplugin::roleName(config_.role));
        createTimerWindow();
    }

    void clbkSimulationEnd() override {
        // The world is already deleted here: do not touch vessels (ORBITER-INTERNALS.md, section 8).
        destroyTimerWindow();
        stopRole();
        oapiWriteLogV("OLC: session ended");
    }

    void clbkPreStep(double simt, double simdt, double mjd) override {
        (void)simt;
        (void)simdt;
        (void)mjd;
        tick(false);
    }

    void clbkDeleteVessel(OBJHANDLE vessel) override {
        if (host_) {
            host_->onDeleteVessel(vessel);
        }
        if (client_) {
            client_->onDeleteVessel(vessel);
        }
    }

    void clbkNewVessel(OBJHANDLE vessel) override {
        if (host_) {
            host_->onNewVessel(vessel);
        }
    }

    void clbkFocusChanged(OBJHANDLE newFocus, OBJHANDLE oldFocus) override {
        (void)oldFocus;
        if (host_) {
            host_->onFocusChanged(newFocus);
        }
        if (client_) {
            client_->onFocusChanged(newFocus);
        }
    }

    // Held keys: the client takes the vessel control keys for INPUT and clears them, so Orbiter does
    // not apply them locally. Returns false: Orbiter still handles the rest (camera, dialogs).
    bool clbkProcessKeyboardImmediate(char kstate[256], bool simRunning) override {
        if (client_ && simRunning) {
            client_->onKeyboard(kstate);
        }
        return false;
    }

    // Ctrl+Alt+U releases the player's vessel (decision 35). Other keys go on to Orbiter.
    bool clbkProcessKeyboardBuffered(DWORD key, char kstate[256], bool simRunning) override {
        (void)simRunning;
        if (key != OAPI_KEY_U || !KEYMOD_CONTROL(kstate) || !KEYMOD_ALT(kstate)) {
            if (client_) {
                client_->onKeyPressed(key, kstate);  // Orbiter handles the key here as well
            }
            return false;
        }
        if (host_) {
            host_->releaseHostVessel();
        }
        if (client_) {
            client_->releaseOwnVessel();
        }
        return host_ || client_;
    }

    // Hidden window timer. Outside pause clbkPreStep does the work.
    void onTimer() {
        if (oapiGetPause()) {
            tick(true);
        }
    }

private:
    void tick(bool paused) {
        const double now = oapiGetSysTime();
        if (host_) {
            host_->tick(now, paused);
        }
        if (client_) {
            client_->tick(now, paused);
        }
    }

    void stopRole() {
        if (host_) {
            host_->stop();
            host_.reset();
        }
        if (client_) {
            client_->stop();
            client_.reset();
        }
        if (networkStarted_) {
            olc::stopNetwork();
            networkStarted_ = false;
        }
    }

    // A message-only hidden window (HWND_MESSAGE): it is not on screen.
    void createTimerWindow() {
        WNDCLASSA windowClass = {};
        windowClass.lpfnWndProc = timerWindowProc;
        windowClass.hInstance = instance_;
        windowClass.lpszClassName = kWindowClass;
        RegisterClassA(&windowClass);
        window_ = CreateWindowExA(0, kWindowClass, "", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                  instance_, nullptr);
        if (!window_ || !SetTimer(window_, kTimerId, kTimerIntervalMs, nullptr)) {
            oapiWriteLogV("OLC: cannot create timer window, pause will not be synced");
        }
    }

    void destroyTimerWindow() {
        if (window_) {
            KillTimer(window_, kTimerId);
            DestroyWindow(window_);
            window_ = nullptr;
        }
        UnregisterClassA(kWindowClass, instance_);
    }

    HINSTANCE instance_;
    olcplugin::LaunchpadPanel* panel_;  // owned by InitModule / ExitModule
    olcplugin::Config config_;
    bool networkStarted_ = false;
    std::unique_ptr<olcplugin::HostSide> host_;
    std::unique_ptr<olcplugin::ClientSide> client_;
    HWND window_ = nullptr;
};

LRESULT CALLBACK timerWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_TIMER && wParam == kTimerId && g_module) {
        g_module->onTimer();
        return 0;
    }
    return DefWindowProcA(window, message, wParam, lParam);
}

}  // namespace

// Entry point: Orbiter calls it when loading the plugin DLL.
DLLCLBK void InitModule(HINSTANCE hDLL) {
    g_panel = new olcplugin::LaunchpadPanel(hDLL);
    oapiRegisterLaunchpadItem(g_panel);
    g_module = new LanCoopModule(hDLL, g_panel);
    oapiRegisterModule(g_module);
}

// Exit point: Orbiter calls it when unloading the DLL.
// The core does not delete a module registered with oapiRegisterModule
// (Orbiter.cpp:617, 656), so we delete it here.
DLLCLBK void ExitModule(HINSTANCE hDLL) {
    (void)hDLL;
    delete g_module;
    g_module = nullptr;
    oapiUnregisterLaunchpadItem(g_panel);
    delete g_panel;
    g_panel = nullptr;
}
