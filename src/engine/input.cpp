// Input mapper – port of 0x411f70..0x412400.
//
// Two devices (keyboard = GetKeyboardState, joystick = joyGetPosEx) each keep
// two 256 byte state buffers that are swapped on every update.  64 virtual
// buttons own a list of (device, code) bindings; a virtual button is 0xff
// when any bound code is held.  The virtual button buffers are 255 bytes
// apart (0x4200c0 / 0x4201bf) exactly like the original.
#include "engine.h"
#include "platform.h"
#include "testing.h"

#include <cstring>
#include <vector>

namespace {

struct Device {
    uint8_t buf[2][256] = {};
    int cur = 0;      // +0x204
    bool ok = false;
    bool joystick = false;

    void update()
    {
        if (!ok)
            return;
        cur ^= 1;
        if (joystick) {
            // The original only polled a joystick found at start-up and kept
            // the stale buffer when joyGetPosEx failed; the port polls every
            // frame (hot plug) and reports "nothing held" while unplugged.
            if (!plat_joystick_state(buf[cur]))
                std::memset(buf[cur], 0, 256);
        } else {
            plat_keyboard_state(buf[cur]);
            test_apply_keyboard(buf[cur]);
        }
    }
    // vtbl+0x08 (0x410c90)
    bool held(int code) const { return (buf[cur][code & 0xff] >> 7) != 0; }
};

Device g_dev[2];                         // 0x4202c0 / 0x4202c4
struct Binding { int device, code; };
std::vector<Binding> g_bind[64];         // 0x4202d4 (64 lists of 12 bytes)
uint8_t g_state[0xff + 256];             // 0x4200c0 (two buffers 0xff apart)
int g_idx = 0;                           // 0x4205d0

inline uint8_t* cur_buf() { return &g_state[0xff * g_idx]; }
inline uint8_t* prev_buf() { return &g_state[0xff * (g_idx ^ 1)]; }

} // namespace

bool in_init()
{
    std::memset(g_state, 0, sizeof(g_state));
    g_idx = 0;
    g_dev[0] = Device();
    g_dev[0].ok = true;           // keyboard always succeeds
    g_dev[1] = Device();
    g_dev[1].joystick = true;
    g_dev[1].ok = true;
    return true;
}

void in_shutdown()
{
    for (auto& b : g_bind)
        b.clear();
}

void in_bind(int device, int button, int code)
{
    if (button >= 0 && button < 64 && (device == 0 || device == 1))
        g_bind[button].push_back({device, code});
}

void in_update()
{
    for (auto& d : g_dev)
        d.update();
    g_idx ^= 1;
    std::memset(cur_buf(), 0, 256);
    for (int b = 0; b < 64; ++b) {
        for (const Binding& bd : g_bind[b]) {
            const Device& d = g_dev[bd.device];
            if (d.ok && d.held(bd.code)) {
                cur_buf()[b] = 0xff;
                break;
            }
        }
    }
}

int in_held(int button) { return cur_buf()[button] != 0; }

bool in_pressed(int button)
{
    if (!(cur_buf()[button] & 0x80))
        return false;
    return !(prev_buf()[button] & 0x80);
}

void in_clear() { std::memset(cur_buf(), 0, 256); }

void in_force(int button) { cur_buf()[button] = 0x80; }
