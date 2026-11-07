#include "attitude.h"

#include <algorithm>
#include <cmath>

namespace olc {

namespace {

// Матрица 3×3: m[строка][столбец].
struct Matrix3 {
    double m[3][3];
};

// Матрица из углов - та же формула, что в Vessel::SetGlobalOrientation.
Matrix3 eulerToMatrix(const Vec3& arot) {
    double sinx = std::sin(arot.x), cosx = std::cos(arot.x);
    double siny = std::sin(arot.y), cosy = std::cos(arot.y);
    double sinz = std::sin(arot.z), cosz = std::cos(arot.z);
    Matrix3 r = {{
        {cosy * cosz, cosy * sinz, -siny},
        {sinx * siny * cosz - cosx * sinz, sinx * siny * sinz + cosx * cosz, sinx * cosy},
        {cosx * siny * cosz + sinx * sinz, cosx * siny * sinz - sinx * cosz, cosx * cosy},
    }};
    return r;
}

// Углы из матрицы - обратное к eulerToMatrix.
// По формуле: m[0][2] = -sin y, m[1][2] / m[2][2] = tg x, m[0][1] / m[0][0] = tg z.
Vec3 matrixToEuler(const Matrix3& r) {
    Vec3 arot;
    // Из-за округления |m[0][2]| может чуть превысить 1: asin тогда дал бы NaN.
    double siny = std::clamp(-r.m[0][2], -1.0, 1.0);
    arot.y = std::asin(siny);
    double cosy = std::cos(arot.y);
    if (cosy > 1e-9) {
        arot.x = std::atan2(r.m[1][2], r.m[2][2]);
        arot.z = std::atan2(r.m[0][1], r.m[0][0]);
    } else {
        // y = ±π/2: x и z крутят вокруг одной оси. Берём x = 0, тогда по формуле
        // m[1][0] = -sin z и m[1][1] = cos z.
        arot.x = 0.0;
        arot.z = std::atan2(-r.m[1][0], r.m[1][1]);
    }
    return arot;
}

// Матрица → кватернион (стандартный способ: выбираем самую большую из четырёх
// величин w², x², y², z², чтобы не делить на число около нуля).
Quat matrixToQuat(const Matrix3& r) {
    const double (*m)[3] = r.m;
    double trace = m[0][0] + m[1][1] + m[2][2];
    Quat q;
    if (trace > 0.0) {
        double s = 2.0 * std::sqrt(1.0 + trace);  // s = 4w
        q.w = 0.25 * s;
        q.x = (m[2][1] - m[1][2]) / s;
        q.y = (m[0][2] - m[2][0]) / s;
        q.z = (m[1][0] - m[0][1]) / s;
    } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
        double s = 2.0 * std::sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]);  // s = 4x
        q.w = (m[2][1] - m[1][2]) / s;
        q.x = 0.25 * s;
        q.y = (m[0][1] + m[1][0]) / s;
        q.z = (m[0][2] + m[2][0]) / s;
    } else if (m[1][1] > m[2][2]) {
        double s = 2.0 * std::sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]);  // s = 4y
        q.w = (m[0][2] - m[2][0]) / s;
        q.x = (m[0][1] + m[1][0]) / s;
        q.y = 0.25 * s;
        q.z = (m[1][2] + m[2][1]) / s;
    } else {
        double s = 2.0 * std::sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]);  // s = 4z
        q.w = (m[1][0] - m[0][1]) / s;
        q.x = (m[0][2] + m[2][0]) / s;
        q.y = (m[1][2] + m[2][1]) / s;
        q.z = 0.25 * s;
    }
    return q;
}

// Кватернион → матрица (обратное к matrixToQuat).
Matrix3 quatToMatrix(const Quat& q) {
    double w = q.w, x = q.x, y = q.y, z = q.z;
    Matrix3 r = {{
        {1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)},
        {2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)},
        {2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)},
    }};
    return r;
}

// Приводит кватернион к единичной длине: после арифметики длина чуть уходит от 1.
Quat normalized(const Quat& q) {
    double length = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    Quat result;
    result.w = q.w / length;
    result.x = q.x / length;
    result.y = q.y / length;
    result.z = q.z / length;
    return result;
}

}  // namespace

Quat eulerToQuat(const Vec3& arot) {
    return normalized(matrixToQuat(eulerToMatrix(arot)));
}

Vec3 quatToEuler(const Quat& q) {
    return matrixToEuler(quatToMatrix(normalized(q)));
}

Quat slerp(const Quat& a, const Quat& b, double s) {
    // q и -q - один и тот же поворот. Если угол между a и b больше 90° в
    // пространстве кватернионов, берём -b: так путь от a к b будет кратчайшим.
    double dot = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    Quat end = b;
    if (dot < 0.0) {
        dot = -dot;
        end.w = -b.w;
        end.x = -b.x;
        end.y = -b.y;
        end.z = -b.z;
    }

    // Почти одинаковые повороты: sin угла около нуля, делить на него нельзя.
    // На таком малом угле обычная линейная смесь ничем не хуже.
    double weightA = 1.0 - s;
    double weightB = s;
    if (dot < 0.9995) {
        double angle = std::acos(dot);
        weightA = std::sin((1.0 - s) * angle) / std::sin(angle);
        weightB = std::sin(s * angle) / std::sin(angle);
    }

    Quat result;
    result.w = weightA * a.w + weightB * end.w;
    result.x = weightA * a.x + weightB * end.x;
    result.y = weightA * a.y + weightB * end.y;
    result.z = weightA * a.z + weightB * end.z;
    return normalized(result);
}

}  // namespace olc
