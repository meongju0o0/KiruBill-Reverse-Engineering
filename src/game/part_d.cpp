// Part D: original functions 0x408920 .. 0x40aae0
//
// Contents: falling-leaf particles, score counter, slash effect, the opening
// "用意... 始め" camera drop, the scrolling story/credits text reader, small
// debris / projectile helpers, the moon boss (surface 45) state machine and
// the ending image fade-in.
//
// Every function is a one to one translation of the original machine code;
// comments give the original address.
#include "game.h"

#include <vector>

// (functions of other parts are declared in game.h)

namespace {

// score += v, clamped to the maximum score (GS+0x3c).  32-bit wrap-around
// like the original.
inline void add_score(uint32_t v)
{
    MI32(GS + 0x10) = (int32_t)((uint32_t)MI32(GS + 0x10) + v);
    if (MI32(GS + 0x10) > MI32(GS + 0x3c))
        MI32(GS + 0x10) = MI32(GS + 0x3c);
}

// Draws the moon picture (surface 45 = data/008.dat) at the task's y.
inline void draw_moon(Task* t)
{
    gfx_blit(0x2d, -MI32(GS + 0), TI32(t, 0x20) - MI32(GS + 8), 640, 480, 0, 0);
}

// MSVC text mode stream ("r"): CR LF -> LF, Ctrl-Z ends the file.  getc()
// returns -1 at the end, which the original only looks at as the byte 0xff.
struct TextStream {
    std::vector<uint8_t> buf;
    size_t pos = 0;
    bool open(const char* path)
    {
        FILE* fp = std::fopen(path, "rb");
        if (!fp)
            return false;
        std::vector<uint8_t> raw;
        uint8_t tmp[4096];
        size_t n;
        while ((n = std::fread(tmp, 1, sizeof(tmp), fp)) > 0)
            raw.insert(raw.end(), tmp, tmp + n);
        std::fclose(fp);
        for (size_t i = 0; i < raw.size(); ++i) {
            uint8_t c = raw[i];
            if (c == 0x1a)
                break;
            if (c == 0x0d && i + 1 < raw.size() && raw[i + 1] == 0x0a)
                continue;
            buf.push_back(c);
        }
        return true;
    }
    int getc() { return pos < buf.size() ? buf[pos++] : -1; }
};

} // namespace

// ---------------------------------------------------------------------------
// 0x408920: spawn one falling leaf/petal particle at a random x above the view.
void f_408920()
{
    Task d; task_desc(&d);
    int32_t y = MI32(GS + 8) - 20;
    TI32(&d, 0x20) = y;                       // y
    TI32(&d, 0x24) = y * 32;                  // y (x32 fixed point)
    TI32(&d, 0x30) = crt_rand() % 16 + 16;    // vy
    int32_t x = crt_rand() % 840 - 100;
    TI32(&d, 0x28) = x;                       // x
    TI32(&d, 0x2c) = x * 32;                  // x (x32 fixed point)
    int32_t vx = (crt_rand() % 64) * 2 - 64;
    TI32(&d, 0x34) = vx;                      // vx
    TI32(&d, 0x3c) = (vx >= 0) ? 16 : 0;      // sprite row (direction)
    TI32(&d, 0x40) = 0;                       // animation frame
    TI32(&d, 0x44) = 0;                       // frame counter
    d.prio = 0x20;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_4089f0;
    task_create(&d);
}

// 0x4089f0: leaf particle per-frame update (drifts randomly, 4 frame anim).
void f_4089f0(Task* t)
{
    int32_t ax = TI32(t, 0x2c) + TI32(t, 0x34);
    TI32(t, 0x2c) = ax;
    int32_t ay = TI32(t, 0x24) + TI32(t, 0x30);
    TI32(t, 0x28) = ax >> 5;
    TI32(t, 0x24) = ay;
    TI32(t, 0x20) = ay >> 5;
    TI32(t, 0x34) = TI32(t, 0x34) + crt_rand() % 6 - 3;
    int32_t r = crt_rand() % 6 - 2;
    int32_t cnt = TI32(t, 0x44) + 1;
    TI32(t, 0x44) = cnt;
    TI32(t, 0x30) = TI32(t, 0x30) + r;
    if (cnt % 6 == 0)
        TI32(t, 0x40) = (TI32(t, 0x40) + 1) % 4;
    if (TI32(t, 0x28) > 740 || TI32(t, 0x28) + 16 < -100 || TI32(t, 0x20) > 420)
        t->dead = 1;
    gfx_blit(46, TI32(t, 0x28) - MI32(GS + 0), TI32(t, 0x20) - MI32(GS + 8), 16, 16,
             TI32(t, 0x40) * 16, TI32(t, 0x3c));
}

// 0x408ae0: create the score counter display task.
void f_408ae0()
{
    Task d; task_desc(&d);
    d.prio = 0x20;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_408b20;
    TI32(&d, 0x20) = 0;                       // displayed score
    task_create(&d);
}

