#include "keymap.h"

#include <cctype>
#include <sstream>

#include "messages.h"

namespace olc {

namespace {

// Orbiter's key names and their scan codes (Src/Orbiter/Keymap.cpp, keyname; codes - OAPI_KEY_*).
struct KeyName {
    const char* name;
    uint8_t code;
};
const KeyName kKeyNames[] = {
    {"ESC", 0x01},
    {"1", 0x02},
    {"2", 0x03},
    {"3", 0x04},
    {"4", 0x05},
    {"5", 0x06},
    {"6", 0x07},
    {"7", 0x08},
    {"8", 0x09},
    {"9", 0x0A},
    {"0", 0x0B},
    {"MINUS", 0x0C},
    {"EQUALS", 0x0D},
    {"BACK", 0x0E},
    {"TAB", 0x0F},
    {"Q", 0x10},
    {"W", 0x11},
    {"E", 0x12},
    {"R", 0x13},
    {"T", 0x14},
    {"Y", 0x15},
    {"U", 0x16},
    {"I", 0x17},
    {"O", 0x18},
    {"P", 0x19},
    {"LBRACKET", 0x1A},
    {"RBRACKET", 0x1B},
    {"RETURN", 0x1C},
    {"A", 0x1E},
    {"S", 0x1F},
    {"D", 0x20},
    {"F", 0x21},
    {"G", 0x22},
    {"H", 0x23},
    {"J", 0x24},
    {"K", 0x25},
    {"L", 0x26},
    {"SEMICOLON", 0x27},
    {"APOSTROPHE", 0x28},
    {"GRAVE", 0x29},
    {"BACKSLASH", 0x2B},
    {"Z", 0x2C},
    {"X", 0x2D},
    {"C", 0x2E},
    {"V", 0x2F},
    {"B", 0x30},
    {"N", 0x31},
    {"M", 0x32},
    {"COMMA", 0x33},
    {"PERIOD", 0x34},
    {"SLASH", 0x35},
    {"MULTIPLY", 0x37},
    {"SPACE", 0x39},
    {"CAPITAL", 0x3A},
    {"F1", 0x3B},
    {"F2", 0x3C},
    {"F3", 0x3D},
    {"F4", 0x3E},
    {"F5", 0x3F},
    {"F6", 0x40},
    {"F7", 0x41},
    {"F8", 0x42},
    {"F9", 0x43},
    {"F10", 0x44},
    {"NUMLOCK", 0x45},
    {"SCROLL", 0x46},
    {"NUMPAD7", 0x47},
    {"NUMPAD8", 0x48},
    {"NUMPAD9", 0x49},
    {"SUBTRACT", 0x4A},
    {"NUMPAD4", 0x4B},
    {"NUMPAD5", 0x4C},
    {"NUMPAD6", 0x4D},
    {"ADD", 0x4E},
    {"NUMPAD1", 0x4F},
    {"NUMPAD2", 0x50},
    {"NUMPAD3", 0x51},
    {"NUMPAD0", 0x52},
    {"DECIMAL", 0x53},
    {"OEM_102", 0x56},
    {"F11", 0x57},
    {"F12", 0x58},
    {"NUMPADENTER", 0x9C},
    {"DIVIDE", 0xB5},
    {"SYSRQ", 0xB7},
    {"HOME", 0xC7},
    {"UP", 0xC8},
    {"PGUP", 0xC9},
    {"LEFT", 0xCB},
    {"RIGHT", 0xCD},
    {"END", 0xCF},
    {"DOWN", 0xD0},
    {"PGDOWN", 0xD1},
    {"INSERT", 0xD2},
    {"DELETE", 0xD3},
};

// Scan codes of the modifier keys (OAPI_KEY_LSHIFT and others).
constexpr uint8_t kKeyLShift = 0x2A;
constexpr uint8_t kKeyRShift = 0x36;
constexpr uint8_t kKeyLCtrl = 0x1D;
constexpr uint8_t kKeyRCtrl = 0x9D;
constexpr uint8_t kKeyLAlt = 0x38;
constexpr uint8_t kKeyRAlt = 0xB8;

// The vessel control keys OLC sends in INPUT: Orbiter's logical key name and the INPUT.keys bits.
// The rotation and translation RCS keys are the same keys in Orbiter (the RCS mode decides which
// one works), so both bits are sent and the host picks by the mode of its vessel.
struct ControlKey {
    const char* name;
    uint32_t bits;
};
const ControlKey kControlKeys[] = {
    {"RCSPitchUp", kInputPitchUp},
    {"RCSPitchDown", kInputPitchDown},
    {"RCSYawLeft", kInputYawLeft},
    {"RCSYawRight", kInputYawRight},
    {"RCSBankLeft", kInputBankLeft},
    {"RCSBankRight", kInputBankRight},
    {"RCSUp", kInputUp},
    {"RCSDown", kInputDown},
    {"RCSLeft", kInputLeft},
    {"RCSRight", kInputRight},
    {"RCSForward", kInputForward},
    {"RCSBack", kInputBack},
    {"LPRCSPitchUp", kInputPitchUp | kInputLowPowerRcs},
    {"LPRCSPitchDown", kInputPitchDown | kInputLowPowerRcs},
    {"LPRCSYawLeft", kInputYawLeft | kInputLowPowerRcs},
    {"LPRCSYawRight", kInputYawRight | kInputLowPowerRcs},
    {"LPRCSBankLeft", kInputBankLeft | kInputLowPowerRcs},
    {"LPRCSBankRight", kInputBankRight | kInputLowPowerRcs},
    {"LPRCSUp", kInputUp | kInputLowPowerRcs},
    {"LPRCSDown", kInputDown | kInputLowPowerRcs},
    {"LPRCSLeft", kInputLeft | kInputLowPowerRcs},
    {"LPRCSRight", kInputRight | kInputLowPowerRcs},
    {"LPRCSForward", kInputForward | kInputLowPowerRcs},
    {"LPRCSBack", kInputBack | kInputLowPowerRcs},
    {"OverrideFullMainThrust", kInputFullMain},
    {"OverrideFullRetroThrust", kInputFullRetro},
    {"KillMainRetroThrust", kInputKillMainRetro},
    {"IncMainThrust", kInputThrottleUp},
    {"DecMainThrust", kInputThrottleDown},
    {"IncHoverThrust", kInputHoverUp},
    {"DecHoverThrust", kInputHoverDown},
    {"IncElevatorTrim", kInputTrimUp},
    {"DecElevatorTrim", kInputTrimDown},
    {"WheelbrakeLeft", kInputWheelBrakeLeft},
    {"WheelbrakeRight", kInputWheelBrakeRight},
};

// Orbiter's default layout for these keys (Src/Orbiter/Keymap.cpp, lkeyspec).
struct DefaultKey {
    const char* name;
    uint8_t key;
    uint16_t mods;
};
constexpr uint16_t kModCtrl = kModLCtrl | kModRCtrl;
const DefaultKey kDefaults[] = {
    {"RCSPitchUp", 0x50, 0},     {"RCSPitchDown", 0x48, 0},   {"RCSYawLeft", 0x4F, 0},
    {"RCSYawRight", 0x51, 0},    {"RCSBankLeft", 0x4B, 0},    {"RCSBankRight", 0x4D, 0},
    {"RCSUp", 0x50, 0},          {"RCSDown", 0x48, 0},        {"RCSLeft", 0x4F, 0},
    {"RCSRight", 0x51, 0},       {"RCSForward", 0x4D, 0},     {"RCSBack", 0x49, 0},
    {"LPRCSPitchUp", 0x50, kModCtrl},   {"LPRCSPitchDown", 0x48, kModCtrl},
    {"LPRCSYawLeft", 0x4F, kModCtrl},   {"LPRCSYawRight", 0x51, kModCtrl},
    {"LPRCSBankLeft", 0x4B, kModCtrl},  {"LPRCSBankRight", 0x4D, kModCtrl},
    {"LPRCSUp", 0x50, kModCtrl},        {"LPRCSDown", 0x48, kModCtrl},
    {"LPRCSLeft", 0x4F, kModCtrl},      {"LPRCSRight", 0x51, kModCtrl},
    {"LPRCSForward", 0x4D, kModCtrl},   {"LPRCSBack", 0x49, kModCtrl},
    {"OverrideFullMainThrust", 0x4E, 0},  {"OverrideFullRetroThrust", 0x4A, 0},
    {"KillMainRetroThrust", 0x37, 0},     {"IncMainThrust", 0x4E, kModCtrl},
    {"DecMainThrust", 0x4A, kModCtrl},    {"IncHoverThrust", 0x52, 0},
    {"DecHoverThrust", 0x53, 0},          {"IncElevatorTrim", 0xD3, 0},
    {"DecElevatorTrim", 0xD2, 0},         {"WheelbrakeLeft", 0x33, 0},
    {"WheelbrakeRight", 0x34, 0},
};

bool down(const char kstate[256], uint8_t key) {
    return (static_cast<unsigned char>(kstate[key]) & 0x80) != 0;
}

// One modifier pair (left and right key): the same check as Orbiter's IsMatchingModifier.
bool modifierMatches(const char kstate[256], uint16_t mods, uint16_t leftMod, uint16_t rightMod,
                     uint8_t leftKey, uint8_t rightKey) {
    const uint16_t wanted = mods & (leftMod | rightMod);
    const bool left = down(kstate, leftKey);
    const bool right = down(kstate, rightKey);
    if (wanted == 0) {
        return !left && !right;  // not wanted - must not be held
    }
    if (wanted == (leftMod | rightMod)) {
        return left || right;
    }
    return wanted == leftMod ? left : right;
}

std::string upper(const std::string& text) {
    std::string result = text;
    for (char& c : result) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return result;
}

}  // namespace

uint8_t keyCodeByName(const std::string& name) {
    const std::string wanted = upper(name);
    for (const KeyName& key : kKeyNames) {
        if (wanted == key.name) {
            return key.code;
        }
    }
    return 0;
}

Keymap::Keymap() {
    for (const DefaultKey& key : kDefaults) {
        bindings_[key.name] = KeyBinding{key.key, key.mods};
    }
}

int Keymap::parse(const std::string& text) {
    int count = 0;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        const size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        std::istringstream name(line.substr(0, equals));
        std::istringstream words(line.substr(equals + 1));
        std::string item;
        std::string keyName;
        if (!(name >> item) || !(words >> keyName)) {
            continue;
        }
        KeyBinding binding;
        binding.key = keyCodeByName(keyName);
        if (binding.key == 0) {
            continue;
        }
        std::string mod;
        while (words >> mod) {
            mod = upper(mod);
            if (mod == "LSHIFT") binding.mods |= kModLShift;
            else if (mod == "RSHIFT") binding.mods |= kModRShift;
            else if (mod == "SHIFT") binding.mods |= kModLShift | kModRShift;
            else if (mod == "LCTRL") binding.mods |= kModLCtrl;
            else if (mod == "RCTRL") binding.mods |= kModRCtrl;
            else if (mod == "CTRL") binding.mods |= kModLCtrl | kModRCtrl;
            else if (mod == "LALT") binding.mods |= kModLAlt;
            else if (mod == "RALT") binding.mods |= kModRAlt;
            else if (mod == "ALT") binding.mods |= kModLAlt | kModRAlt;
        }
        bindings_[item] = binding;
        count++;
    }
    return count;
}

