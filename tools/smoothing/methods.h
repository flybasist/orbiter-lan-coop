// Способы восстановления движения на клиенте.
#pragma once

#include <vector>

#include "trajectory.h"

// Что клиент рисует в данный кадр.
struct Display {
    Vec3 pos;
    Quat att;
    bool starved = false;  // буфер кончился: пришлось экстраполировать (только для интерполяции)
};

// 1. Как обычный DefSetStateEx в Orbiter: последнее состояние, перенесённое
//    до момента nowSim только под действием гравитации.
Display snapGravity(const State& newest, double nowSim);

// 2. Последнее состояние, перенесённое до nowSim с постоянным ускорением из пакета.
Display extrapolate(const State& newest, double nowSim);

// 3. Линейная интерполяция положения между пакетами, slerp для ориентации.
//    buffer - принятые пакеты по возрастанию времени, tDisp - показываемое время.
Display interpLinear(const std::vector<State>& buffer, double tDisp);

// 4. Кубическая интерполяция Эрмита по положениям и скоростям, slerp для ориентации.
Display interpHermite(const std::vector<State>& buffer, double tDisp);
