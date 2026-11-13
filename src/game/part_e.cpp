// part_e: translation of the original functions in [0x40aae0, 0x40d560).
//
// Effects (dust cloud, guard enemies, waza beam, palette fades), resource
// loading, the main loop tick, the title screen and the three replay list
// screens.  See docs/notes/part_e.md for the meaning of globals and fields.
#include "game.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

// (functions of other parts are declared in game.h)

// ===========================================================================
// 0x40aae0: spawn the dust cloud shown when the player lands on the ground.
// Surface 3 (image/tuti.bmp, 512x3840) holds 15 frames of 512x256.
// Task data: +0x20 x, +0x24 y, +0x28+8i/+0x2c+8i frame (sx, sy) table,
//            +0xa0 frame index, +0xa4 tick counter.
// ===========================================================================
void f_40aae0()
{
    Task d; task_desc(&d);
    d.prio = 0x10;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_40ac20;
    TI32(&d, 0x20) = 0x40;
    TI32(&d, 0x24) = 0xac;
    for (int i = 0; i < 16; i++) {
        TI32(&d, 0x28 + 8 * i) = 0;
        TI32(&d, 0x2c + 8 * i) = (i < 15) ? 0x100 * i : 0;
    }
    // +0xa0 / +0xa4 coincide with the 16th table entry (both 0).
    task_create(&d);
}

// 0x40ac20: dust cloud animation, advance one frame every 2 ticks.
void f_40ac20(Task* t)
{
    int32_t tick = ++TI32(t, 0xa4);
    if (tick % 2 == 0) {
        int32_t f = ++TI32(t, 0xa0);
        if (f > 14) {
            t->dead = 1;
            return;
        }
    }
    int32_t x = TI32(t, 0x20) - MI32(GS + 0);
    int32_t y = TI32(t, 0x24) - MI32(GS + 8);
    int32_t f = TI32(t, 0xa0);
    gfx_blit(3, x, y, 0x200, 0x100, TI32(t, 0x28 + f * 8), TI32(t, 0x2c + f * 8));
}

// ===========================================================================
// 0x40acb0: spawn a guard enemy standing on a building floor.
// Task data: +0x20 x, +0x24 y offset, +0x28 hp (2), +0x2c age, +0x30 type
// (0/1), +0x34 recoil counter (starts -90), +0x38 shield (0, becomes 2),
// +0x3c (byte) hit flag, +0x40 pending damage.
// The parent is the building floor task (parent data +0x20 = floor y).
// ===========================================================================
void f_40acb0(int32_t x, int32_t type, Task* parent)
{
    Task d; task_desc(&d);
    d.prio = 8;
    d.f04 = 0;
    d.wait = 0;
    d.parent = parent;
    d.fn = f_40ad50;
    TI32(&d, 0x20) = x;
    TI32(&d, 0x24) = 0;
    TI32(&d, 0x28) = 2;
    TI32(&d, 0x2c) = 0;
    TI32(&d, 0x30) = type;
    TI32(&d, 0x34) = -90;
    TI32(&d, 0x38) = 0;
    TU8(&d, 0x3c) = 0;
    TI32(&d, 0x40) = 0;
    Task* n = task_create(&d);
    f_407640(MTASK(GS + 0x44), n);
}

// 0x40ad50: guard enemy.
void f_40ad50(Task* t)
{
    Task* p = t->parent;   // building floor; its data +0x20 is the floor y

    if (TI32(t, 0x34) < 0) {
        if (++TI32(t, 0x34) == 0)
            TI32(t, 0x38) = 2;
    }
    int32_t age = ++TI32(t, 0x2c);
    if (age < 60)
        TI32(t, 0x24)++;
    if (age > 350) {
        if (--TI32(t, 0x24) < 0) {
            t->dead = 1;
            return;
        }
    }

    int32_t sx = TI32(t, 0x20) - MI32(GS + 0);                          // ebx
    int32_t sy = TI32(t, 0x24) - MI32(GS + 8) + TI32(p, 0x20) + 0x17c;  // [esp+0x10]
    int32_t ty = TI32(t, 0x34) + sy;                                    // ebp
    int32_t tx = sx - TI32(t, 0x30) * 76 + 60;                          // edi

    if (TU8(t, 0x3c)) {
        int32_t s = TI32(t, 0x38);
        TU8(t, 0x3c) = 0;
        if (s > 0) {
            s -= TI32(t, 0x40);
            TI32(t, 0x38) = s;
            if (s <= 0) {
                TI32(t, 0x34) = -90;
                f_404170(MI32(GS + 0) + tx + 10, MI32(GS + 8) + ty + 10, 2);
                f_404170(MI32(GS + 0) + tx + 10, MI32(GS + 8) + ty + 10, 2);
                f_404170(MI32(GS + 0) + tx + 10, MI32(GS + 8) + ty + 10, 2);
                snd_play(7);
            }
        } else {
            TI32(t, 0x28) -= TI32(t, 0x40);
        }
        TI32(t, 0x40) = 0;
    }

    if (TI32(t, 0x28) > 0 && (uint8_t)p->dead == 0) {
        if (TI32(p, 0x30) > TI32(p, 0x38) || TU8(p, 0x34) == 0) {
            int32_t c = TI32(t, 0x2c);
            if (c > 200 && c < 240 && TI32(t, 0x38) > 0 && c % 10 == 0) {
                f_4092d0(MI32(GS + 0) + (tx - TI32(t, 0x30) * 20) + 0x14,
                         MI32(GS + 8) + ty + 0x1e,
                         TI32(t, 0x30));
                snd_play(6);
            }
            gfx_blit(0x2f, sx, sy, 0x50, 0x50, TI32(t, 0x30) * 80, 0);
            gfx_blit(0x30, tx, ty, 0x24, 0x2e, TI32(t, 0x30) * 36, 0);
            return;
        }
    }
    // killed, or the floor below collapsed: fall off
    f_40afa0(TI32(t, 0x20), TI32(t, 0x24) + TI32(p, 0x20) + 0x17c, TI32(t, 0x30));
    t->dead = 1;
}

