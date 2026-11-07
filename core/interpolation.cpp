#include "interpolation.h"

#include "attitude.h"

namespace olc {

namespace {

bool isFlying(const VesselState& state) {
    return state.status == static_cast<uint8_t>(VesselStatus::Flying);
}

Vec3 lerp(const Vec3& a, const Vec3& b, double s) {
    Vec3 result;
    result.x = a.x + (b.x - a.x) * s;
    result.y = a.y + (b.y - a.y) * s;
    result.z = a.z + (b.z - a.z) * s;
    return result;
}

// Кубическая интерполяция Эрмита: кривая проходит через p0 и p1 и имеет в них
// скорости v0 и v1. h - время между точками, s - доля пути от 0 до 1.
// Скорости умножаются на h, потому что формула считает в долях, а не в секундах.
double hermite(double p0, double v0, double p1, double v1, double h, double s) {
    double s2 = s * s;
    double s3 = s2 * s;
    double h00 = 2 * s3 - 3 * s2 + 1;
    double h10 = s3 - 2 * s2 + s;
    double h01 = -2 * s3 + 3 * s2;
    double h11 = s3 - s2;
    return h00 * p0 + h10 * h * v0 + h01 * p1 + h11 * h * v1;
}

// Состояние, продолженное от пакета на dt секунд по его скорости (ориентация не меняется).
VesselState moveOn(const VesselState& state, double dt) {
    VesselState result = state;
    if (isFlying(state)) {
        result.rpos.x += state.rvel.x * dt;
        result.rpos.y += state.rvel.y * dt;
        result.rpos.z += state.rvel.z * dt;
    }
    return result;
}

}  // namespace

bool InterpolationBuffer::add(double simTime, const VesselState& state) {
    if (!entries_.empty() && simTime <= entries_.back().simTime) {
        return false;
    }
    entries_.push_back({simTime, state});
    // Старые пакеты больше не нужны: показываемый момент только движется вперёд.
    if (entries_.size() > kInterpolationBufferSize) {
        entries_.erase(entries_.begin());
    }
    return true;
}

bool InterpolationBuffer::sample(double simTime, VesselState& result, bool& starved) const {
    starved = false;
    if (entries_.empty()) {
        return false;
    }

    // Раньше первого пакета показать нечего, кроме него самого.
    if (simTime <= entries_.front().simTime) {
        result = entries_.front().state;
        return true;
    }

    // Буфер кончился: продолжаем движение по последней скорости.
    const Entry& newest = entries_.back();
    if (simTime >= newest.simTime) {
        starved = simTime > newest.simTime;
        result = moveOn(newest.state, simTime - newest.simTime);
        return true;
    }

    // Ищем с конца пару пакетов a и b, между которыми лежит simTime: показываемый
    // момент обычно близко к концу буфера.
    size_t i = entries_.size() - 2;
    while (entries_[i].simTime > simTime) {
        i--;
    }
    const Entry& a = entries_[i];
    const Entry& b = entries_[i + 1];

    // Между стоящим на поверхности и летящим, а также между пакетами с разными
    // опорными телами интерполировать нельзя: положения в разных системах отсчёта.
    // Тогда до прихода момента b показываем a, продолженный по его скорости.
    if (!isFlying(a.state) || !isFlying(b.state) || a.state.refBody != b.state.refBody) {
        result = moveOn(a.state, simTime - a.simTime);
        return true;
    }

    double h = b.simTime - a.simTime;
    double s = (simTime - a.simTime) / h;
    const VesselState& from = a.state;
    const VesselState& to = b.state;

    result = from;  // номер, статус, опорное тело и уровни двигателей - из a
    result.rpos.x = hermite(from.rpos.x, from.rvel.x, to.rpos.x, to.rvel.x, h, s);
    result.rpos.y = hermite(from.rpos.y, from.rvel.y, to.rpos.y, to.rvel.y, h, s);
    result.rpos.z = hermite(from.rpos.z, from.rvel.z, to.rpos.z, to.rvel.z, h, s);
    result.rvel = lerp(from.rvel, to.rvel, s);
    result.arot = quatToEuler(slerp(eulerToQuat(from.arot), eulerToQuat(to.arot), s));
    result.vrot = lerp(from.vrot, to.vrot, s);
    return true;
}

}  // namespace olc