// 0x408b20: score counter.  The displayed value rolls toward the real score
// (GS+0x10) digit by digit, then is drawn with surface 2 (score.bmp).
void f_408b20(Task* t)
{
    int32_t digits = 7;
    int32_t base_x = 350;
    if (MI32(GS + 0x38)) {
        digits = 8;
        base_x = 314;
    }
    int32_t diff = MI32(GS + 0x10) - TI32(t, 0x20);
    int32_t p = 1;
    for (int32_t i = digits; i > 0; --i) {
        p *= 10;
        if (diff == p)
            diff--;
    }
    int32_t m = 1;
    for (int32_t i = digits; i > 0; --i) {
        if ((diff / m) % 10)
            TI32(t, 0x20) += m;
        m *= 10;
    }
    int32_t d = 1;
    int32_t x = base_x + digits * 36;
    for (int32_t i = digits; i > 0; --i) {
        int32_t digit = (TI32(t, 0x20) / d) % 10;
        gfx_blit(2, x, 432, 38, 48, digit * 38, 0);
        x -= 36;
        d *= 10;
    }
}

// 0x408c10: spawn the sword slash effect (surface 17 = slash.bmp, 18 frames
// of 128x128 stacked vertically) at world position (x, y).
void f_408c10(int32_t x, int32_t y)
{
    Task d; task_desc(&d);
    TI32(&d, 0x20) = x;
    TI32(&d, 0x24) = y;
    for (int k = 0; k < 18; ++k) {
        TI32(&d, 0x28 + 8 * k) = 0;           // frame k source x
        TI32(&d, 0x2c + 8 * k) = 128 * k;     // frame k source y
    }
    TI32(&d, 0xb8) = 0;                       // frame counter
    d.prio = 0x10;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_408d80;
    task_create(&d);
}

// 0x408d80: slash effect animation (frames 1..17, then dies).
void f_408d80(Task* t)
{
    int32_t f = TI32(t, 0xb8) + 1;
    TI32(t, 0xb8) = f;
    if (f > 0x11) {
        t->dead = 1;
        return;
    }
    gfx_blit(0x11, TI32(t, 0x20) - MI32(GS + 0), TI32(t, 0x24) - MI32(GS + 8), 128, 128,
             TI32(t, 0x28 + 8 * f), TI32(t, 0x2c + 8 * f));
}

// 0x408df0: start the opening camera drop ("用意" ... "始め").  Moves the
// camera to y = -960 and pauses priority-8 tasks for 6000 frames (they are
// resumed by f_408e80 when the camera lands).
void f_408df0()
{
    MI32(GS + 8) = -960;
    MI32(GS + 4) = MI32(GS + 8) + 480;
    Task d; task_desc(&d);
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_408e80;
    d.prio = 0x20;
    TF32(&d, 0x20) = -960.0f;                 // camera y (float)
    TF32(&d, 0x24) = 10.0f;                   // camera speed
    task_create(&d);
    task_set_wait_mask(8, 6000);
}

// 0x408e80: opening camera drop update.
void f_408e80(Task* t)
{
    TF32(t, 0x20) = (float)((double)TF32(t, 0x24) + (double)TF32(t, 0x20));
    TF32(t, 0x24) = (float)((double)TF32(t, 0x24) - (double)0.05f);
    if ((double)TF32(t, 0x20) > 0.0) {
        TF32(t, 0x20) = 0.0f;
        t->dead = 1;
        task_resume_mask(8);
    }
    MI32(GS + 8) = ftol((double)TF32(t, 0x20));
    MI32(GS + 4) = MI32(GS + 8) + 480;
    if ((double)TF32(t, 0x20) < -200.0)
        gfx_blit(19, 220, 190, 200, 100, 0, 0);   // youi.bmp  "用意"
    else
        gfx_blit(20, 220, 190, 200, 100, 0, 0);   // hajime.bmp "始め"
}

// 0x408f50: open an encoded text file (story data/023.dat, staff roll
// data/024.dat) and create one scrolling line task per line, managed by a
// f_4090c0 task.  Bytes >= 0x20 are XOR 0x3a encoded (unless that would give
// a control character); a decoded 0xff becomes 0xc5.
void f_408f50(const char* file)
{
    TextStream fp;
    if (!fp.open(file))
        return;

    Task d; task_desc(&d);
    d.fn = f_4090c0;
    d.prio = 0x20;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    TI32(&d, 0x20) = 0;                       // first line task
    TI32(&d, 0x24) = 1;                       // scroll speed
    Task* mgr = task_create(&d);

    Task* last = nullptr;
    Task* head = nullptr;
    int32_t y45 = 0;
    int32_t y40 = 0;
    uint8_t buf[0x200];
    for (;;) {
        y40 += 40;
        y45 += 45;
        int i = 0;
        for (;;) {
            uint8_t c = (uint8_t)fp.getc();
            buf[i] = c;
            if (c == 0xff) {
                if (i != 0)
                    buf[i] = 0;
                break;
            }
            if ((c & 0xe0) > 0x10) {
                c ^= 0x3a;
                buf[i] = c;
                if ((c & 0xe0) < 0x20) {
                    c ^= 0x3a;
                    buf[i] = c;
                }
            }
            if (buf[i] == 0xff)
                buf[i] = 0xc5;
            if (buf[i] == 0x0a) {
                if (i != 0)
                    buf[i] = 0;
                break;
            }
            ++i;
        }
        if (buf[0] == 0xff)
            break;
        if (buf[0] == 0x0a)
            continue;
        char* nl = std::strchr((char*)buf, 0x0a);
        if (nl)
            *nl = 0;
        if (!last) {
            last = f_409220(mgr, (const char*)buf, y40);
            head = last;
        } else {
            Task* n = f_409220(mgr, (const char*)buf, y45);
            TTASK(last, 0x5c) = n;
            last = n;
        }
    }
    if (mgr)
        TTASK(mgr, 0x20) = head;
}