// ===========================================================================
// 0x40afa0: spawn a falling (defeated) guard.
// Task data: +0x20 x, +0x24 y, +0x28 vx (x16), +0x2c vy (x16), +0x30 type.
// ===========================================================================
void f_40afa0(int32_t x, int32_t y, int32_t type)
{
    Task d; task_desc(&d);
    TI32(&d, 0x20) = x;
    TI32(&d, 0x24) = y;
    TI32(&d, 0x30) = type;
    TI32(&d, 0x2c) = 0;
    TI32(&d, 0x28) = crt_rand() % 80 - 40;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_40b030;
    d.prio = 8;
    task_create(&d);
}

// 0x40b030: falling guard.
void f_40b030(Task* t)
{
    int32_t vx = TI32(t, 0x28);
    int32_t vy = TI32(t, 0x2c);
    TI32(t, 0x20) += vx >> 4;
    TI32(t, 0x24) += vy >> 4;
    TI32(t, 0x2c) = vy + 1;
    int32_t x = TI32(t, 0x20) - MI32(GS + 0);
    int32_t y = TI32(t, 0x24) - MI32(GS + 8);
    gfx_blit(0x2f, x, y, 0x50, 0x50, TI32(t, 0x30) * 80, 0x50);
    if (TI32(t, 0x24) > 0x260 || TI32(t, 0x20) < -0x50 || TI32(t, 0x20) > 0x2d0)
        t->dead = 1;
}

// ===========================================================================
// 0x40b0c0: special move (waza) beam.  Two half-beam tasks (prio 0x20 and 8)
// rising from (x, y); then the hit is dispatched by the stage type
// MTASK(GS+0x40)->f04 and the camera follow task is attached at +0x34.
// Beam task data: +0x20 x, +0x24 y, +0x28 rise speed (20), +0x2c half (0/1).
// ===========================================================================
Task* f_40b0c0(int32_t x, int32_t y)
{
    Task d; task_desc(&d);
    TI32(&d, 0x20) = x;
    TI32(&d, 0x24) = y;
    TI32(&d, 0x28) = 0x14;
    TI32(&d, 0x2c) = 0;
    d.fn = f_40b2c0;
    d.prio = 0x20;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    Task* a = task_create(&d);                      // ebx

    task_desc(&d);
    TI32(&d, 0x20) = x;
    TI32(&d, 0x24) = y;
    TI32(&d, 0x2c) = 1;
    TI32(&d, 0x28) = 0x14;
    d.fn = f_40b2c0;
    d.prio = 8;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    Task* b = task_create(&d);                      // esi

    Task* stage = MTASK(GS + 0x40);
    int32_t camy = 0;                               // edi (default case: uninitialised in the original)
    switch ((uint32_t)stage->f04) {
    case 0:
        f_406dc0(a, MTASK(GS + 0x40));
        camy = TI32(MTASK(GS + 0x40), 0x20);
        break;
    case 1:
        f_404870(a, MTASK(GS + 0x40));
        camy = TI32(MTASK(GS + 0x40), 0x20);
        break;
    case 2:
        f_404f30(a, b, MTASK(GS + 0x40));
        camy = TI32(MTASK(GS + 0x40), 0x20);
        break;
    case 4: {
        Task* st = MTASK(GS + 0x40);
        Task* q = TTASK(st, 0x24 + 4 * TI32(MTASK(GS + 0x44), 0x28));
        Task* first = TTASK(q, 0x20);
        if (first == nullptr) {
            camy = MI32(GS + 8);
            break;
        }
        Task* obj = TTASK(q, 0x24);
        camy = TI32(first, 0x20);
        f_405a00(a, obj);
        for (Task* n = TTASK(obj, 0x44); n != nullptr; n = TTASK(n, 0x44))
            f_405a00(a, n);
        break;
    }
    case 3:
        camy = MI32(GS + 8);
        break;
    case 5:
        f_4074a0(a, b, MTASK(GS + 0x40));
        camy = TI32(MTASK(GS + 0x40), 0x20);
        break;
    default:
        break;
    }
    TTASK(a, 0x34) = f_4019d0(camy);
    return a;
}

// 0x40b2c0: one half of the waza beam (surface 8 = image/waza.bmp, 2 columns
// of 40x513).  When it leaves the screen the player's +0x30 flag is cleared
// and all tasks with prio & 0x7f are resumed.
void f_40b2c0(Task* t)
{
    TI32(t, 0x24) -= TI32(t, 0x28);
    if (TI32(t, 0x24) + 0x200 < MI32(GS + 8)) {
        t->dead = 1;
        TI32(MTASK(GS + 0x44), 0x30) = 0;
        task_resume_mask(0x7f);
        return;
    }
    int32_t x = TI32(t, 0x20) - MI32(GS + 0);
    int32_t y = TI32(t, 0x24) - MI32(GS + 8);
    int32_t sx = TI32(t, 0x2c) * 40;
    gfx_blit(8, sx + x, y, 0x28, 0x200, sx, 0);
}

// ===========================================================================
// 0x40b350 / 0x40b390: fade the palette to black over 32 frames.
// Each frame: out = base * (32 - n) / 32 where base is the palette loaded
// from image/game.pal (gfx_get_palette returns that, not the faded one).
// ===========================================================================
Task* f_40b350()
{
    Task d; task_desc(&d);
    d.fn = f_40b390;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    TI32(&d, 0x20) = 0;
    d.prio = 0x10;
    return task_create(&d);
}

