// Orbiter's key layout and the vessel control keys the player holds (stage 3, decision 35).
//
// Orbiter reads its layout from keymap.cfg in the Orbiter folder (Src/Orbiter/Keymap.cpp), but the
// SDK does not give it to plugins. The client needs it twice:
// - to turn held keys into INPUT actions (PROTOCOL.md, section 7): INPUT carries actions, not key
//   codes, so the layout on the client may differ from the host;
// - to clear the vessel control keys in the key state before Orbiter applies them locally: the
//   client's vessels are flown by the host. Only these keys are cleared - the camera keeps working.
//
// The file has lines "Name = KEY [MODIFIERS]", for example "IncMainThrust = ADD CTRL". Key names
// are Orbiter's (ESC, A, NUMPAD2, PGUP, ...), their codes are DirectInput scan codes - the same
// numbers as OAPI_KEY_* in the SDK. The key state is Orbiter's 256-byte array: a byte with the
// 0x80 bit set means the key is down.
// Orbiter is not needed here, so the layout is tested on any system.
#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace olc {

// Modifiers of a binding, the same bits as KMOD_* in Orbiter's Keymap.h. A binding with SHIFT
// accepts either Shift key, with LSHIFT only the left one; the same for Ctrl and Alt.
constexpr uint16_t kModLShift = 0x0100;
constexpr uint16_t kModRShift = 0x0200;
constexpr uint16_t kModLCtrl = 0x0400;
constexpr uint16_t kModRCtrl = 0x0800;
constexpr uint16_t kModLAlt = 0x1000;
constexpr uint16_t kModRAlt = 0x2000;

// One logical key: the key code and the modifiers that must be held with it.
struct KeyBinding {
    uint8_t key = 0;    // scan code; 0 - not bound
    uint16_t mods = 0;  // kMod* bits
};

class Keymap {
public:
    // Orbiter's default layout for the keys OLC uses (in case keymap.cfg cannot be read).
    Keymap();

    // Reads the text of keymap.cfg. Lines that are not "Name = KEY [MODIFIERS]" with a known key
    // are skipped. Returns how many bindings were read.
    int parse(const std::string& text);

    // The binding of a logical key (Orbiter's name, e.g. "RCSPitchUp"); key 0 if there is none.
    KeyBinding binding(const std::string& name) const;

    // Whether the logical key is held: its key is down and exactly its modifiers are held - the
    // same rule as Orbiter's Keymap::IsLogicalKey (an extra Ctrl, Shift or Alt means "not held").
    bool isHeld(const std::string& name, const char kstate[256]) const;

    // INPUT.keys bits of the vessel control keys held now.
    uint32_t inputKeys(const char kstate[256]) const;

    // Clears in kstate the keys of the vessel controls held now. Modifier keys are not cleared.
    void clearControlKeys(char kstate[256]) const;

private:
    std::map<std::string, KeyBinding> bindings_;
};

// Scan code of a key by Orbiter's name; 0 if the name is unknown. Case does not matter.
uint8_t keyCodeByName(const std::string& name);

}  // namespace olc