// 0x4090c0: scroll text lines upward (8x speed while button 9 is held) and
// draw them centred in white.  When every line has scrolled away the byte at
// GS+0x29 is cleared and the task ends.
void f_4090c0(Task* t)
{
    Task* node = TTASK(t, 0x20);
    Task* prev = nullptr;
    if (in_held(9))
        TI32(t, 0x24) = 8;
    else
        TI32(t, 0x24) = 1;
    const bool dc = gfx_draw_enabled();
    int32_t count = 0;
    while (node) {
        count++;
        if (TI32(node, 0x58) < TI32(node, 0x54)) {
            // still waiting for its turn
            prev = node;
            TI32(node, 0x58) = TI32(node, 0x58) + TI32(t, 0x24);
            node = TTASK(node, 0x5c);
        } else {
            int32_t y = TI32(node, 0x4c) - TI32(t, 0x24);
            TI32(node, 0x4c) = y;
            if (y < -0x20) {
                node->dead = 1;
                if (!prev)
                    TTASK(t, 0x20) = TTASK(node, 0x5c);
                else
                    TTASK(prev, 0x5c) = TTASK(node, 0x5c);
            }
            const char* s = (const char*)&TU8(node, 0x20);
            int32_t len = (int32_t)std::strlen(s);
            if (dc)
                gfx_text((640 - len * 14) / 2, TI32(node, 0x4c), s, len, 0xffffff);
            prev = node;
            node = TTASK(node, 0x5c);
        }
    }
    if (count == 0) {
        MU8(GS + 0x29) = 0;
        t->dead = 1;
    }
}

// 0x409220: create one text line task (text at +0x20, y at +0x4c, start
// delay at +0x54, delay counter +0x58, next line +0x5c).
Task* f_409220(Task* parent, const char* str, int32_t delay)
{
    Task d; task_desc(&d);
    size_t n = std::strlen(str) + 1;
    if (n > 0x40)
        n = 0x40;
    std::memcpy(&TU8(&d, 0x20), str, n);
    d.fn = f_4092c0;
    TI32(&d, 0x58) = 0;
    TI32(&d, 0x54) = delay;
    TI32(&d, 0x5c) = 0;
    d.f04 = 0;
    d.wait = 0;
    TI32(&d, 0x4c) = 480;
    d.prio = 0x20;
    d.parent = parent;
    return task_create(&d);
}

// 0x4092c0: empty task callback (text lines are driven by their manager).
void f_4092c0(Task*)
{
}

// 0x4092d0: spawn a small debris piece moving sideways (dir 0 = right,
// 1 = left) and register it with the player collision watcher.
void f_4092d0(int32_t x, int32_t y, int32_t dir)
{
    Task d; task_desc(&d);
    TI32(&d, 0x20) = x;
    TI32(&d, 0x24) = y;
    TI32(&d, 0x28) = 4 - dir * 8;             // vx
    TI32(&d, 0x2c) = 4;                       // vy
    TU8(&d, 0x30) = 0;                        // hit flag
    TI32(&d, 0x34) = 0;
    d.fn = f_409370;
    d.prio = 8;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    Task* n = task_create(&d);
    f_405f20(MTASK(GS + 0x44), n);
}

// 0x409370: debris piece update (surface 49 = data/012.dat).
void f_409370(Task* t)
{
    TI32(t, 0x20) = TI32(t, 0x20) + TI32(t, 0x28);
    TI32(t, 0x24) = TI32(t, 0x24) + TI32(t, 0x2c);
    if (TU8(t, 0x30) != 0) {
        t->dead = 1;
        return;
    }
    gfx_blit(0x31, TI32(t, 0x20) - MI32(GS + 0), TI32(t, 0x24) - MI32(GS + 8), 16, 16, 0, 0);
    if (TI32(t, 0x24) + 16 > 0x1ad)
        t->dead = 1;
}

// 0x4093e0: spawn a shock wave / projectile (dir 0 = falls, 1 = left,
// 2 = right) and register it with the player collision watcher.
void f_4093e0(int32_t x, int32_t y, int32_t dir)
{
    Task d; task_desc(&d);
    TI32(&d, 0x20) = x;
    TI32(&d, 0x24) = y;
    TI32(&d, 0x28) = 0;                       // animation source x (0 / 0x40)
    TU8(&d, 0x30) = 0;                        // hit flag
    TI32(&d, 0x34) = 0;
    TI32(&d, 0x38) = dir;
    d.fn = f_409470;
    d.prio = 8;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    Task* n = task_create(&d);
    f_406590(MTASK(GS + 0x44), n);
}

