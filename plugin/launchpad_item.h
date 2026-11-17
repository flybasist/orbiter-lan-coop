// The "OLC" item in Launchpad (the "Extra" tab): settings and the "Connect" button.
//
// The dialog edits the role, host address, port and player name and writes them to
// Config\OrbiterLanCoop.cfg. "Connect" (client role) asks the host for its scenario
// (HELLO with kNoScenario, PROTOCOL.md section 3), writes it and starts it through Launchpad
// (scenario_launch.h). The dialog is modal, so the network is polled by the dialog's timer.
// The new session then connects with the number of this scenario (takeLaunchedScenarioId).
#pragma once

#include <cstdint>
#include <memory>

#include "Orbitersdk.h"

#include "client_link.h"
#include "config.h"

namespace olcplugin {

class LaunchpadPanel : public LaunchpadItem {
public:
    explicit LaunchpadPanel(HINSTANCE instance);
    ~LaunchpadPanel() override;

    char* Name() override;
    char* Description() override;
    bool clbkOpen(HWND launchpad) override;

    // The number of the host's scenario that "Connect" started; kOwnScenario if the session
    // was started some other way. Called at session start: the number is used only once.
    uint32_t takeLaunchedScenarioId();

private:
    static INT_PTR CALLBACK dialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam);
    void onInit(HWND dialog);
    void onConnect(HWND dialog);
    void onTimer(HWND dialog);
    void onClose(HWND dialog);
    // Reads the fields into config_.
    void readFields(HWND dialog);
    // Connect is only for the client role.
    void updateConnectButton(HWND dialog);
    void setStatus(HWND dialog, const std::string& text);
    void stopLink();

    HINSTANCE instance_;
    HWND launchpad_ = nullptr;
    Config config_;
    std::unique_ptr<olc::ClientLink> link_;  // only while connecting
    bool networkStarted_ = false;
    uint32_t launchedScenarioId_ = olc::kOwnScenario;
};

}  // namespace olcplugin
