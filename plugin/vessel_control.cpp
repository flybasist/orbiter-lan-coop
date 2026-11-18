#include "vessel_control.h"

#include <algorithm>
#include <string>

namespace olcplugin {

namespace {

// A key bit and the thruster group it fires.
struct KeyGroup {
    uint32_t bit;
    THGROUP_TYPE group;
};

const KeyGroup kRotation[] = {
    {olc::kInputPitchUp, THGROUP_ATT_PITCHUP},   {olc::kInputPitchDown, THGROUP_ATT_PITCHDOWN},
    {olc::kInputYawLeft, THGROUP_ATT_YAWLEFT},   {olc::kInputYawRight, THGROUP_ATT_YAWRIGHT},
    {olc::kInputBankLeft, THGROUP_ATT_BANKLEFT}, {olc::kInputBankRight, THGROUP_ATT_BANKRIGHT},
};

const KeyGroup kTranslation[] = {
    {olc::kInputUp, THGROUP_ATT_UP},           {olc::kInputDown, THGROUP_ATT_DOWN},
    {olc::kInputLeft, THGROUP_ATT_LEFT},       {olc::kInputRight, THGROUP_ATT_RIGHT},
    {olc::kInputForward, THGROUP_ATT_FORWARD}, {olc::kInputBack, THGROUP_ATT_BACK},
};

// Joystick axis pairs: a positive value fires the first group, a negative one the second
// (PROTOCOL.md, INPUT: rot - pitch up, yaw left, bank left are positive; lin - up, left, forward).
const THGROUP_TYPE kRotationAxes[3][2] = {
    {THGROUP_ATT_PITCHUP, THGROUP_ATT_PITCHDOWN},
    {THGROUP_ATT_YAWLEFT, THGROUP_ATT_YAWRIGHT},
    {THGROUP_ATT_BANKLEFT, THGROUP_ATT_BANKRIGHT},
};
const THGROUP_TYPE kTranslationAxes[3][2] = {
    {THGROUP_ATT_UP, THGROUP_ATT_DOWN},
    {THGROUP_ATT_LEFT, THGROUP_ATT_RIGHT},
    {THGROUP_ATT_FORWARD, THGROUP_ATT_BACK},
};

// RCS modes of VESSEL::GetAttitudeMode.
constexpr int kRcsRotation = 1;
constexpr int kRcsTranslation = 2;

// How fast the throttle and hover levels change while the key is held, per second (as the core).
constexpr double kThrottleRate = 0.2;
// How fast the elevator trim changes, per second (as the core, Vessel::IncTrim).
constexpr double kTrimRate = 0.25;

void fireKeys(VESSEL* vessel, uint32_t keys, const KeyGroup* groups, double level) {
    for (int i = 0; i < 6; i++) {
        if (keys & groups[i].bit) {
            vessel->IncThrusterGroupLevel_SingleStep(groups[i].group, level);
        }
    }
}

void fireAxes(VESSEL* vessel, const float* axes, const THGROUP_TYPE pairs[3][2]) {
    for (int i = 0; i < 3; i++) {
        const double value = axes[i];
        if (value > 0.0) {
            vessel->IncThrusterGroupLevel_SingleStep(pairs[i][0], value);
        } else if (value < 0.0) {
            vessel->IncThrusterGroupLevel_SingleStep(pairs[i][1], -value);
        }
    }
}

// The core's IncMainRetroLevel: "more throttle" first takes the retro engines down to zero.
void changeMainRetro(VESSEL* vessel, double delta) {
    const double main = vessel->GetThrusterGroupLevel(THGROUP_MAIN);
    const double retro = vessel->GetThrusterGroupLevel(THGROUP_RETRO);
    if (delta > 0.0) {
        if (retro > 0.0) {
            vessel->SetThrusterGroupLevel(THGROUP_RETRO, (std::max)(retro - delta, 0.0));
            vessel->SetThrusterGroupLevel(THGROUP_MAIN, 0.0);
        } else {
            vessel->SetThrusterGroupLevel(THGROUP_MAIN, (std::min)(main + delta, 1.0));
        }
    } else {
        if (main > 0.0) {
            vessel->SetThrusterGroupLevel(THGROUP_MAIN, (std::max)(main + delta, 0.0));
            vessel->SetThrusterGroupLevel(THGROUP_RETRO, 0.0);
        } else {
            vessel->SetThrusterGroupLevel(THGROUP_RETRO, (std::min)(retro - delta, 1.0));
        }
    }
}

}  // namespace

void applyInput(VESSEL* vessel, const olc::Input& input, double simdt) {
    const uint32_t keys = input.keys;

    // RCS: keys and joystick, by the vessel's RCS mode.
    const double level = (keys & olc::kInputLowPowerRcs) ? 0.1 : 1.0;
    const int mode = vessel->GetAttitudeMode();
    if (mode == kRcsRotation) {
        fireKeys(vessel, keys, kRotation, level);
        fireAxes(vessel, input.rot, kRotationAxes);
    } else if (mode == kRcsTranslation) {
        fireKeys(vessel, keys, kTranslation, level);
        fireAxes(vessel, input.lin, kTranslationAxes);
    }

    // Main and retro engines.
    if (keys & olc::kInputFullMain) {
        vessel->IncThrusterGroupLevel_SingleStep(THGROUP_MAIN, 1.0);
    }
    if (keys & olc::kInputFullRetro) {
        vessel->IncThrusterGroupLevel_SingleStep(THGROUP_RETRO, 1.0);
    }
    if (keys & olc::kInputKillMainRetro) {
        vessel->SetThrusterGroupLevel(THGROUP_MAIN, 0.0);
        vessel->SetThrusterGroupLevel(THGROUP_RETRO, 0.0);
    }
    if (keys & olc::kInputThrottleUp) {
        changeMainRetro(vessel, kThrottleRate * simdt);
    }
    if (keys & olc::kInputThrottleDown) {
        changeMainRetro(vessel, -kThrottleRate * simdt);
    }
    if (input.throttle >= 0.0f) {
        vessel->SetThrusterGroupLevel(THGROUP_MAIN, input.throttle);  // a throttle lever
    }

    // Hover engines.
    if (keys & olc::kInputHoverUp) {
        vessel->IncThrusterGroupLevel(THGROUP_HOVER, kThrottleRate * simdt);
    }
    if (keys & olc::kInputHoverDown) {
        vessel->IncThrusterGroupLevel(THGROUP_HOVER, -kThrottleRate * simdt);
    }

    // Elevator trim. SetControlSurfaceLevel with direct = false moves the trim smoothly, as the
    // core does for the focus vessel.
    if (keys & (olc::kInputTrimUp | olc::kInputTrimDown)) {
        const double step = (keys & olc::kInputTrimUp) ? kTrimRate * simdt : -kTrimRate * simdt;
        const double trim = vessel->GetControlSurfaceLevel(AIRCTRL_ELEVATORTRIM) + step;
        vessel->SetControlSurfaceLevel(AIRCTRL_ELEVATORTRIM, (std::max)(-1.0, (std::min)(1.0, trim)),
                                       false);
    }

    // Wheel brakes: for this step only (permanent = false), as the core.
    if (keys & olc::kInputWheelBrakeLeft) {
        vessel->SetWheelbrakeLevel(1.0, 1, false);
    }
    if (keys & olc::kInputWheelBrakeRight) {
        vessel->SetWheelbrakeLevel(1.0, 2, false);
    }
}

void applyKey(VESSEL* vessel, const olc::KeyPress& press) {
    // The vessel's key handlers check the modifiers in the key state, so the state holds them.
    char kstate[256] = {};
    if (press.mods & 0x01) {
        kstate[OAPI_KEY_LSHIFT] = static_cast<char>(0x80);
    }
    if (press.mods & 0x02) {
        kstate[OAPI_KEY_LCONTROL] = static_cast<char>(0x80);
    }
    if (press.mods & 0x04) {
        kstate[OAPI_KEY_LALT] = static_cast<char>(0x80);
    }
    kstate[press.key] = static_cast<char>(0x80);
    const int consumed = vessel->SendBufferedKey(press.key, true, kstate);
    // Autopilot modes after the key (NAVMODE_* are 1...7), to see in the log what the key did.
    std::string modes;
    for (int mode = NAVMODE_KILLROT; mode <= NAVMODE_ANTINORMAL; mode++) {
        if (vessel->GetNavmodeState(mode)) {
            modes += " " + std::to_string(mode);
        }
    }
    oapiWriteLogV("OLC host: key 0x%02X mods %d for '%s': %s, autopilot modes on:%s", press.key,
                  press.mods, vessel->GetName(),
                  consumed ? "taken by the vessel" : "passed to the core handler",
                  modes.empty() ? " none" : modes.c_str());
}

}  // namespace olcplugin