void f_40b390(Task* t)
{
    uint32_t base[256];
    uint32_t out[256];
    int32_t n = ++TI32(t, 0x20);
    gfx_get_palette(base);
    if (TI32(t, 0x20) > 0x20) {
        t->dead = 1;
        return;
    }
    int32_t k = 0x20 - n;
    const uint8_t* src = reinterpret_cast<const uint8_t*>(base);
    uint8_t* dst = reinterpret_cast<uint8_t*>(out);
    std::memset(out, 0, sizeof(out));   // reserved byte is uninitialised in the original (ignored)
    for (int i = 0; i < 0x400; i += 4) {
        dst[i + 0] = (uint8_t)(((int32_t)src[i + 0] * k) / 32);
        dst[i + 1] = (uint8_t)(((int32_t)src[i + 1] * k) / 32);
        dst[i + 2] = (uint8_t)(((int32_t)src[i + 2] * k) / 32);
    }
    gfx_set_palette(out);
}

// 0x40b450 / 0x40b490: fade the palette in from black over 32 frames.
Task* f_40b450()
{
    Task d; task_desc(&d);
    d.fn = f_40b490;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    TI32(&d, 0x20) = 0;
    d.prio = 0x40;
    return task_create(&d);
}

void f_40b490(Task* t)
{
    uint32_t base[256];
    uint32_t out[256];
    gfx_get_palette(base);
    int32_t n = ++TI32(t, 0x20);
    if (n > 0x20) {
        t->dead = 1;
        gfx_set_palette(base);
        return;
    }
    const uint8_t* src = reinterpret_cast<const uint8_t*>(base);
    uint8_t* dst = reinterpret_cast<uint8_t*>(out);
    std::memset(out, 0, sizeof(out));
    for (int i = 0; i < 0x400; i += 4) {
        dst[i + 0] = (uint8_t)(((int32_t)src[i + 0] * n) / 32);
        dst[i + 1] = (uint8_t)(((int32_t)src[i + 1] * n) / 32);
        dst[i + 2] = (uint8_t)(((int32_t)src[i + 2] * n) / 32);
    }
    gfx_set_palette(out);
}

// ===========================================================================
// 0x40b550 / 0x40b5c0 / 0x40b580 / 0x40b590: static object at 0x41f7b0.
// Layout: +0x00 (0x41f7b0) replay random seed, +0x04 (0x41f7b4) allocator
// byte, +0x08 (0x41f7b8) vector begin, +0x0c (0x41f7bc) end,
// +0x10 (0x41f7c0) capacity end.  Elements are 8-byte input records.
// The pointers live in the MPTR side table.
// ===========================================================================
void f_40b550()
{
    MU8(0x41f7b4) = 0;           // allocator byte (garbage in the original)
    MPTR(0x41f7b8) = nullptr;
    MPTR(0x41f7bc) = nullptr;
    MPTR(0x41f7c0) = nullptr;
    std::atexit(f_40b580);
}

void f_40b580()
{
    // 0x413efe = operator delete (VC6 -> free)
    ::operator delete(MPTR(0x41f7b8));
    MPTR(0x41f7b8) = nullptr;
    MPTR(0x41f7bc) = nullptr;
    MPTR(0x41f7c0) = nullptr;
}

