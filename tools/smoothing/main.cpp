// olc-smoothing: measures how smoothly and accurately the client shows the host's vessel
// with different ways of reconstructing motion from packets with delay, jitter and loss.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "methods.h"
#include "network.h"
#include "trajectory.h"

namespace {

constexpr double kFps = 60.0;       // client frame rate (real time)
constexpr double kWarmupSim = 5.0;  // the first seconds of simulation are not counted in the metrics
constexpr double kRadToDeg = 57.29577951308232;
constexpr int kMethodCount = 4;

const char* const kMethodNames[kMethodCount] = {"snap_gravity", "extrapolate", "interp_linear",
                                                "interp_hermite"};
// Interpolation methods show time with a delay and may "starve" without packets.
const bool kIsInterp[kMethodCount] = {false, false, true, true};

// Profile name for output.
const char* profileName(Profile profile) {
    return profile == Profile::Smooth ? "smooth" : "pilot";
}

const char* const kCsvHeader =
    "rate,warp,loss,latency_ms,jitter_ms,delay_ms,profile,method,pos_mean_m,pos_p95_m,pos_max_m,"
    "att_max_deg,jump_max_m,starved_pct,lag_sim_s";

// Run parameters: simulation numbers and output mode.
struct Options {
    Config config;
    bool csv = false;
    bool csvHeader = false;
    bool help = false;
};

// The result for one method.
struct Result {
    double posMean = 0.0;
    double posP95 = 0.0;
    double posMax = 0.0;
    double attMaxDeg = 0.0;
    double jumpMax = 0.0;
    double starvedPct = 0.0;
};

// Accumulates one method's metrics frame by frame.
class Accumulator {
public:
    // A new frame: d - what was shown, truth - what it should be at the same time.
    void addFrame(const Display& d, const State& truth) {
        posErrors_.push_back(length(d.pos - truth.pos));
        attMaxRad_ = std::max(attMaxRad_, angleBetween(d.att, truth.att));
        if (d.starved) {
            starvedFrames_++;
        }
        // Jerk: how much the shift of the shown state differs from the shift of the true one over a frame.
        if (hasPrev_) {
            Vec3 shown = d.pos - prevShown_;
            Vec3 real = truth.pos - prevTruth_;
            jumpMax_ = std::max(jumpMax_, length(shown - real));
        }
        prevShown_ = d.pos;
        prevTruth_ = truth.pos;
        hasPrev_ = true;
    }

    // A frame was skipped (warm-up): the next frame must not be compared with the previous one.
    void skipFrame() { hasPrev_ = false; }

    size_t frameCount() const { return posErrors_.size(); }

