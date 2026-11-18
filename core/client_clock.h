// Client display clock: which moment of host sim time to display now.
//
// The client shows the world with a small delay (docs/SMOOTHING.md): displayDelay seconds
// of real time behind the host, that is displayDelay × warp seconds of sim time.
// Then there is always something to interpolate between two WORLD_STATE packets.
//
// How the clock works:
// - between packets display time runs by itself: real time × host warp (stopped
//   while paused);
// - each packet tells where the host was when it was received; from that we get where the
//   clock should be (target = host time − displayDelay × warp);
// - a small error is corrected smoothly, by the fraction kClockGain per packet, so that network
//   delay jitter does not shake the picture;
// - a large error (first packet, a time jump on the host) is corrected at once, with a jump.
// All times are in seconds; now is the client's real time (oapiGetSysTime).
#pragma once

namespace olc {

// Default display delay, seconds of real time: for a wired LAN with 60 states per second
// (decision 36, measured on the test rigs). The smoothing model chose 0.1 s for a network with
// jitter (SMOOTHING.md); over Wi-Fi a larger delay is set in the settings.
constexpr double kDefaultDisplayDelay = 0.03;

// Which fraction of the error the clock corrects per packet. At 30 packets per second the error
// halves in about 0.5 s, at 60 - in about 0.25 s.
constexpr double kClockGain = 0.05;

// An error larger than this (seconds of real time, i.e. × warp in sim time)
// is corrected with a jump.
constexpr double kClockJumpThreshold = 1.0;

class ClientClock {
public:
    explicit ClientClock(double displayDelay = kDefaultDisplayDelay);

    // A packet arrived: the host was at sim time hostSimTime with warp and paused,
    // the client received the packet at real time now. Out-of-order packets may also
    // be passed: a late packet gives a target slightly behind, and the clock barely moves.
    void onPacket(double hostSimTime, double warp, bool paused, double now);

    // Host sim time to display at moment now. Before the first packet - 0.
    // now must not go backwards; if it does, display time does not change.
    double displayTime(double now);

    // Whether at least one packet has arrived.
    bool started() const { return started_; }

    // How many times the clock jumped (the tests check the jump rule by it).
    int jumps() const { return jumps_; }

private:
    // Moves display time up to moment now at the current rate.
    void advance(double now);

    double displayDelay_;
    bool started_ = false;
    double displayTime_ = 0.0;  // host sim time displayed at lastNow_
    double lastNow_ = 0.0;      // real time of the last advance
    double rate_ = 1.0;         // running rate: host warp, 0 while paused
    int jumps_ = 0;
};

}  // namespace olc
