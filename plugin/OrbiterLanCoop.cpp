// OrbiterLanCoop - плагин сетевой игры для Orbiter.
//
// Пока это скелет (задача И3 в docs/PLAN.md). Он проверяет три вещи:
//   1. плагин собирается против SDK Orbiter вместе с сетевым ядром olc-core;
//   2. Orbiter загружает его (вкладка Modules в Launchpad);
//   3. плагин получает колбэки начала и конца сессии и пишет о них в Orbiter.log.
//
// Сеть и остальная работа создаются в clbkSimulationStart, а не в InitModule:
// InitModule вызывается из DllMain, где нельзя запускать потоки и Winsock
// (docs/ORBITER-INTERNALS.md, раздел 14).

#define ORBITER_MODULE
#include "Orbitersdk.h"

#include "protocol.h"

namespace {

// Модуль плагина: Orbiter создаёт сессии, а этот объект живёт между ними.
class LanCoopModule : public oapi::Module {
public:
    explicit LanCoopModule(HINSTANCE hDLL) : oapi::Module(hDLL) {}

    void clbkSimulationStart(RenderMode mode) override {
        // Строки в лог - на английском (правила проекта).
        oapiWriteLogV("OLC: session started (protocol v%u, default port %u, render mode %d)",
                      static_cast<unsigned>(olc::kProtocolVersion),
                      static_cast<unsigned>(olc::kDefaultPort), static_cast<int>(mode));
    }

    void clbkSimulationEnd() override {
        // Мир здесь уже удалён: корабли не трогаем (ORBITER-INTERNALS.md, раздел 8).
        oapiWriteLogV("OLC: session ended");
    }
};

LanCoopModule* g_module = nullptr;

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
