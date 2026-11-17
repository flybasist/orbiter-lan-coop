// A minimal 3D vector and quaternion for the smoothing simulation.
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

// A unit vector of the same direction. For a zero vector returns zero.
inline Vec3 normalize(Vec3 a) {
    double len = length(a);
    if (len == 0.0) {
        return a;
    }
    return a * (1.0 / len);
}

// Linear interpolation: s = 0 gives a, s = 1 gives b.
inline Vec3 lerp(Vec3 a, Vec3 b, double s) { return a + (b - a) * s; }

// Cubic Hermite interpolation between points p0 and p1 with velocities v0 and v1.
// h - interval length in time, s - position within the interval from 0 to 1.
inline Vec3 hermite(Vec3 p0, Vec3 v0, Vec3 p1, Vec3 v1, double h, double s) {
    double s2 = s * s;
    double s3 = s2 * s;
    double h00 = 2.0 * s3 - 3.0 * s2 + 1.0;
    double h10 = s3 - 2.0 * s2 + s;
    double h01 = -2.0 * s3 + 3.0 * s2;
    double h11 = s3 - s2;
    return p0 * h00 + v0 * (h10 * h) + p1 * h01 + v1 * (h11 * h);
}

// Rotation quaternion (w - scalar part); only unit quaternions are used.
struct Quat {
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

// Hamilton product: first rotation b, then a.
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

// Rotation by angle radians around axis (the axis need not be unit length).
inline Quat quatFromAxisAngle(Vec3 axis, double angle) {
    Vec3 n = normalize(axis);
    double s = std::sin(angle / 2.0);
    return {std::cos(angle / 2.0), n.x * s, n.y * s, n.z * s};
}

// Rotates q over time dt with angular velocity w (a vector in body axes).
inline Quat rotateBy(Quat q, Vec3 w, double dt) {
    double speed = length(w);
    if (speed == 0.0) {
        return q;
    }
    return normalize(q * quatFromAxisAngle(w, speed * dt));
}

// Spherical interpolation: s = 0 gives a, s = 1 gives b, along the short arc.
inline Quat slerp(Quat a, Quat b, double s) {
    double d = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    if (d < 0.0) {  // q and -q give the same rotation
        b = {-b.w, -b.x, -b.y, -b.z};
        d = -d;
    }
    double ka = 1.0 - s;
    double kb = s;
    if (d < 0.9995) {  // the usual case; for almost equal quaternions a linear blend is enough
        double theta = std::acos(d);
        ka = std::sin((1.0 - s) * theta) / std::sin(theta);
        kb = std::sin(s * theta) / std::sin(theta);
    }
    return normalize({a.w * ka + b.w * kb, a.x * ka + b.x * kb,
                      a.y * ka + b.y * kb, a.z * ka + b.z * kb});
}

// Angle in radians between two attitudes.
inline double angleBetween(Quat a, Quat b) {
    // Difference rotation: conj(a) * b
    Quat conjA = {a.w, -a.x, -a.y, -a.z};
    Quat d = conjA * b;
    double vectorPart = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    return 2.0 * std::atan2(vectorPart, std::fabs(d.w));
}
