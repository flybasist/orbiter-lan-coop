#include "launchpad_item.h"

#include <string>

#include "protocol.h"
#include "resource.h"
#include "scenario_launch.h"
#include "socket.h"

namespace olcplugin {

namespace {

// How often the dialog polls the network while connecting, milliseconds.
constexpr UINT kPollIntervalMs = 50;
constexpr UINT_PTR kPollTimerId = 1;

// After "Connect" the dialog closes, and the scenario is started a little later by a timer:
// by then Launchpad is back in its own message loop, as in the check of ORBITER-INTERNALS.md,
// section 18 (pressing "Launch" from inside the modal dialog is not relied upon).
constexpr UINT kLaunchDelayMs = 300;
HWND g_launchpadToStart = nullptr;

void CALLBACK startScenarioTimer(HWND, UINT, UINT_PTR timer, DWORD) {
    KillTimer(nullptr, timer);
    if (g_launchpadToStart) {
        launchReceivedScenario(g_launchpadToStart);
        g_launchpadToStart = nullptr;
    }
}

// Roles in the list, in this order.
const Role kRoles[] = {Role::Off, Role::Host, Role::Client};

// Real time in seconds for ClientLink. oapiGetSysTime belongs to a session, and in Launchpad
// there is no session yet.
double secondsNow() {
    return static_cast<double>(GetTickCount64()) / 1000.0;
}

std::string fieldText(HWND dialog, int id) {
    char buffer[256] = {};
    GetDlgItemTextA(dialog, id, buffer, sizeof(buffer));
    return buffer;
}

}  // namespace

LaunchpadPanel::LaunchpadPanel(HINSTANCE instance) : LaunchpadItem(), instance_(instance) {}

LaunchpadPanel::~LaunchpadPanel() {
    stopLink();
}

char* LaunchpadPanel::Name() {
    return const_cast<char*>("OLC - LAN co-op");
}

char* LaunchpadPanel::Description() {
    return const_cast<char*>(
        "Orbiter LAN co-op: play one world together on a local network. "
        "Choose the role here; a client connects to the host with one button.");
}

bool LaunchpadPanel::clbkOpen(HWND launchpad) {
    launchpad_ = launchpad;
    return OpenDialog(instance_, launchpad, IDD_OLC, dialogProc);
}

uint32_t LaunchpadPanel::takeLaunchedScenarioId() {
    const uint32_t id = launchedScenarioId_;
    launchedScenarioId_ = olc::kOwnScenario;
    return id;
}

INT_PTR CALLBACK LaunchpadPanel::dialogProc(HWND dialog, UINT message, WPARAM wParam,
                                            LPARAM lParam) {
    // OpenDialog passes the item as the dialog parameter; keep it in the window.
    if (message == WM_INITDIALOG) {
        SetWindowLongPtrA(dialog, DWLP_USER, lParam);
        reinterpret_cast<LaunchpadPanel*>(lParam)->onInit(dialog);
        return TRUE;
    }
    auto* panel = reinterpret_cast<LaunchpadPanel*>(GetWindowLongPtrA(dialog, DWLP_USER));
    if (!panel) {
        return FALSE;
    }
    switch (message) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_OLC_CONNECT:
            panel->onConnect(dialog);
            return TRUE;
        case IDC_OLC_ROLE:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                panel->updateConnectButton(dialog);
            }
            return TRUE;
        case IDCANCEL:
            panel->onClose(dialog);
            return TRUE;
        }
        break;
    case WM_TIMER:
        if (wParam == kPollTimerId) {
            panel->onTimer(dialog);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void LaunchpadPanel::onInit(HWND dialog) {
    config_ = loadConfig();
    HWND roles = GetDlgItem(dialog, IDC_OLC_ROLE);
    for (Role role : kRoles) {
        SendMessageA(roles, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(roleName(role)));
        if (role == config_.role) {
            SendMessageA(roles, CB_SETCURSEL, SendMessageA(roles, CB_GETCOUNT, 0, 0) - 1, 0);
        }
    }
    SetDlgItemTextA(dialog, IDC_OLC_ADDRESS, config_.hostAddress.c_str());
    SetDlgItemInt(dialog, IDC_OLC_PORT, static_cast<UINT>(config_.port), FALSE);
    SetDlgItemTextA(dialog, IDC_OLC_PLAYER, config_.player.c_str());
    updateConnectButton(dialog);
}

void LaunchpadPanel::readFields(HWND dialog) {
    const LRESULT index = SendDlgItemMessageA(dialog, IDC_OLC_ROLE, CB_GETCURSEL, 0, 0);
    if (index >= 0 && index < static_cast<LRESULT>(sizeof(kRoles) / sizeof(kRoles[0]))) {
        config_.role = kRoles[index];
    }
    config_.hostAddress = fieldText(dialog, IDC_OLC_ADDRESS);
    BOOL portOk = FALSE;
    const UINT port = GetDlgItemInt(dialog, IDC_OLC_PORT, &portOk, FALSE);
    if (portOk && port >= 1 && port <= 65535) {
        config_.port = static_cast<int>(port);
    }
    config_.player = fieldText(dialog, IDC_OLC_PLAYER);
}

void LaunchpadPanel::updateConnectButton(HWND dialog) {
    readFields(dialog);
    EnableWindow(GetDlgItem(dialog, IDC_OLC_CONNECT), config_.role == Role::Client && !link_);
}

void LaunchpadPanel::setStatus(HWND dialog, const std::string& text) {
    SetDlgItemTextA(dialog, IDC_OLC_STATUS, text.c_str());
}

void LaunchpadPanel::onConnect(HWND dialog) {
    readFields(dialog);
    saveConfig(config_);

    olc::ClientConfig clientConfig;
    if (!olc::parseIpv4(config_.hostAddress, clientConfig.host.ip)) {
        setStatus(dialog, "The host address must be digits, for example 192.168.1.10.");
        return;
    }
    if (!networkStarted_ && !olc::startNetwork()) {
        setStatus(dialog, "Cannot start the network.");
        return;
    }
    networkStarted_ = true;
    clientConfig.host.port = static_cast<uint16_t>(config_.port);
    clientConfig.player = config_.player;
    clientConfig.orbiterVersion = static_cast<uint32_t>(oapiGetOrbiterVersion());
    clientConfig.pluginVersion = kPluginVersion;
    clientConfig.scenarioId = olc::kNoScenario;  // ask the host for its world

    link_.reset(new olc::ClientLink());
    if (!link_->start(clientConfig, secondsNow())) {
        setStatus(dialog, "Cannot connect: " + link_->closeReason());
        stopLink();
        updateConnectButton(dialog);
        return;
    }
    setStatus(dialog, "Connecting to " + config_.hostAddress + " and getting the host's world...");
    EnableWindow(GetDlgItem(dialog, IDC_OLC_CONNECT), FALSE);
    SetTimer(dialog, kPollTimerId, kPollIntervalMs, nullptr);
}

void LaunchpadPanel::onTimer(HWND dialog) {
    if (!link_) {
        KillTimer(dialog, kPollTimerId);
        return;
    }
    link_->poll(secondsNow());
    for (const std::string& line : link_->takeLog()) {
        oapiWriteLogV("%s", line.c_str());
    }
    if (link_->state() != olc::ClientLink::State::Closed) {
        return;
    }
    KillTimer(dialog, kPollTimerId);

    if (!link_->hasScenario()) {
        setStatus(dialog, "Not connected: " + link_->closeReason());
        stopLink();
        updateConnectButton(dialog);
        return;
    }
    const uint32_t scenarioId = link_->scenario().scenarioId;
    const bool written = writeReceivedScenario(link_->scenario().data);
    stopLink();
    if (!written) {
        setStatus(dialog, "Cannot write the host's scenario, see Orbiter.log.");
        updateConnectButton(dialog);
        return;
    }
    // The next session started from Launchpad is this scenario; it gets the number in HELLO.
    launchedScenarioId_ = scenarioId;
    EndDialog(dialog, 0);
    g_launchpadToStart = launchpad_;
    SetTimer(nullptr, 0, kLaunchDelayMs, startScenarioTimer);
}

void LaunchpadPanel::onClose(HWND dialog) {
    KillTimer(dialog, kPollTimerId);
    stopLink();
    readFields(dialog);
    saveConfig(config_);
    EndDialog(dialog, 0);
}

void LaunchpadPanel::stopLink() {
    if (link_) {
        link_->stop("connection cancelled");
        link_.reset();
    }
    if (networkStarted_) {
        olc::stopNetwork();
        networkStarted_ = false;
    }
}

}  // namespace olcplugin
