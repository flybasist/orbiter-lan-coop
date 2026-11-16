// Настройки плагина из файла Config\OrbiterLanCoop.cfg в каталоге Orbiter.
//
// На этапе 1 настройки задаются файлом: так хост и клиент запускаются без интерфейса,
// в том числе по SSH без графики. Окно в Launchpad появится позже
// (docs/ARCHITECTURE.md, раздел «Интерфейс»). Пример файла - plugin/OrbiterLanCoop.cfg.
#pragma once

#include <string>

namespace olcplugin {

enum class Role {
    Off,     // плагин загружен, но ничего не делает
    Host,
    Client,
};

struct Config {
    Role role = Role::Off;
    std::string hostAddress = "127.0.0.1";  // клиент: адрес хоста
    int port = 47800;                       // хост и клиент
    std::string player = "player";          // клиент: имя игрока
    int maxPlayers = 4;                     // хост: предел игроков, хост тоже игрок
    int stateRateHz = 30;                   // хост: частота WORLD_STATE
    double displayDelay = 0.1;              // клиент: задержка показа, секунды
    // Клиент: расхождение сим-времени с хостом (секунды × ускорение), после которого
    // время клиента подтягивается прыжком.
    double timeSyncThreshold = 0.1;
    // Если не пусто - каждый кадр писать положение этого корабля в файл trace (CSV) для
    // сравнения хоста и клиента после прогона.
    std::string traceVessel;
    std::string traceFile = "olc-trace.csv";
};

// Читает Config\OrbiterLanCoop.cfg. Если файла нет - Role::Off. Ошибки в значениях
// пишутся в Orbiter.log, а для такого поля остаётся значение по умолчанию.
Config loadConfig();

const char* roleName(Role role);

}  // namespace olcplugin