// 0x409470: shock wave update (surface 58 = data/021.dat, 2 frames 64x64).
// A falling one splits into a left and a right wave on the ground.
void f_409470(Task* t)
{
    switch (TI32(t, 0x38)) {
    case 0: TI32(t, 0x24) += 10; break;
    case 1: TI32(t, 0x20) += -16; break;
    case 2: TI32(t, 0x20) += 16; break;
    default: break;
    }
    TI32(t, 0x28) ^= 0x40;
    if (TU8(t, 0x30) != 0) {
        t->dead = 1;
        return;
    }
    gfx_blit(0x3a, TI32(t, 0x20) - MI32(GS + 0), TI32(t, 0x24) - MI32(GS + 8), 0x40, 0x40,
             TI32(t, 0x28), 0);
    if (TI32(t, 0x24) + 0x20 > 0x1ad && TI32(t, 0x38) == 0) {
        t->dead = 1;
        TI32(t, 0x24) = 0x18d;
        f_4093e0(TI32(t, 0x20), 0x18d, 1);
        f_4093e0(TI32(t, 0x20), TI32(t, 0x24), 2);
        return;
    }
    if (TI32(t, 0x20) < -0x40 || TI32(t, 0x20) > 0x2c0)
        t->dead = 1;
}

// ---------------------------------------------------------------------------
// Moon boss.  Task data:
//   +0x20 y (pixels, relative to the ground line), +0x24 vy, +0x28 y << 6,
//   +0x2c acceleration / parameter, +0x30 hit points, +0x34 hit flag (byte,
//   set by the player's attack code), +0x38 damage of the hit, +0x3c timer.
// The y coordinate is drawn as y - camera y; ground contact is at
// y + 468 >= 429.

// 0x409540: create the moon (form 1, 50 HP) far above the screen.
Task* f_409540()
{
    Task d; task_desc(&d);
    TI32(&d, 0x20) = -960;
    TI32(&d, 0x24) = 0;
    TI32(&d, 0x28) = -61440;                  // -960 << 6
    TI32(&d, 0x2c) = 5;
    TI32(&d, 0x30) = 0x32;                    // 50 HP
    TU8(&d, 0x34) = 0;
    TI32(&d, 0x38) = 0;
    TI32(&d, 0x3c) = 0;
    d.prio = 0x10;
    d.f04 = 5;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_4095c0;
    return task_create(&d);
}

// 0x4095c0: moon form 1 (50 HP): falls toward the ground, bounces.
// Defeating it switches to form 2 (f_4099d0, 70 HP).
void f_4095c0(Task* t)
{
    if (t->wait <= 0) {
        int32_t vy = TI32(t, 0x24);
        int32_t p = TI32(t, 0x28) + vy;
        TI32(t, 0x28) = p;
        TI32(t, 0x20) = p >> 6;
        if (TI32(t, 0x20) + 0x1d4 >= 0x1ad) {
            if (vy > TI32(t, 0x2c)) {
                f_40aae0();
                f_401bc0();
            }
            TI32(t, 0x24) = 0;
            TI32(t, 0x20) = -39;
            TI32(t, 0x28) = -2496;
        }
        int32_t a = TI32(t, 0x2c);
        int32_t nv = TI32(t, 0x24) + a;
        TI32(t, 0x24) = nv;
        if (nv > a * 10)
            TI32(t, 0x24) = a * 10;
    }
    if (TU8(t, 0x34) == 1) {
        int32_t hp = TI32(t, 0x30) - TI32(t, 0x38);
        TU8(t, 0x34) = 0;
        TI32(t, 0x30) = hp;
        if (hp <= 0) {
            add_score(((uint32_t)MI32(GS + 0x14) + 100u) * 15000u);
            f_4040b0(0x140, TI32(t, 0x20) + 0x17c);
            snd_play(7);
            TI32(t, 0x30) = 0x46;
            t->fn = f_4099d0;
            TI32(t, 0x2c) = 0x1e;
            TI32(t, 0x24) = 0x97;
        } else {
            add_score((uint32_t)MI32(GS + 0x14) * 100u);
        }
    }
    draw_moon(t);
}