// ===========================================================================
// 0x40b5f0: startup.  Shows image/load.bmp, binds the controls, loads every
// surface and sound, applies the palette, converts the three UI colours and
// selects the first scene.
// ===========================================================================
void f_40b5f0()
{
    task_system_init();
    gfx_load_bmp(0, STR_41e598, 0xff00ff, 0);       // image/load.bmp
    gfx_clear_rgb(0);
    gfx_blit(0, 0xa0, 0xb4, 0x140, 0x5a, 0, 0);
    gfx_flip();

    // keyboard (device 0): Z = attack/decide, X = 8, arrows, Esc, Tab
    in_bind(0, 5, 0x5a);
    in_bind(0, 8, 0x58);
    in_bind(0, 6, 0x26);
    in_bind(0, 7, 0x28);
    in_bind(0, 1, 0x26);
    in_bind(0, 3, 0x28);
    in_bind(0, 2, 0x25);
    in_bind(0, 4, 0x27);
    in_bind(0, 9, 0x5a);
    in_bind(0, 10, 0x1b);
    in_bind(0, 11, 0x09);
    // joystick (device 1): 0 up 1 down 2 left 3 right 4.. buttons
    in_bind(1, 5, 4);
    in_bind(1, 8, 5);
    in_bind(1, 6, 0);
    in_bind(1, 7, 1);
    in_bind(1, 7, 6);
    in_bind(1, 1, 0);
    in_bind(1, 3, 1);
    in_bind(1, 2, 2);
    in_bind(1, 4, 3);
    in_bind(1, 9, 4);

    gfx_load_bmp(0x00, STR_41e588, 0xff00ff, 0);    // image/char.bmp
    gfx_load_bmp(0x0a, STR_41e578, 0xff00ff, 0);    // image/bg.bmp
    gfx_load_bmp(0x01, STR_41e568, 0xff00ff, 0);    // image/life.bmp
    gfx_load_bmp(0x02, STR_41e558, 0xff00ff, 0);    // image/score.bmp
    gfx_load_bmp(0x03, STR_41e548, 0xff00ff, 0);    // image/tuti.bmp
    gfx_load_bmp(0x04, STR_41e538, 0xff00ff, 0);    // image/bil10.bmp
    gfx_load_bmp(0x05, STR_41e528, 0xff00ff, 0);    // image/bil20.bmp
    gfx_load_bmp(0x06, STR_41e518, 0xff00ff, 0);    // image/bil30.bmp
    gfx_load_bmp(0x07, STR_41e504, 0xff00ff, 0);    // image/bougyo.bmp
    gfx_load_bmp(0x08, STR_41e4f4, 0xff00ff, 0);    // image/waza.bmp
    gfx_load_bmp(0x09, STR_41e4e4, 0xff00ff, 0);    // image/jump.bmp
    gfx_load_bmp(0x0c, STR_41e4d4, 0xff00ff, 0);    // image/title.bmp
    gfx_load_bmp(0x0d, STR_41e4c0, 0xff00ff, 0);    // image/hahen_1.bmp
    gfx_load_bmp(0x0e, STR_41e4ac, 0xff00ff, 0);    // image/hahen_2.bmp
    gfx_load_bmp(0x0f, STR_41e498, 0xff00ff, 0);    // image/hahen_3.bmp
    gfx_load_bmp(0x10, STR_41e488, 0xff00ff, 0);    // image/icon.bmp
    gfx_load_bmp(0x11, STR_41e478, 0xff00ff, 0);    // image/slash.bmp
    gfx_load_bmp(0x12, STR_41e468, 0xff00ff, 0);    // image/kumo.bmp
    gfx_load_bmp(0x13, STR_41e458, 0xff00ff, 0);    // image/youi.bmp
    gfx_load_bmp(0x14, STR_41e444, 0xff00ff, 0);    // image/hajime.bmp
    gfx_load_bmp(0x15, STR_41e434, 0xff00ff, 0);    // image/combo.bmp
    gfx_load_bmp(0x16, STR_41e424, 0xff00ff, 0);    // image/zan.bmp
    gfx_load_bmp(0x17, STR_41e414, 0xff00ff, 0);    // image/syuen.bmp
    gfx_load_bmp(0x18, STR_41e400, 0xff00ff, 0);    // image/renzan.bmp
    gfx_load_bmp(0x19, STR_41e3f0, 0xff00ff, 0);    // image/ten.bmp
    gfx_load_bmp(0x1a, STR_41e3e0, 0xff00ff, 0);    // image/seika.bmp
    gfx_load_bmp(0x1b, STR_41e3cc, 0xff00ff, 0);    // image/bougyobar.bmp
    gfx_load_bmp(0x1c, STR_41e3b8, 0xff00ff, 0);    // image/wazabar.bmp
    gfx_load_bmp(0x1d, STR_41e3a4, 0xff00ff, 0);    // image/wazabar2.bmp
    gfx_load_bmp(0x1f, STR_41e390, 0xff00ff, 0);    // image/score_s.bmp
    gfx_load_bmp(0x20, STR_41e380, 0xff00ff, 0);    // image/ten_s.bmp
    gfx_load_bmp(0x21, STR_41e36c, 0xff00ff, 0);    // image/renzan_s.bmp
    gfx_load_bmp(0x22, STR_41e360, 0xff00ff, 0);    // image/i.bmp
    gfx_load_bmp(0x23, STR_41e350, 0xff00ff, 0);    // image/demo.bmp
    gfx_load_bmp(0x24, STR_41e33c, 0xff00ff, 0);    // image/saiengi.bmp
    gfx_load_bmp(0x25, STR_41e328, 0xff00ff, 0);    // image/saiengi2.bmp
    gfx_load_bmp(0x26, STR_41e314, 0xff00ff, 0);    // image/modoru.bmp
    gfx_load_bmp(0x27, STR_41e300, 0xff00ff, 0);    // image/modoru2.bmp
    gfx_load_bmp(0x1e, STR_41e2f0, 0xff00ff, 0);    // data/001.dat
    gfx_load_bmp(0x0b, STR_41e2e0, 0xff00ff, 0);    // data/002.dat
    gfx_load_bmp(0x28, STR_41e2d0, 0xff00ff, 0);    // data/003.dat
    gfx_load_bmp(0x29, STR_41e2c0, 0xff00ff, 0);    // data/004.dat
    gfx_load_bmp(0x2a, STR_41e2b0, 0xff00ff, 0);    // data/005.dat
    gfx_load_bmp(0x2b, STR_41e2a0, 0xff00ff, 0);    // data/006.dat
    gfx_load_bmp(0x2c, STR_41e290, 0xff00ff, 0);    // data/007.dat
    gfx_load_bmp(0x2d, STR_41e280, 0xff00ff, 0);    // data/008.dat
    gfx_load_bmp(0x2e, STR_41e270, 0xff00ff, 0);    // data/009.dat
    gfx_load_bmp(0x2f, STR_41e260, 0xff00ff, 0);    // data/010.dat
    gfx_load_bmp(0x30, STR_41e250, 0xff00ff, 0);    // data/011.dat
    gfx_load_bmp(0x31, STR_41e240, 0xff00ff, 0);    // data/012.dat
    gfx_load_bmp(0x32, STR_41e230, 0xff00ff, 0);    // data/013.dat
    gfx_load_bmp(0x33, STR_41e220, 0xff00ff, 0);    // data/014.dat
    gfx_load_bmp(0x34, STR_41e210, 0xff00ff, 0);    // data/015.dat
    gfx_load_bmp(0x35, STR_41e200, 0xff00ff, 0);    // data/016.dat
    gfx_load_bmp(0x36, STR_41e1f0, 0xff00ff, 0);    // data/017.dat
    gfx_load_bmp(0x37, STR_41e1e0, 0xff00ff, 0);    // data/018.dat
    gfx_load_bmp(0x38, STR_41e1d0, 0xff00ff, 0);    // data/019.dat
    gfx_load_bmp(0x39, STR_41e1c0, 0xff00ff, 0);    // data/020.dat
    gfx_load_bmp(0x3a, STR_41e1b0, 0xff00ff, 0);    // data/021.dat
    gfx_load_bmp(0x3b, STR_41e1a0, 0xff00ff, 0);    // data/022.dat

    // re-apply the palette loaded from image/game.pal
    uint32_t pal[256];
    gfx_get_palette(pal);
    gfx_set_palette(pal);

    MU32(0x41f6d0) = gfx_rgb_to_pixel(0xfe9656);
    MU32(0x41f6d4) = gfx_rgb_to_pixel(0x2020a0);
    MU32(0x41f6d8) = gfx_rgb_to_pixel(0x343434);

    snd_load(STR_41e190, 4);    // sound/enter.wav
    snd_load(STR_41e17c, 3);    // sound/select.wav
    snd_load(STR_41e16c, 0);    // sound/slash.wav
    snd_load(STR_41e158, 1);    // sound/slash2.wav
    snd_load(STR_41e144, 2);    // sound/slash3.wav
    snd_load(STR_41e130, 5);    // sound/bougyo.wav
    snd_load(STR_41e11c, 6);    // sound/tubure.wav
    snd_load(STR_41e10c, 7);    // sound/hakai.wav
    snd_load(STR_41e0fc, 8);    // sound/move.wav
    snd_load(STR_41e0ec, 9);    // sound/jump.wav
    snd_load(STR_41e0d8, 10);   // sound/tyakuti.wav
    snd_load(STR_41e0c8, 11);   // sound/waza.wav
    snd_load(STR_41e0b4, 12);   // sound/kaminari.wav
    snd_load(STR_41e0a0, 13);   // sound/sibire.wav
    snd_load(STR_41e08c, 14);   // sound/gorogoro.wav
    snd_load(STR_41e07c, 15);   // sound/iwa.wav

    // Font: CreateFontA(28, 14, 0, 0, FW_NORMAL, 0, 0, 0, SHIFTJIS_CHARSET,
    // 0, 0, 0, 0, "HGS行書体"), falling back to "ＭＳ 明朝"; stored at gfx+0x620.
    // The engine creates it (gfx_text).

    MI32(0x41f798) = 0;
    MFN(0x41f79c) = f_40c050;
    MI32(0x41f6dc) = f_410280();
    MI32(0x41f6e0) = f_410310();
#ifdef _WIN32
    _mkdir(STR_41e05c);                 // CreateDirectoryA("save", NULL)
#else
    mkdir(STR_41e05c, 0777);
#endif
}

