// Эталонное ("истинное") движение корабля на хосте.
#pragma once

#include <vector>

#include "vec.h"

// Шаг интегрирования по симуляционному времени, секунды.
constexpr double kStep = 0.01;

// Профиль движения: плавный (один длинный манёвр) или "пилотский" (импульсы двигателей и RCS).
enum class Profile { Smooth, Pilot };

// Состояние корабля в момент симуляционного времени time.
// Это же содержимое сетевого пакета.
struct State {
    double time = 0.0;  // симуляционное время, с
    Vec3 pos;           // положение, м
    Vec3 vel;           // скорость, м/с
    Vec3 acc;           // полное ускорение (гравитация + тяга), м/с^2
    Quat att;           // ориентация
    Vec3 angVel;        // угловая скорость, рад/с
};

// Силы, кроме гравитации, для расчёта орбиты.
struct Forces {
    double thrustAlongVelocity = 0.0;  // тяга вдоль скорости, м/с^2
    Vec3 constantAcc;                  // постоянное ускорение в мировых осях, м/с^2
};

// Ускорение свободного падения к центру Земли в точке pos.
Vec3 gravityAccel(Vec3 pos);

// Переносит pos и vel вперёд на duration секунд методом RK4 (шаги не больше kStep).
void propagateOrbit(Vec3& pos, Vec3& vel, const Forces& forces, double duration);

// Истинное движение: считается один раз при создании, дальше можно
// спрашивать состояние в любой момент симуляционного времени.
class Trajectory {
public:
    Trajectory(double duration, Profile profile);

    // Состояние в момент t (за пределами записанного диапазона берётся крайнее значение).
    State at(double t) const;

private:
    std::vector<State> samples_;  // состояния через каждые kStep секунд
};