KeyBinding Keymap::binding(const std::string& name) const {
    auto found = bindings_.find(name);
    return found == bindings_.end() ? KeyBinding{} : found->second;
}

bool Keymap::isHeld(const std::string& name, const char kstate[256]) const {
    const KeyBinding key = binding(name);
    if (key.key == 0 || !down(kstate, key.key)) {
        return false;
    }
    return modifierMatches(kstate, key.mods, kModLCtrl, kModRCtrl, kKeyLCtrl, kKeyRCtrl) &&
           modifierMatches(kstate, key.mods, kModLShift, kModRShift, kKeyLShift, kKeyRShift) &&
           modifierMatches(kstate, key.mods, kModLAlt, kModRAlt, kKeyLAlt, kKeyRAlt);
}

uint32_t Keymap::inputKeys(const char kstate[256]) const {
    uint32_t keys = 0;
    for (const ControlKey& control : kControlKeys) {
        if (isHeld(control.name, kstate)) {
            keys |= control.bits;
        }
    }
    return keys;
}

void Keymap::clearControlKeys(char kstate[256]) const {
    // First find all held controls, then clear: two controls may share a key (RCSPitchUp and
    // RCSUp), and clearing the key at the first one would hide the second.
    bool clear[256] = {};
    for (const ControlKey& control : kControlKeys) {
        if (isHeld(control.name, kstate)) {
            clear[binding(control.name).key] = true;
        }
    }
    for (int key = 0; key < 256; key++) {
        if (clear[key]) {
            kstate[key] = 0;
        }
    }
}

}  // namespace olc