// 0x40bf30: shutdown.
void f_40bf30()
{
    f_410360(MI32(0x41f6e0));
    f_4102d0(MI32(0x41f6dc));
    task_kill_all();    // 0x412860: 0x412950 + 0x412a30
    task_reap();
}

// ===========================================================================
// 0x40bf50: one main loop tick.  Runs the current scene at 60 fps
// (16/17/17 ms), presents, and skips drawing of every other frame when late.
// While MU8(0x41f6b8) is set (replay fast forward) the scene runs 3 extra
// times per tick without drawing.
// ===========================================================================
void f_40bf50()
{
    uint8_t on = 1;   // bl
    if (!(MU8(0x41f6e8) & 1)) {
        MU8(0x41f6e8) |= 1;
        MU32(0x41f828) = plat_time_ms();
    }
    MU32(0x41f82c) = plat_time_ms();
    if (MU32(0x41f82c) - MU32(0x41f828) <= 16)
        return;
    if (MU8(0x41f6b8)) {
        gfx_set_draw_enabled(false);
        MFN(0x41f79c)();
        MFN(0x41f79c)();
        MFN(0x41f79c)();
        gfx_set_draw_enabled(true);
    }
    MFN(0x41f79c)();
    gfx_flip();
    MU32(0x41f828) = MU32(0x41f828) + ((MI32(0x41f830) % 3) != 0 ? 1u : 0u) + 16u;
    if (MU32(0x41f82c) > MU32(0x41f828) + 10)
        on = (MU8(0x41f834) == 0) ? 1 : 0;
    MU8(0x41f834) = on;
    gfx_set_draw_enabled(on != 0);
    if (MU32(0x41f82c) > MU32(0x41f828) + 500)
        MU32(0x41f828) = MU32(0x41f82c);
    if (++MI32(0x41f830) == 99999999)
        MI32(0x41f830) = 0;
}

// ===========================================================================
// 0x40c050: scene 0.  Polls input once and switches to the title.
// ===========================================================================
void f_40c050()
{
    in_update();
    if (in_pressed(10))
        plat_quit();
    MI32(0x41f798) = 1;
    MFN(0x41f79c) = f_40c080;
}