    Result finish() {
        Result r;
        std::sort(posErrors_.begin(), posErrors_.end());
        double sum = 0.0;
        for (double e : posErrors_) {
            sum += e;
        }
        size_t n = posErrors_.size();
        r.posMean = sum / n;
        r.posP95 = posErrors_[static_cast<size_t>(0.95 * (n - 1))];
        r.posMax = posErrors_.back();
        r.attMaxDeg = attMaxRad_ * kRadToDeg;
        r.jumpMax = jumpMax_;
        r.starvedPct = 100.0 * starvedFrames_ / n;
        return r;
    }

private:
    std::vector<double> posErrors_;
    double attMaxRad_ = 0.0;
    double jumpMax_ = 0.0;
    size_t starvedFrames_ = 0;
    bool hasPrev_ = false;
    Vec3 prevShown_;
    Vec3 prevTruth_;
};

// Runs the client over the packet stream and returns results for all methods.
// Returns false if not a single frame was collected for the metrics.
bool runClient(const Config& config, const Trajectory& truth, const std::vector<Packet>& packets,
               Result results[kMethodCount]) {
    Accumulator accumulators[kMethodCount];

    std::vector<State> buffer;  // received packets in ascending simulation time
    double newestArrival = 0.0;
    size_t nextPacket = 0;

    double delaySim = config.delayMs / 1000.0 * config.warp;  // interpolation delay in simulation seconds
    long lastFrame = static_cast<long>(config.duration / config.warp * kFps);

    for (long k = 0; k <= lastFrame; k++) {
        double wall = k / kFps;

        // Accept everything that arrived by this frame. A packet older than one we have is dropped.
        while (nextPacket < packets.size() && packets[nextPacket].arrivalWall <= wall) {
            const Packet& p = packets[nextPacket];
            if (buffer.empty() || p.state.time > buffer.back().time) {
                buffer.push_back(p.state);
                newestArrival = p.arrivalWall;
            }
            nextPacket++;
        }
        if (buffer.empty()) {
            continue;
        }

        // Estimate of the current host simulation time: the time of the last packet plus
        // the real time since it arrived, multiplied by warp. This is a simplification:
        // real time synchronization is a separate task.
        double nowSim = buffer.back().time + (wall - newestArrival) * config.warp;

        Display shown[kMethodCount];
        double shownTime[kMethodCount];
        shown[0] = snapGravity(buffer.back(), nowSim);
        shownTime[0] = nowSim;
        shown[1] = extrapolate(buffer.back(), nowSim);
        shownTime[1] = nowSim;
        shownTime[2] = shownTime[3] = nowSim - delaySim;
        shown[2] = interpLinear(buffer, shownTime[2]);
        shown[3] = interpHermite(buffer, shownTime[3]);

        // Warm-up frames and frames where the displayed time is earlier than the first packet are not counted:
        // a real client starts showing only when it has data for that time.
        // Without this, at rare packets (×100, 10 Hz) the client start gets into the error.
        for (int m = 0; m < kMethodCount; m++) {
            if (shownTime[m] < kWarmupSim || shownTime[m] < buffer.front().time) {
                accumulators[m].skipFrame();
            } else {
                accumulators[m].addFrame(shown[m], truth.at(shownTime[m]));
            }
        }
    }

    for (int m = 0; m < kMethodCount; m++) {
        if (accumulators[m].frameCount() == 0) {
            return false;
        }
        results[m] = accumulators[m].finish();
    }
    return true;
}

// CSV output: one line per method.
void printCsv(const Config& c, const Result results[kMethodCount]) {
    for (int m = 0; m < kMethodCount; m++) {
        const Result& r = results[m];
        double lag = kIsInterp[m] ? c.delayMs / 1000.0 * c.warp : 0.0;
        std::string starved = "-";
        if (kIsInterp[m]) {
            char text[32];
            std::snprintf(text, sizeof(text), "%.2f", r.starvedPct);
            starved = text;
        }
        std::printf("%g,%g,%g,%g,%g,%g,%s,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%s,%.3f\n", c.rate, c.warp,
                    c.lossPercent, c.latencyMs, c.jitterMs, c.delayMs, profileName(c.profile), kMethodNames[m], r.posMean,
                    r.posP95, r.posMax, r.attMaxDeg, r.jumpMax, starved.c_str(), lag);
    }
}

// Aligned table output.
void printTable(const Config& c, const Result results[kMethodCount]) {
    std::printf("rate=%g Hz  warp=%g  loss=%g%%  latency=%g ms  jitter=%g ms  delay=%g ms  "
                "seed=%u  duration=%g s  profile=%s\n\n",
                c.rate, c.warp, c.lossPercent, c.latencyMs, c.jitterMs, c.delayMs, c.seed,
                c.duration, profileName(c.profile));
    std::printf("%-15s %12s %12s %12s %12s %12s %10s %10s\n", "method", "pos_mean_m", "pos_p95_m",
                "pos_max_m", "att_max_deg", "jump_max_m", "starved_%", "lag_sim_s");
    for (int m = 0; m < kMethodCount; m++) {
        const Result& r = results[m];
        char starved[32] = "-";
        if (kIsInterp[m]) {
            std::snprintf(starved, sizeof(starved), "%.2f", r.starvedPct);
        }
        double lag = kIsInterp[m] ? c.delayMs / 1000.0 * c.warp : 0.0;
        std::printf("%-15s %12.4f %12.4f %12.4f %12.4f %12.4f %10s %10.3f\n", kMethodNames[m],
                    r.posMean, r.posP95, r.posMax, r.attMaxDeg, r.jumpMax, starved, lag);
    }
}

void printUsage() {
    std::printf(
        "Usage: olc-smoothing [options]\n"
        "\n"
        "Simulates a host sending vessel states over UDP and compares how well\n"
        "several client-side methods reconstruct the motion.\n"
        "\n"
        "Options:\n"
        "  --rate R        send rate, packets per wall second (default 20)\n"
        "  --warp W        sim time runs W times faster than wall time (default 1)\n"
        "  --loss P        packet loss, percent (default 0)\n"
        "  --latency MS    base network latency, ms of wall time (default 1)\n"
        "  --jitter MS     extra random latency 0..MS, ms of wall time (default 5)\n"
        "  --delay MS      interpolation delay, ms of wall time (default 100)\n"
        "  --seed N        random seed (default 1)\n"
        "  --duration S    simulated time in sim seconds (default 120)\n"
        "  --profile P     reference motion: smooth or pilot (default pilot)\n"
        "  --csv           print one CSV line per method instead of a table\n"
        "  --csv-header    print only the CSV header line\n"
        "  --help          print this text\n");
}

// Reads a number from text; false if the text is not just a number.
bool parseNumber(const char* text, double& value) {
    char* end = nullptr;
    value = std::strtod(text, &end);
    return end != text && *end == '\0' && std::isfinite(value);
}

// Parses command line arguments. On error prints a message and returns false.
bool parseArgs(int argc, char** argv, Options& options) {
    Config& c = options.config;
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (std::strcmp(arg, "--help") == 0) {
            options.help = true;
        } else if (std::strcmp(arg, "--csv") == 0) {
            options.csv = true;
        } else if (std::strcmp(arg, "--csv-header") == 0) {
            options.csvHeader = true;
        } else if (std::strcmp(arg, "--profile") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Error: option '--profile' requires a value\n");
                return false;
            }
            i++;
            if (std::strcmp(argv[i], "smooth") == 0) {
                c.profile = Profile::Smooth;
            } else if (std::strcmp(argv[i], "pilot") == 0) {
                c.profile = Profile::Pilot;
            } else {
                std::fprintf(stderr, "Error: --profile must be 'smooth' or 'pilot', got '%s'\n",
                             argv[i]);
                return false;
            }
        } else {
            // The other parameters take one number.
            double* target = nullptr;
            double seedValue = 0.0;
            if (std::strcmp(arg, "--rate") == 0) target = &c.rate;
            else if (std::strcmp(arg, "--warp") == 0) target = &c.warp;
            else if (std::strcmp(arg, "--loss") == 0) target = &c.lossPercent;
            else if (std::strcmp(arg, "--latency") == 0) target = &c.latencyMs;
            else if (std::strcmp(arg, "--jitter") == 0) target = &c.jitterMs;
            else if (std::strcmp(arg, "--delay") == 0) target = &c.delayMs;
            else if (std::strcmp(arg, "--duration") == 0) target = &c.duration;
            else if (std::strcmp(arg, "--seed") == 0) target = &seedValue;
            else {
                std::fprintf(stderr, "Error: unknown argument '%s'\n", arg);
                return false;
            }

            if (i + 1 >= argc) {
                std::fprintf(stderr, "Error: option '%s' requires a value\n", arg);
                return false;
            }
            i++;
            if (!parseNumber(argv[i], *target)) {
                std::fprintf(stderr, "Error: invalid number '%s' for option '%s'\n", argv[i], arg);
                return false;
            }
            if (target == &seedValue) {
                if (seedValue < 0.0 || seedValue > 4294967295.0 || seedValue != std::floor(seedValue)) {
                    std::fprintf(stderr, "Error: --seed must be an integer from 0 to 4294967295\n");
                    return false;
                }
                c.seed = static_cast<unsigned>(seedValue);
            }
        }
    }

    // Checking allowed ranges.
    if (c.rate <= 0.0 || c.warp <= 0.0 || c.duration <= kWarmupSim) {
        std::fprintf(stderr, "Error: --rate and --warp must be positive, --duration must exceed %g\n",
                     kWarmupSim);
        return false;
    }
    if (c.lossPercent < 0.0 || c.lossPercent > 100.0) {
        std::fprintf(stderr, "Error: --loss must be between 0 and 100\n");
        return false;
    }
    if (c.latencyMs < 0.0 || c.jitterMs < 0.0 || c.delayMs < 0.0) {
        std::fprintf(stderr, "Error: --latency, --jitter and --delay must not be negative\n");
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parseArgs(argc, argv, options)) {
        std::fprintf(stderr, "\n");
        printUsage();
        return 2;
    }
    if (options.help) {
        printUsage();
        return 0;
    }
    if (options.csvHeader) {
        std::printf("%s\n", kCsvHeader);
        return 0;
    }

    const Config& config = options.config;
    Trajectory truth(config.duration, config.profile);
    std::vector<Packet> packets = simulateNetwork(truth, config);

    Result results[kMethodCount];
    if (!runClient(config, truth, packets, results)) {
        std::fprintf(stderr, "Error: no frames to measure (no packets arrived or delay is too long "
                             "for this duration)\n");
        return 1;
    }

    if (options.csv) {
        printCsv(config, results);
    } else {
        printTable(config, results);
    }
    return 0;
}