// 0x409760: moon form 2, hovering near y = 200..210; at timer 60 it launches five
// attack objects.  After 440 frames it randomly switches to f_4099d0 or
// f_409bc0.
void f_409760(Task* t)
{
    if (t->wait <= 0) {
        if (TI32(t, 0x20) + 0x1d4 < 0xc8)
            TI32(t, 0x28) += 0x20;
        if (TI32(t, 0x20) + 0x1d4 > 0xd2)
            TI32(t, 0x28) += -0x20;
        int32_t y = TI32(t, 0x28) >> 6;
        TI32(t, 0x20) = y;
        TI32(t, 0x24) = 5;
        TI32(t, 0x3c) = TI32(t, 0x3c) + 1;
        if (y + 0x1d4 >= 0x1ad) {
            if (TI32(t, 0x2c) < 5) {
                f_40aae0();
                f_401bc0();
            }
            TI32(t, 0x24) = 0;
            TI32(t, 0x20) = -39;
            TI32(t, 0x28) = -2496;
        }
        if (TI32(t, 0x3c) == 0x3c) {
            f_40acb0(0x64, 0, t);
            f_40acb0(0xbe, 0, t);
            f_40acb0(0x118, crt_rand() % 2, t);
            f_40acb0(0x172, 1, t);
            f_40acb0(0x1cc, 1, t);
        }
    }
    draw_moon(t);
    if (TU8(t, 0x34) == 1) {
        int32_t hp = TI32(t, 0x30) - TI32(t, 0x38);
        TU8(t, 0x34) = 0;
        TI32(t, 0x30) = hp;
        if (hp <= 0) {
            TI32(t, 0x30) = 0x46;
            t->fn = f_409e70;
            TI32(t, 0x2c) = 0x1e;
            TI32(t, 0x24) = -50;
            TI32(t, 0x3c) = 0;
            add_score(((uint32_t)MI32(GS + 0x14) + 100u) * 25000u);
            f_4040b0(0x140, TI32(t, 0x20) + 0x190);
            snd_play(7);
            return;
        }
        add_score((uint32_t)MI32(GS + 0x14) * 150u);
    }
    if (TI32(t, 0x3c) > 0x1b8) {
        if (crt_rand() % 2 == 0) {
            t->fn = f_4099d0;
            TI32(t, 0x3c) = 0;
            TI32(t, 0x2c) = 0x1e;
            TI32(t, 0x24) = 0x97;
            return;
        }
        t->fn = f_409bc0;
        TI32(t, 0x3c) = 0;
        TI32(t, 0x2c) = 5;
    }
}

// 0x4099d0: moon form 2 (70 HP), bouncing on the ground (gravity +0x2c, bounce speed
// -30 * 0x2c).  At the top of a bounce it switches to f_409760 or f_409bc0.
void f_4099d0(Task* t)
{
    if (t->wait <= 0) {
        int32_t vy = TI32(t, 0x24);
        int32_t p = TI32(t, 0x28) + vy;
        TI32(t, 0x28) = p;
        TI32(t, 0x20) = p >> 6;
        if (TI32(t, 0x20) + 0x1d4 >= 0x1ad) {
            if (vy > TI32(t, 0x2c)) {
                f_40aae0();
                f_401bc0();
            }
            int32_t a = TI32(t, 0x2c);
            TI32(t, 0x20) = -39;
            TI32(t, 0x28) = -2496;
            TI32(t, 0x24) = -(a * 15) * 2;
        }
        TI32(t, 0x24) = TI32(t, 0x24) + TI32(t, 0x2c);
    }
    draw_moon(t);
    if (TU8(t, 0x34) == 1) {
        int32_t hp = TI32(t, 0x30) - TI32(t, 0x38);
        TU8(t, 0x34) = 0;
        TI32(t, 0x30) = hp;
        if (hp <= 0) {
            TI32(t, 0x30) = 0x46;
            t->fn = f_409e70;
            TI32(t, 0x2c) = 0x1e;
            TI32(t, 0x24) = -50;
            TI32(t, 0x3c) = 0;
            add_score(((uint32_t)MI32(GS + 0x14) + 100u) * 25000u);
            f_4040b0(0x140, TI32(t, 0x20) + 0x190);
            snd_play(7);
            return;
        }
        add_score((uint32_t)MI32(GS + 0x14) * 150u);
    }
    if (TI32(t, 0x24) == 0) {
        if (crt_rand() % 2 == 0) {
            t->fn = f_409760;
            TI32(t, 0x3c) = 0;
            TI32(t, 0x2c) = 5;
            return;
        }
        t->fn = f_409bc0;
        TI32(t, 0x3c) = 0;
        TI32(t, 0x2c) = 5;
    }
}