// ===========================================================================
// 0x40c080: scene 1, title screen.
// MI32(0x41f838): cursor (0 = start, 1 = replays)
// MI32(0x41f83c): mode (0 = 第一幕, 1 = 第二幕, 2 = とことん)
// MI32(0x41f6dc): unlock level (0..2) from save/game.sav
// MI32(0x41f840): idle counter, MU8(0x41e058): "arrows need spawning" flag
// ===========================================================================
void f_40c080()
{
    if (MU8(0x41e058)) {
        MU8(0x41e058) = 0;
        if (MI32(0x41f6dc) != 0 && MI32(0x41f838) == 0) {
            f_4032a0(10, 0x11d);
            f_4033f0(0x256, 0x11d);
        }
    }
    in_update();
    MI32(0x41f840)++;
    if (in_pressed(1)) {
        MI32(0x41f840) = 0;
        if (MI32(0x41f838) == 1) {
            if (MI32(0x41f6dc) != 0) {
                f_4032a0(10, 0x11d);
                f_4033f0(0x256, 0x11d);
            }
            snd_play(3);
        }
        MI32(0x41f838) = 0;
    } else if (in_pressed(3)) {
        MI32(0x41f840) = 0;
        if (MI32(0x41f838) == 0) {
            task_kill_all();
            snd_play(3);
        }
        MI32(0x41f838) = 1;
    } else if (in_pressed(4) && MI32(0x41f838) == 0) {
        if (MI32(0x41f6dc) != 0 && MI32(0x41f83c) <= MI32(0x41f6dc)) {
            MI32(0x41f840) = 0;
            snd_play(3);
            MI32(0x41f83c) = (MI32(0x41f83c) + 1) % (MI32(0x41f6dc) + 1);
        }
    } else if (in_pressed(2) && MI32(0x41f838) == 0) {
        if (MI32(0x41f6dc) > 0) {
            MI32(0x41f840) = 0;
            snd_play(3);
            if (--MI32(0x41f83c) < 0) {
                if (MI32(0x41f6dc) == 2)
                    MI32(0x41f83c) = 2;
                else if (MI32(0x41f6dc) == 1)
                    MI32(0x41f83c) = 1;
            }
        }
    } else if (in_pressed(9)) {
        MU8(0x41e058) = 1;
        MI32(0x41f840) = 0;
        snd_play(4);
        if (MI32(0x41f838) != 0) {
            f_40c420();
            MI32(0x41f798) = 9;
            MFN(0x41f79c) = f_40c4a0;
            return;
        }
        switch (MI32(0x41f83c)) {
        case 0:
            mus_load(STR_41e5b8, 0);            // sound/bgm.mid
            mus_set_loop(0, 0, 0x21ef8);
            mus_play(0);
            f_40d410();
            MU32(0x41f7b0) = crt_time();
            crt_srand(MU32(0x41f7b0));
            break;
        case 1:
            mus_load(STR_41e5a8, 0);            // data/000.dat (MIDI)
            mus_set_loop(0, 0x7b00, 0x3a500);
            mus_play(0);
            f_40d410();
            task_kill_all();
            task_run_all();
            f_40e750();
            MI32(0x41f798) = 3;
            MFN(0x41f79c) = f_40e840;
            MU32(0x41f7b0) = crt_time();
            crt_srand(MU32(0x41f7b0));
            break;
        case 2:
            mus_load(STR_41e5b8, 0);
            mus_set_loop(0, 0, 0x21ef8);
            mus_play(0);
            f_40f2d0();
            MU32(0x41f7b0) = crt_time();
            crt_srand(MU32(0x41f7b0));
            break;
        default:
            break;
        }
    } else if (in_pressed(10)) {
        plat_quit();
    }

    gfx_blit(0x0c, 0, 0, 0x280, 0x1e0, 0, 0);
    gfx_blit(0x10, 0x5a, MI32(0x41f838) * 0x55 + 0x118, 0x54, 0x42, 0, 0);
    if (MI32(0x41f83c) == 1) {
        gfx_blit(0x35, 0xba, 0x104, 0xdc, 0x50, 0, 0);
        task_run_all();
        return;
    }
    if (MI32(0x41f83c) == 2)
        gfx_blit(0x36, 0xba, 0x104, 0xdc, 0x50, 0, 0);
    task_run_all();
}

// 0x40c420: page arrows for the replay list (only when another list exists).
void f_40c420()
{
    FILE* f = std::fopen(STR_41e5e0, STR_41e5f8);   // save/daini_score.sav
    if (f) {
        f_4033f0(600, 0);
        f_4032a0(8, 0);
        std::fclose(f);
        return;
    }
    f = std::fopen(STR_41e5c8, STR_41e5f8);         // save/endless_score.sav
    if (f) {
        f_4033f0(0x256, 0);
        f_4032a0(10, 0);
        std::fclose(f);
    }
}

// ---------------------------------------------------------------------------
// Shared drawing of the replay list rows (inlined three times in the
// original).  Digits use surface 0x1f (image/score_s.bmp, 20x24 per digit).
// ---------------------------------------------------------------------------
static void draw_digits(int32_t x0, int32_t xstop, int32_t y, int32_t value)
{
    int32_t div = 1;
    int32_t x = x0;
    do {
        int32_t q = value / div;
        int32_t d = q % 10;
        if (q > 0 || x == x0)
            gfx_blit(0x1f, x, y + 4, 0x14, 0x18, d * 20, 0);
        div *= 10;
        x -= 0x14;
    } while (x > xstop);
}

// rows: `scores` = address of 10 {combo, score} pairs, `flags` = address of
// the per-row "replay file exists" bytes (index 1..10), `sel` = selection.
static void draw_replay_rows(uint32_t scores, uint32_t flags, int32_t sel)
{
    int32_t y = 0x7e;
    int32_t i = 1;
    for (uint32_t p = scores + 4; p < scores + 0x54; p += 8, y += 0x20, i++) {
        gfx_blit(0x22, 0x50, y, 0x20, 0x20, 0, 0);        // row number icon
        draw_digits(0x3c, 0x14, y, i);
        gfx_blit(0x21, 0xd2, y, 0x40, 0x20, 0, 0);        // "renzan" (combo)
        draw_digits(0xbe, 0x82, y, MI32(p - 4));
        gfx_blit(0x20, 0x1cc, y, 0x20, 0x20, 0, 0);       // "ten" (points)
        draw_digits(0x1b8, 0x118, y, MI32(p));
        if (sel == 0)
            gfx_blit(0x27, 0x208, 0x32, 0x60, 0x32, 0, 0);    // "modoru" selected
        else
            gfx_blit(0x26, 0x208, 0x32, 0x60, 0x32, 0, 0);
        if (MU8(flags + i)) {
            if (i == sel)
                gfx_blit(0x25, 0x208, y, 0x60, 0x20, 0, 0);   // "saiengi" selected
            else
                gfx_blit(0x24, 0x208, y, 0x60, 0x20, 0, 0);
        }
    }
}

