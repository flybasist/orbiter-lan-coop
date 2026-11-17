// Interpolation buffer tests: accuracy on orbit, buffer edges, late packets,
// landing and a reference body change.

#include <cmath>

#include "check.h"
#include "interpolation.h"

namespace {

// A circular orbit in the xy plane: 400 km above Earth with circular velocity.
const double kRadius = 6.771e6;
const double kSpeed = 7672.6;
const double kRate = 30.0;  // packets per second

// A flying vessel on a circular orbit at moment t.
olc::VesselState orbitState(double t) {
    double angle = kSpeed / kRadius * t;
    olc::VesselState state;
    state.netId = 7;
    state.status = static_cast<uint8_t>(olc::VesselStatus::Flying);
    state.refBody = 3;
    state.rpos = {kRadius * std::cos(angle), kRadius * std::sin(angle), 0.0};
    state.rvel = {-kSpeed * std::sin(angle), kSpeed * std::cos(angle), 0.0};
    state.arot = {0.0, 0.0, angle};  // the vessel turns together with the orbit
    state.vrot = {0.0, 0.0, kSpeed / kRadius};
    return state;
}

double distance(const olc::Vec3& a, const olc::Vec3& b) {
    double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Whether numbers match to a micrometre (coordinates are millions of metres).
bool near(double a, double b) {
    return std::fabs(a - b) < 1e-6;
}

void testOrbit() {
    olc::InterpolationBuffer buffer;
    for (int i = 0; i < 10; i++) {
        CHECK(buffer.add(i / kRate, orbitState(i / kRate)));
    }

    // Midway between packets 4 and 5. A straight line between them would cut the arc by ~1 mm,
    // Hermite must be off by much less.
    double t = 4.5 / kRate;
    olc::VesselState shown;
    bool starved = true;
    CHECK(buffer.sample(t, shown, starved));
    CHECK(!starved);
    olc::VesselState truth = orbitState(t);
    CHECK(distance(shown.rpos, truth.rpos) < 1e-5);
    CHECK(distance(shown.rvel, truth.rvel) < 1e-3);
    CHECK(std::fabs(shown.arot.z - truth.arot.z) < 1e-12);
    CHECK(shown.netId == 7 && shown.refBody == 3);

    // Exactly at a packet moment - the packet itself.
    CHECK(buffer.sample(3 / kRate, shown, starved));
    CHECK(distance(shown.rpos, orbitState(3 / kRate).rpos) < 1e-6);
}

void testEdges() {
    olc::InterpolationBuffer buffer;
    olc::VesselState shown;
    bool starved = false;

    // An empty buffer - nothing to show.
    CHECK(!buffer.sample(1.0, shown, starved));

    CHECK(buffer.add(1.0, orbitState(1.0)));
    CHECK(buffer.add(2.0, orbitState(2.0)));

    // A late or repeated packet is not accepted.
    CHECK(!buffer.add(1.5, orbitState(1.5)));
    CHECK(!buffer.add(2.0, orbitState(2.0)));
    CHECK(buffer.size() == 2);

    // Before the first packet - the first packet as is.
    CHECK(buffer.sample(0.5, shown, starved));
    CHECK(!starved && shown.rpos.x == orbitState(1.0).rpos.x);

    // The buffer ran out: motion with the last velocity, the last attitude.
    olc::VesselState last = orbitState(2.0);
    CHECK(buffer.sample(2.5, shown, starved));
    CHECK(starved);
    CHECK(near(shown.rpos.x, last.rpos.x + last.rvel.x * 0.5));
    CHECK(near(shown.rpos.y, last.rpos.y + last.rvel.y * 0.5));
    CHECK(shown.arot.z == last.arot.z);
}

void testBufferLimit() {
    // The buffer keeps only the last kInterpolationBufferSize packets.
    olc::InterpolationBuffer buffer;
    for (int i = 0; i < 100; i++) {
        buffer.add(i, orbitState(i));
    }
    CHECK(buffer.size() == olc::kInterpolationBufferSize);

    // The oldest are forgotten: before the remaining ones the first remaining one is shown.
    olc::VesselState shown;
    bool starved = false;
    CHECK(buffer.sample(0.0, shown, starved));
    double firstKept = 100.0 - olc::kInterpolationBufferSize;
    CHECK(shown.rpos.x == orbitState(firstKept).rpos.x);
}

void testNoInterpolationAcrossFrames() {
    // The vessel landed: we do not interpolate between flying and landed packets but continue
    // the flying one with its velocity until the landing moment comes.
    olc::VesselState flying = orbitState(0.0);
    flying.thrusters = {200};
    olc::VesselState landed;
    landed.netId = 7;
    landed.status = static_cast<uint8_t>(olc::VesselStatus::Landed);
    landed.refBody = 3;

    olc::InterpolationBuffer buffer;
    buffer.add(0.0, flying);
    buffer.add(0.1, landed);
    olc::VesselState shown;
    bool starved = true;
    CHECK(buffer.sample(0.05, shown, starved));
    CHECK(!starved);
    CHECK(shown.status == static_cast<uint8_t>(olc::VesselStatus::Flying));
    CHECK(near(shown.rpos.y, flying.rvel.y * 0.05));
    CHECK(shown.thrusters == flying.thrusters);

    // From the landing moment on - a landed vessel.
    CHECK(buffer.sample(0.1, shown, starved));
    CHECK(shown.status == static_cast<uint8_t>(olc::VesselStatus::Landed));

    // A reference body change: positions in different frames, also not interpolated.
    olc::VesselState otherBody = orbitState(0.1);
    otherBody.refBody = 4;
    olc::InterpolationBuffer soi;
    soi.add(0.0, flying);
    soi.add(0.1, otherBody);
    CHECK(soi.sample(0.05, shown, starved));
    CHECK(shown.refBody == 3);
    CHECK(near(shown.rpos.x, flying.rpos.x + flying.rvel.x * 0.05));
}

}  // namespace

int main() {
    testOrbit();
    testEdges();
    testBufferLimit();
    testNoInterpolationAcrossFrames();
    return checkResult();
}
