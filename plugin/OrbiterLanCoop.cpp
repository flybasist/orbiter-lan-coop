// OrbiterLanCoop - плагин сетевой игры для Orbiter (этап 1: хост рассылает мир, клиент
// его показывает; docs/PLAN.md).
//
// Роль (хост, клиент или выключен) задаётся файлом Config\OrbiterLanCoop.cfg (config.h).
// Работа роли - в HostSide и ClientSide, а этот файл только связывает их с Orbiter:
// - clbkSimulationStart - запуск сети и роли. Не InitModule: InitModule вызывается из
//   DllMain, где нельзя запускать Winsock (docs/ORBITER-INTERNALS.md, раздел 14);
// - clbkPreStep - работа каждый кадр (снять или навязать состояния);
// - на паузе clbkPreStep не вызывается, поэтому у плагина есть скрытое окно Windows с
//   таймером: его сообщения приходят всегда, и на паузе роль работает по таймеру
//   (решение 23 в docs/DECISIONS.md);
// - clbkSimulationEnd - остановка.

#define ORBITER_MODULE
#include "Orbitersdk.h"

#include <memory>

#include "client_side.h"
#include "config.h"
#include "host_side.h"
#include "protocol.h"
#include "socket.h"

namespace {

// Таймер скрытого окна: как часто работать на паузе, миллисекунды.
constexpr UINT kTimerIntervalMs = 50;
constexpr UINT_PTR kTimerId = 1;
const char kWindowClass[] = "OrbiterLanCoopTimer";

class LanCoopModule;
LanCoopModule* g_module = nullptr;

LRESULT CALLBACK timerWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

// Модуль плагина: Orbiter создаёт сессии, а этот объект живёт между ними.
class LanCoopModule : public oapi::Module {
public:
    explicit LanCoopModule(HINSTANCE hDLL) : oapi::Module(hDLL), instance_(hDLL) {}

    void clbkSimulationStart(RenderMode mode) override {
        // Строки в лог - на английском (правила проекта).
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
            host_.reset(new olcplugin::HostSide(config_));
            started = host_->start();
        } else {
            client_.reset(new olcplugin::ClientSide(config_));
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
        // Мир здесь уже удалён: корабли не трогаем (ORBITER-INTERNALS.md, раздел 8).
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

    // Таймер скрытого окна. Вне паузы работу делает clbkPreStep.
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

    // Скрытое окно только для сообщений (HWND_MESSAGE): на экране его нет.
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

// Точка входа: Orbiter вызывает её при загрузке DLL плагина.
DLLCLBK void InitModule(HINSTANCE hDLL) {
    g_module = new LanCoopModule(hDLL);
    oapiRegisterModule(g_module);
}

// Точка выхода: Orbiter вызывает её при выгрузке DLL.
// Модуль, зарегистрированный через oapiRegisterModule, ядро само не удаляет
// (Orbiter.cpp:617, 656), поэтому удаляем его здесь.
DLLCLBK void ExitModule(HINSTANCE hDLL) {
    (void)hDLL;
    delete g_module;
    g_module = nullptr;
}