// Cursor movement shared by the three list scenes (inlined in the original).
static void list_cursor_up(uint32_t selAddr, uint32_t flags)
{
    MI32(selAddr)--;
    snd_play(3);
    int32_t c = MI32(selAddr);
    if (c < 0) {
        c = 10;
        MI32(selAddr) = 10;
    } else if (c <= 0) {
        return;
    }
    while (MU8(flags + c) == 0) {
        c--;
        if (c <= 0)
            break;
    }
    MI32(selAddr) = c;
}

static void list_cursor_down(uint32_t selAddr, uint32_t flags)
{
    MI32(selAddr)++;
    snd_play(3);
    int32_t c = MI32(selAddr);
    if (c > 10) {
        MI32(selAddr) = 0;
        return;
    }
    if (c <= 0)
        return;
    for (;;) {
        if (MU8(flags + c)) {
            MI32(selAddr) = c;
            return;
        }
        c++;
        if (c > 10) {
            MI32(selAddr) = c;
            MI32(selAddr) = 0;
            return;
        }
        if (c <= 0) {
            MI32(selAddr) = c;
            return;
        }
    }
}

static bool file_exists_rb(const char* path)
{
    FILE* f = std::fopen(path, STR_41e5f8);
    if (!f)
        return false;
    std::fclose(f);
    return true;
}

// ===========================================================================
// 0x40c4a0: scene 9, replay list for 第一幕 (save/replay1..10.rep, scores
// from save/score.sav at 0x41f740).  Left/right switch to the other lists.
// MI32(0x41f844) init flag, MI32(0x41f790) selection (0 = back),
// MU8(0x41f818 + i) replay i exists, MU8(0x41f848) daini list exists,
// MU8(0x41f849) endless list exists.
// ===========================================================================
void f_40c4a0()
{
    if (MI32(0x41f844) == 0) {
        MI32(0x41f844) = 1;
        MU8(0x41f848) = 0;
        MU8(0x41f849) = 0;
        f_40ff30(&MI32(0x41f740));
        for (int i = 1; i < 11; i++) {
            char path[32];
            std::snprintf(path, sizeof(path), STR_41e5fc, i);
            MU8(0x41f818 + i) = file_exists_rb(path) ? 1 : 0;
        }
        if (file_exists_rb(STR_41e5e0))
            MU8(0x41f848) = 1;
        if (file_exists_rb(STR_41e5c8))
            MU8(0x41f849) = 1;
    }

    in_update();
    if (in_pressed(1)) {
        list_cursor_up(0x41f790, 0x41f818);
    } else if (in_pressed(3)) {
        list_cursor_down(0x41f790, 0x41f818);
    } else if (in_pressed(4)) {
        if (MU8(0x41f848)) {
            snd_play(3);
            MI32(0x41f798) = 10;
            MFN(0x41f79c) = f_40ca00;
        } else if (MU8(0x41f849)) {
            snd_play(3);
            MI32(0x41f798) = 11;
            MFN(0x41f79c) = f_40cf10;
        }
    } else if (in_pressed(2)) {
        if (MU8(0x41f849)) {
            snd_play(3);
            MI32(0x41f798) = 11;
            MFN(0x41f79c) = f_40cf10;
        } else if (MU8(0x41f848)) {
            snd_play(3);
            MI32(0x41f798) = 10;
            MFN(0x41f79c) = f_40ca00;
        }
    } else if (in_pressed(9)) {
        if (MI32(0x41f790) == 0) {
            MI32(0x41f798) = 1;
            MFN(0x41f79c) = f_40c080;
        } else {
            mus_load(STR_41e5b8, 0);
            mus_set_loop(0, 0, 0x21ef8);
            mus_play(0);
            f_40d410();
            f_40fb90(MI32(0x41f790));
            crt_srand(MU32(0x41f7b0));
            MI32(0x41f798) = 5;
            MFN(0x41f79c) = f_40e2d0;
        }
        snd_play(4);
        MI32(0x41f844) = 0;
    } else if (in_pressed(10)) {
        plat_quit();
    }

    gfx_clear_rgb(0x307896);
    gfx_blit(0x1e, 0xc8, 0x14, 0xf0, 0x5a, 0, 0);
    draw_replay_rows(0x41f740, 0x41f818, MI32(0x41f790));
    task_run_all();
}

// ===========================================================================
// 0x40ca00: scene 10, replay list for 第二幕 (save/replay11..20.rep, scores
// from save/daini_score.sav at 0x41f6f0).
// MI32(0x41f84c) init flag, MI32(0x41f824) selection, MU8(0x41f7a0 + i)
// replay exists, MU8(0x41f850) endless list exists.
// ===========================================================================
void f_40ca00()
{
    if (MI32(0x41f84c) == 0) {
        MI32(0x41f84c) = 1;
        MU8(0x41f850) = 0;
        f_4100b0(&MI32(0x41f6f0));
        for (int i = 1; i < 11; i++) {
            char path[32];
            std::snprintf(path, sizeof(path), STR_41e5fc, i + 10);
            MU8(0x41f7a0 + i) = file_exists_rb(path) ? 1 : 0;
        }
        if (file_exists_rb(STR_41e5c8))
            MU8(0x41f850) = 1;
    }

    in_update();
    if (in_pressed(1)) {
        list_cursor_up(0x41f824, 0x41f7a0);
    } else if (in_pressed(3)) {
        list_cursor_down(0x41f824, 0x41f7a0);
    } else if (in_pressed(2)) {
        snd_play(3);
        MI32(0x41f798) = 9;
        MFN(0x41f79c) = f_40c4a0;
    } else if (in_pressed(4)) {
        snd_play(3);
        if (MU8(0x41f850)) {
            MI32(0x41f798) = 11;
            MFN(0x41f79c) = f_40cf10;
        } else {
            MI32(0x41f798) = 9;
            MFN(0x41f79c) = f_40c4a0;
        }
    } else if (in_pressed(9)) {
        if (MI32(0x41f824) == 0) {
            MI32(0x41f798) = 1;
            MFN(0x41f79c) = f_40c080;
        } else {
            mus_load(STR_41e5a8, 0);
            mus_set_loop(0, 0x7b00, 0x3a500);
            mus_play(0);
            f_40d410();
            task_kill_all();
            task_run_all();
            f_40e750();
            f_40fb90(MI32(0x41f824) + 10);
            crt_srand(MU32(0x41f7b0));
            MI32(0x41f798) = 6;
            MFN(0x41f79c) = f_40ef60;
        }
        snd_play(4);
        MI32(0x41f84c) = 0;
    } else if (in_pressed(10)) {
        plat_quit();
    }

    gfx_clear_rgb(0x307896);
    gfx_blit(0x1e, 0xc8, 0x14, 0xf0, 0x5a, 0, 0x5a);
    draw_replay_rows(0x41f6f0, 0x41f7a0, MI32(0x41f824));
    task_run_all();
}

