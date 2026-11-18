// Tests of Keymap: Orbiter's key layout, modifiers and INPUT actions.
#include <cstring>
#include <string>

#include "check.h"
#include "keymap.h"
#include "messages.h"

namespace {

// Scan codes used in the tests (the same as OAPI_KEY_* in the SDK).
constexpr uint8_t kNumpad2 = 0x50;
constexpr uint8_t kNumpad8 = 0x48;
constexpr uint8_t kAdd = 0x4E;
constexpr uint8_t kLCtrl = 0x1D;
constexpr uint8_t kRCtrl = 0x9D;
constexpr uint8_t kLShift = 0x2A;
constexpr uint8_t kKeyJ = 0x24;

struct KeyState {
    char keys[256];
    KeyState() { std::memset(keys, 0, sizeof(keys)); }
    void press(uint8_t key) { keys[key] = static_cast<char>(0x80); }
};

void testKeyNames() {
    CHECK(olc::keyCodeByName("NUMPAD2") == kNumpad2);
    CHECK(olc::keyCodeByName("numpad2") == kNumpad2);  // case does not matter
    CHECK(olc::keyCodeByName("ESC") == 0x01);
    CHECK(olc::keyCodeByName("PGUP") == 0xC9);
    CHECK(olc::keyCodeByName("NOSUCHKEY") == 0);
}

void testDefaults() {
    olc::Keymap keymap;
    KeyState state;
    state.press(kNumpad2);
    // Numpad2 is both "pitch up" and "translate up": both bits go, the host picks by RCS mode.
    CHECK(keymap.inputKeys(state.keys) == (olc::kInputPitchUp | olc::kInputUp));

    // With Ctrl it is the low-power variant, and the full-power one is no longer held.
    state.press(kLCtrl);
    CHECK(keymap.inputKeys(state.keys) ==
          (olc::kInputPitchUp | olc::kInputUp | olc::kInputLowPowerRcs));
    CHECK(!keymap.isHeld("RCSPitchUp", state.keys));

    // An extra Shift: no binding matches (the same rule as Orbiter's IsLogicalKey).
    state.press(kLShift);
    CHECK(keymap.inputKeys(state.keys) == 0);
}

void testParse() {
    olc::Keymap keymap;
    const std::string text =
        "RCSPitchUp = NUMPAD8\n"
        "IncMainThrust = J RCTRL\n"
        "Broken line without equals\n"
        "RCSPitchDown = NOSUCHKEY\n";
    CHECK(keymap.parse(text) == 2);
    CHECK(keymap.binding("RCSPitchUp").key == kNumpad8);
    CHECK(keymap.binding("RCSPitchDown").key == kNumpad8);  // unknown key: the default stays

    KeyState state;
    state.press(kKeyJ);
    state.press(kRCtrl);
    CHECK(keymap.isHeld("IncMainThrust", state.keys));
    KeyState left;  // RCTRL means the right Ctrl only
    left.press(kKeyJ);
    left.press(kLCtrl);
    CHECK(!keymap.isHeld("IncMainThrust", left.keys));
}

void testClearControlKeys() {
    olc::Keymap keymap;
    KeyState state;
    state.press(kAdd);       // full main
    state.press(kNumpad2);   // pitch up
    state.press(0xC8);       // UP arrow - a camera key, not a control
    keymap.clearControlKeys(state.keys);
    CHECK(state.keys[kAdd] == 0);
    CHECK(state.keys[kNumpad2] == 0);
    CHECK(state.keys[0xC8] != 0);

    // Modifiers stay: the camera uses Ctrl and Alt too.
    KeyState low;
    low.press(kNumpad2);
    low.press(kLCtrl);
    keymap.clearControlKeys(low.keys);
    CHECK(low.keys[kNumpad2] == 0);
    CHECK(low.keys[kLCtrl] != 0);
}

}  // namespace

int main() {
    testKeyNames();
    testDefaults();
    testParse();
    testClearControlKeys();
    return checkResult();
}