// 0x409bc0: moon form 2, rises off screen, calls lightning (aimed at the player's
// lane 40% of the time) every 50 frames, then comes back down.
void f_409bc0(Task* t)
{
    if (t->wait <= 0) {
        int32_t p = TI32(t, 0x28) + TI32(t, 0x24);
        TI32(t, 0x28) = p;
        int32_t y = p >> 6;
        int32_t c = TI32(t, 0x3c) + 1;
        TI32(t, 0x20) = y;
        TI32(t, 0x3c) = c;
        if (y < -0x5a0) {
            TI32(t, 0x20) = -0x5a0;
            TI32(t, 0x28) = -0x16800;
            TI32(t, 0x24) = 0;
        }
        if (c < 0x78)
            TI32(t, 0x24) = TI32(t, 0x24) - TI32(t, 0x2c);
        if (c > 0x78 && c < 0x168) {
            if (c % 50 == 0) {
                if (crt_rand() % 100 < 40)
                    f_408440(TI32(MTASK(GS + 0x44), 0x28));
                else
                    f_408440(crt_rand() % 3);
                f_403980(0x93fdfd);
            }
        }
        c = TI32(t, 0x3c);
        if (c > 0x136 && c < 0x1e4)
            TI32(t, 0x24) = TI32(t, 0x24) + TI32(t, 0x2c);
        if (c > 0x1e4 && c < 0x222) {
            int32_t v = TI32(t, 0x24) + (-TI32(t, 0x2c)) * 8;
            TI32(t, 0x24) = v;
            if (v < 0)
                TI32(t, 0x24) = 0;
        }
        if (c > 0x222 && c < 0x254)
            TI32(t, 0x24) = TI32(t, 0x24) - TI32(t, 0x2c);
        if (c > 0x254)
            TI32(t, 0x24) = TI32(t, 0x24) + TI32(t, 0x2c) * 2;
    }
    draw_moon(t);
    if (TU8(t, 0x34) == 1) {
        int32_t hp = TI32(t, 0x30) - TI32(t, 0x38);
        TU8(t, 0x34) = 0;
        TI32(t, 0x30) = hp;
        if (hp <= 0) {
            TI32(t, 0x30) = 0x46;
            t->fn = f_409e70;
            TI32(t, 0x2c) = 0x1e;
            TI32(t, 0x24) = -50;
            TI32(t, 0x3c) = 0;
            add_score(((uint32_t)MI32(GS + 0x14) + 100u) * 25000u);
            f_4040b0(0x140, TI32(t, 0x20) + 0x190);
            snd_play(7);
            return;
        }
        add_score((uint32_t)MI32(GS + 0x14) * 150u);
    }
    if (TI32(t, 0x3c) > 0x254 && TI32(t, 0x24) > 0) {
        if (crt_rand() % 2 == 0) {
            t->fn = f_4099d0;
            TI32(t, 0x3c) = 0;
            TI32(t, 0x2c) = 0x1e;
            TI32(t, 0x24) = 0x97;
            return;
        }
        t->fn = f_409760;
        TI32(t, 0x3c) = 0;
        TI32(t, 0x2c) = 5;
    }
}

// 0x409e70: moon form 3 (70 HP), bouncing.  +0x3c counts hits; after more than 4
// hits, at the top of a bounce, it picks f_40a0a0 (1/3), f_40a2e0 or
// f_40a5f0.  Defeat -> f_40a850 (death sequence).
void f_409e70(Task* t)
{
    if (t->wait <= 0) {
        int32_t vy = TI32(t, 0x24);
        int32_t p = TI32(t, 0x28) + vy;
        TI32(t, 0x28) = p;
        TI32(t, 0x20) = p >> 6;
        if (TI32(t, 0x20) + 0x1d4 >= 0x1ad) {
            if (vy > TI32(t, 0x2c)) {
                f_40aae0();
                f_401bc0();
            }
            int32_t a = TI32(t, 0x2c);
            TI32(t, 0x20) = -39;
            TI32(t, 0x28) = -2496;
            TI32(t, 0x24) = -(a * 15) * 2;
        }
        TI32(t, 0x24) = TI32(t, 0x24) + TI32(t, 0x2c);
    }
    draw_moon(t);
    if (TU8(t, 0x34) == 1) {
        int32_t hp = TI32(t, 0x30) - TI32(t, 0x38);
        TI32(t, 0x3c) = TI32(t, 0x3c) + 1;
        TI32(t, 0x30) = hp;
        TU8(t, 0x34) = 0;
        if (hp <= 0) {
            t->fn = f_40a850;
            TI32(t, 0x3c) = 0;
            TI32(t, 0x24) = 0;
            TI32(t, 0x2c) = 1;
            add_score(((uint32_t)MI32(GS + 0x14) + 200u) * 50000u);
            f_4040b0(0x140, TI32(t, 0x20) + 0x190);
            snd_play(7);
            return;
        }
        add_score((uint32_t)MI32(GS + 0x14) * 300u);
    }
    if (TI32(t, 0x24) == 0 && TI32(t, 0x3c) > 4) {
        if (crt_rand() % 3 == 0) {
            TI32(t, 0x3c) = 0;
            TI32(t, 0x2c) = 0x1e;
            TI32(t, 0x24) = -30;
            t->fn = f_40a0a0;
            return;
        }
        if (crt_rand() % 2 == 0) {
            t->fn = f_40a2e0;
            TI32(t, 0x3c) = 0;
            TI32(t, 0x2c) = 5;
            TI32(t, 0x24) = 0;
            return;
        }
        TI32(t, 0x3c) = 0;
        TI32(t, 0x2c) = 5;
        TI32(t, 0x24) = 0;
        t->fn = f_40a5f0;
    }
}