// ===========================================================================
// 0x40cf10: scene 11, replay list for とことん (save/replay21..30.rep, scores
// from save/endless_score.sav at 0x41f7c8).
// MI32(0x41f854) init flag, MI32(0x41f7ac) selection, MU8(0x41f678 + i)
// replay exists, MU8(0x41f858) daini list exists.
// ===========================================================================
void f_40cf10()
{
    if (MI32(0x41f854) == 0) {
        MI32(0x41f854) = 1;
        MU8(0x41f858) = 0;
        f_410230(&MI32(0x41f7c8));
        for (int i = 1; i < 11; i++) {
            char path[32];
            std::snprintf(path, sizeof(path), STR_41e5fc, i + 20);
            MU8(0x41f678 + i) = file_exists_rb(path) ? 1 : 0;
        }
        if (file_exists_rb(STR_41e5e0))
            MU8(0x41f858) = 1;
    }

    in_update();
    if (in_pressed(1)) {
        list_cursor_up(0x41f7ac, 0x41f678);
    } else if (in_pressed(3)) {
        list_cursor_down(0x41f7ac, 0x41f678);
    } else if (in_pressed(2)) {
        snd_play(3);
        if (MU8(0x41f858)) {
            MI32(0x41f798) = 10;
            MFN(0x41f79c) = f_40ca00;
        } else {
            MI32(0x41f798) = 9;
            MFN(0x41f79c) = f_40c4a0;
        }
    } else if (in_pressed(4)) {
        snd_play(3);
        MI32(0x41f798) = 9;
        MFN(0x41f79c) = f_40c4a0;
    } else if (in_pressed(9)) {
        if (MI32(0x41f7ac) == 0) {
            MI32(0x41f798) = 1;
            MFN(0x41f79c) = f_40c080;
        } else {
            mus_load(STR_41e5b8, 0);
            mus_set_loop(0, 0, 0x21ef8);
            mus_play(0);
            f_40f2d0();
            f_40fb90(MI32(0x41f7ac) + 20);
            crt_srand(MU32(0x41f7b0));
            MI32(0x41f798) = 7;
            MFN(0x41f79c) = f_40f850;
        }
        snd_play(4);
        MI32(0x41f854) = 0;
    } else if (in_pressed(10)) {
        plat_quit();
    }

    gfx_clear_rgb(0x307896);
    gfx_blit(0x1e, 0xc8, 0x14, 0xf0, 0x5a, 0, 0xb4);
    draw_replay_rows(0x41f7c8, 0x41f678, MI32(0x41f7ac));
    task_run_all();
}

// ===========================================================================
// 0x40d410: reset the game state and start 第一幕 (scene 2 = 0x40d560).
// Also used by the other modes / replays before their own setup.
// ===========================================================================
void f_40d410()
{
    MI32(0x41f798) = 2;
    MFN(0x41f79c) = f_40d560;
    f_401100();
    f_4085f0(0x1e0, 5, 1);
    f_4085f0(0xf0, -0x384, 1);
    f_4085f0(0x78, -0x2bc, 1);
    f_4085f0(0x280, -0x1f4, 1);
    f_4085f0(-0x64, -0x12c, 1);
    f_4085f0(0x12c, -0x64, 1);
    MTASK(0x41f6cc) = f_401cc0();          // GS+0x44: player task
    f_408ae0();
    MI32(0x41f68c) = 0x280;                // GS+0x04
    MI32(0x41f688) = 0;                    // GS+0x00 camera x
    MI32(0x41f690) = 0;                    // GS+0x08 camera y
    MI32(0x41f694) = 0x1e0;                // GS+0x0c camera y + 480
    f_408df0();
    MI32(0x41f698) = 0;
    MI32(0x41f6c4) = 9999999;
    MI32(0x41f69c) = 0;
    MI32(0x41f6a0) = 0;
    MI32(0x41f6a8) = 0;
    MI32(0x41f6ac) = 0;
    MU8(0x41f6b0) = 0;
    MU8(0x41f6b1) = 0;
    MI32(0x41f6b4) = 0;
    MI32(0x41f6a4) = 0;
    // replay input vector: erase(begin(), end())
    MU32(0x41f7b0) = 0;
    MU8(0x41f6b8) = 0;
    MU8(0x41f6b9) = 0;
    MU8(0x41f6ba) = 0;
    MI32(0x41f6c0) = 0;
    MPTR(0x41f7bc) = MPTR(0x41f7b8);
    MI32(0x41f6bc) = -1;
}
