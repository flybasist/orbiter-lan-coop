// Минимальные трёхмерный вектор и кватернион для симуляции сглаживания.
#pragma once

#include <cmath>

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, double k) { return {a.x * k, a.y * k, a.z * k}; }

inline double length(Vec3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }

// Единичный вектор того же направления. Для нулевого вектора возвращает ноль.
inline Vec3 normalize(Vec3 a) {
    double len = length(a);
    if (len == 0.0) {
        return a;
    }
    return a * (1.0 / len);
}

// Линейная интерполяция: s = 0 даёт a, s = 1 даёт b.
inline Vec3 lerp(Vec3 a, Vec3 b, double s) { return a + (b - a) * s; }

// Кубическая интерполяция Эрмита между точками p0 и p1 со скоростями v0 и v1.
// h - длина интервала по времени, s - положение внутри интервала от 0 до 1.
inline Vec3 hermite(Vec3 p0, Vec3 v0, Vec3 p1, Vec3 v1, double h, double s) {
    double s2 = s * s;
    double s3 = s2 * s;
    double h00 = 2.0 * s3 - 3.0 * s2 + 1.0;
    double h10 = s3 - 2.0 * s2 + s;
    double h01 = -2.0 * s3 + 3.0 * s2;
    double h11 = s3 - s2;
    return p0 * h00 + v0 * (h10 * h) + p1 * h01 + v1 * (h11 * h);
}

// Кватернион поворота (w - скалярная часть), используем только единичные.
struct Quat {
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

// Произведение Гамильтона: сначала поворот b, потом a.
inline Quat operator*(Quat a, Quat b) {
    return {a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
            a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
}

inline Quat normalize(Quat q) {
    double len = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    return {q.w / len, q.x / len, q.y / len, q.z / len};
}

// Поворот на angle радиан вокруг оси axis (ось не обязана быть единичной).
inline Quat quatFromAxisAngle(Vec3 axis, double angle) {
    Vec3 n = normalize(axis);
    double s = std::sin(angle / 2.0);
    return {std::cos(angle / 2.0), n.x * s, n.y * s, n.z * s};
}

// Поворачивает q за время dt с угловой скоростью w (вектор в осях тела).
inline Quat rotateBy(Quat q, Vec3 w, double dt) {
    double speed = length(w);
    if (speed == 0.0) {
        return q;
    }
    return normalize(q * quatFromAxisAngle(w, speed * dt));
}

// Сферическая интерполяция: s = 0 даёт a, s = 1 даёт b, путь по короткой дуге.
inline Quat slerp(Quat a, Quat b, double s) {
    double d = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    if (d < 0.0) {  // q и -q задают один и тот же поворот
        b = {-b.w, -b.x, -b.y, -b.z};
        d = -d;
    }
    double ka = 1.0 - s;
    double kb = s;
    if (d < 0.9995) {  // обычный случай; при почти равных кватернионах хватает линейной смеси
        double theta = std::acos(d);
        ka = std::sin((1.0 - s) * theta) / std::sin(theta);
        kb = std::sin(s * theta) / std::sin(theta);
    }
    return normalize({a.w * ka + b.w * kb, a.x * ka + b.x * kb,
                      a.y * ka + b.y * kb, a.z * ka + b.z * kb});
}

// Угол в радианах между двумя ориентациями.
inline double angleBetween(Quat a, Quat b) {
    // Разностный поворот: conj(a) * b
    Quat conjA = {a.w, -a.x, -a.y, -a.z};
    Quat d = conjA * b;
    double vectorPart = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    return 2.0 * std::atan2(vectorPart, std::fabs(d.w));
}