// 0x40a0a0: moon form 3, high jump (decelerates for 32 frames, then falls).
void f_40a0a0(Task* t)
{
    if (t->wait <= 0) {
        int32_t vy = TI32(t, 0x24);
        int32_t p = TI32(t, 0x28) + vy;
        TI32(t, 0x28) = p;
        TI32(t, 0x20) = p >> 6;
        int32_t c = TI32(t, 0x3c) + 1;
        TI32(t, 0x3c) = c;
        if (c < 0x20)
            TI32(t, 0x24) = vy - TI32(t, 0x2c);
        if (c > 0x20)
            TI32(t, 0x24) = TI32(t, 0x24) + TI32(t, 0x2c);
        if (TI32(t, 0x20) + 0x1d4 >= 0x1ad) {
            if (TI32(t, 0x24) > TI32(t, 0x2c)) {
                f_40aae0();
                f_401bc0();
            }
            int32_t a = TI32(t, 0x2c);
            TI32(t, 0x20) = -39;
            TI32(t, 0x28) = -2496;
            TI32(t, 0x24) = -(a * 15) * 2;
        }
    }
    draw_moon(t);
    if (TU8(t, 0x34) == 1) {
        int32_t hp = TI32(t, 0x30) - TI32(t, 0x38);
        TU8(t, 0x34) = 0;
        TI32(t, 0x30) = hp;
        if (hp <= 0) {
            t->fn = f_40a850;
            TI32(t, 0x3c) = 0;
            TI32(t, 0x24) = 0;
            TI32(t, 0x2c) = 1;
            add_score(((uint32_t)MI32(GS + 0x14) + 200u) * 50000u);
            f_4040b0(0x140, TI32(t, 0x20) + 0x190);
            snd_play(7);
            return;
        }
        add_score((uint32_t)MI32(GS + 0x14) * 300u);
    }
    if (TI32(t, 0x24) == 0 && TI32(t, 0x3c) > 0x54) {
        if (crt_rand() % 3 == 0) {
            t->fn = f_409e70;
            TI32(t, 0x2c) = 0x1e;
            TI32(t, 0x24) = -50;
            TI32(t, 0x3c) = 0;
            return;
        }
        if (crt_rand() % 2 == 0) {
            t->fn = f_40a2e0;
            TI32(t, 0x3c) = 0;
            TI32(t, 0x2c) = 5;
            TI32(t, 0x24) = 0;
            return;
        }
        TI32(t, 0x3c) = 0;
        TI32(t, 0x2c) = 5;
        TI32(t, 0x24) = 0;
        t->fn = f_40a5f0;
    }
}

// 0x40a2e0: moon form 3, rises and calls a scripted lightning barrage on
// lanes 1, 0, 2, then drops back.
void f_40a2e0(Task* t)
{
    if (t->wait <= 0) {
        int32_t p = TI32(t, 0x28) + TI32(t, 0x24);
        TI32(t, 0x28) = p;
        int32_t y = p >> 6;
        int32_t c = TI32(t, 0x3c) + 1;
        TI32(t, 0x20) = y;
        TI32(t, 0x3c) = c;
        if (y < -0x5a0) {
            TI32(t, 0x20) = -0x5a0;
            TI32(t, 0x28) = -0x16800;
            TI32(t, 0x24) = 0;
        }
        if (c < 0xa0)
            TI32(t, 0x24) = TI32(t, 0x24) + (-TI32(t, 0x2c)) * 2;
        if (c == 0x82) {
            f_408440(1);
            f_403980(0x93fdfd);
        }
        if (TI32(t, 0x3c) == 0x8c)
            f_408440(1);
        if (TI32(t, 0x3c) == 0x96)
            f_408440(1);
        if (TI32(t, 0x3c) == 0x91) {
            f_408440(0);
            f_403980(0x93fdfd);
        }
        if (TI32(t, 0x3c) == 0x9b)
            f_408440(0);
        if (TI32(t, 0x3c) == 0xa5)
            f_408440(0);
        if (TI32(t, 0x3c) == 0xa0) {
            f_408440(2);
            f_403980(0x93fdfd);
        }
        if (TI32(t, 0x3c) == 0xaa)
            f_408440(2);
        if (TI32(t, 0x3c) == 0xb4)
            f_408440(2);
        int32_t cc = TI32(t, 0x3c);
        if (cc > 0xb4 && cc < 0xfa)
            TI32(t, 0x24) = TI32(t, 0x24) + TI32(t, 0x2c) * 4;
        if (cc > 0xfa)
            TI32(t, 0x24) = TI32(t, 0x24) + (-TI32(t, 0x2c)) * 8;
    }
    draw_moon(t);
    if (TU8(t, 0x34) == 1) {
        int32_t hp = TI32(t, 0x30) - TI32(t, 0x38);
        TU8(t, 0x34) = 0;
        TI32(t, 0x30) = hp;
        if (hp <= 0) {
            t->fn = f_40a850;
            TI32(t, 0x3c) = 0;
            TI32(t, 0x24) = 0;
            TI32(t, 0x2c) = 1;
            add_score(((uint32_t)MI32(GS + 0x14) + 200u) * 50000u);
            f_4040b0(0x140, TI32(t, 0x20) + 0x190);
            snd_play(7);
            return;
        }
        add_score((uint32_t)MI32(GS + 0x14) * 300u);
    }
    if (TI32(t, 0x3c) > 0xfa && TI32(t, 0x24) <= 0) {
        if (crt_rand() % 3 == 0) {
            t->fn = f_409e70;
            TI32(t, 0x2c) = 0x1e;
            TI32(t, 0x24) = -50;
            TI32(t, 0x3c) = 0;
            return;
        }
        int32_t r = crt_rand() % 2;
        TI32(t, 0x3c) = 0;
        if (r == 0) {
            TI32(t, 0x2c) = 0x1e;
            TI32(t, 0x24) = -30;
            t->fn = f_40a0a0;
            return;
        }
        TI32(t, 0x2c) = 5;
        TI32(t, 0x24) = 0;
        t->fn = f_40a5f0;
    }
}

// 0x40a5f0: moon form 3, hovers and drops three rocks (x = 140, 260, 380)
// at timer 60; after 390 frames picks the next pattern.
void f_40a5f0(Task* t)
{
    if (t->wait <= 0) {
        if (TI32(t, 0x20) + 0x1d4 < 0xc8)
            TI32(t, 0x28) += 0x20;
        if (TI32(t, 0x20) + 0x1d4 > 0xd2)
            TI32(t, 0x28) += -0x20;
        int32_t y = TI32(t, 0x28) >> 6;
        TI32(t, 0x20) = y;
        TI32(t, 0x24) = 5;
        TI32(t, 0x3c) = TI32(t, 0x3c) + 1;
        if (y + 0x1d4 >= 0x1ad) {
            if (TI32(t, 0x2c) < 5) {
                f_40aae0();
                f_401bc0();
            }
            TI32(t, 0x24) = 0;
            TI32(t, 0x20) = -39;
            TI32(t, 0x28) = -2496;
        }
        if (TI32(t, 0x3c) == 0x3c) {
            f_407990(0x8c, t);
            f_407990(0x104, t);
            f_407990(0x17c, t);
        }
    }
    draw_moon(t);
    if (TU8(t, 0x34) == 1) {
        int32_t hp = TI32(t, 0x30) - TI32(t, 0x38);
        TU8(t, 0x34) = 0;
        TI32(t, 0x30) = hp;
        if (hp <= 0) {
            t->fn = f_40a850;
            TI32(t, 0x3c) = 0;
            TI32(t, 0x24) = 0;
            TI32(t, 0x2c) = 1;
            add_score(((uint32_t)MI32(GS + 0x14) + 200u) * 50000u);
            f_4040b0(0x140, TI32(t, 0x20) + 0x190);
            snd_play(7);
            return;
        }
        add_score((uint32_t)MI32(GS + 0x14) * 300u);
    }
    if (TI32(t, 0x3c) > 0x186) {
        if (crt_rand() % 3 == 0) {
            t->fn = f_409e70;
            TI32(t, 0x2c) = 0x1e;
            TI32(t, 0x24) = -50;
            TI32(t, 0x3c) = 0;
            return;
        }
        if (crt_rand() % 2 == 0) {
            t->fn = f_40a2e0;
            TI32(t, 0x3c) = 0;
            TI32(t, 0x2c) = 5;
            TI32(t, 0x24) = 0;
            return;
        }
        TI32(t, 0x3c) = 0;
        TI32(t, 0x2c) = 0x1e;
        TI32(t, 0x24) = -30;
        t->fn = f_40a0a0;
    }
}

// 0x40a850: moon death: rises with explosions every 40 frames (4 fragments
// each), removed after 540 frames.
void f_40a850(Task* t)
{
    if (t->wait <= 0) {
        int32_t vy = TI32(t, 0x24);
        int32_t p = TI32(t, 0x28) + vy;
        TI32(t, 0x28) = p;
        int32_t a = TI32(t, 0x2c);
        int32_t c = TI32(t, 0x3c) + 1;
        TI32(t, 0x20) = p >> 6;
        int32_t nv = vy - a;
        TI32(t, 0x3c) = c;
        TI32(t, 0x24) = nv;
        if (nv < -200)
            TI32(t, 0x24) = -200;
        if (c % 40 == 0) {
            f_4040b0(0x140, TI32(t, 0x20) + 0x190);
            snd_play(7);
            for (int i = 0; i < 4; ++i) {
                int32_t kind = crt_rand() % 2;
                int32_t y = TI32(t, 0x20) - crt_rand() % 200 + 0x17c;
                int32_t x = crt_rand() % 480 + 100;
                f_40afa0(x, y, kind);
            }
        }
        if (TI32(t, 0x3c) > 0x21c) {
            t->dead = 1;
            return;
        }
    }
    if (TU8(t, 0x34) == 1)
        TU8(t, 0x34) = 0;
    draw_moon(t);
}

// 0x40aa30: create the ending picture task.
void f_40aa30()
{
    Task d; task_desc(&d);
    TI32(&d, 0x3c) = 0;                       // timer
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_40aa80;
    d.prio = 2;
    task_create(&d);
}

// 0x40aa80: ending picture (surface 59 = data/022.dat) slides down from
// y = -100 to y = 100, removed after 1400 frames.
void f_40aa80(Task* t)
{
    int32_t c = TI32(t, 0x3c) + 1;
    TI32(t, 0x3c) = c;
    if (c > 0x578) {
        t->dead = 1;
        return;
    }
    int32_t y = c / 4 - 100;
    if (y > 100)
        y = 100;
    gfx_blit(0x3b, 200, y, 100, 100, 0, 0);
}
