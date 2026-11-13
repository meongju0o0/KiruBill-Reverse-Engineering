// Part A: original functions 0x401000 .. 0x403a30
// Background, HUD, buildings (stage 1/2 style), camera tasks, player
// character state machine, animation helpers, score popups, title arrows,
// stage-2 boss building, banners and screen flash.
//
// Translated 1:1 from the disassembly of 시작.exe (see
// docs/TRANSLATION_CONVENTIONS.md and docs/notes/part_a.md).
#include "game.h"

// (functions of other parts are declared in game.h)

// ---------------------------------------------------------------------------
// small helpers for repeated x87 idioms
// ---------------------------------------------------------------------------

// fld [t+0x38]; fadd add; fst [t+0x38]; fcomp 100.0; test ah,0x41; jne
// (defense gauge regeneration, clamped to 100)
static inline void def_regen(Task* t, float add)
{
    double s = (double)TF32(t, 0x38) + (double)add;
    TF32(t, 0x38) = (float)s;
    if (s > 100.0) TF32(t, 0x38) = 100.0f;
}

// Draw the 128x128 player sprite cell (sx,sy) of char.bmp (surface 0).
static inline void draw_player(Task* t, int32_t sx, int32_t sy)
{
    int32_t x = TI32(t, 0x20) - MI32(GS + 0x0);
    int32_t y = TI32(t, 0x24) - MI32(GS + 0x8);
    gfx_blit(0, x, y, 0x80, 0x80, sx, sy);
}

// Draw the current frame of an animation record (0x41f468 + n*0x68).
static inline void draw_player_anim(Task* t, uint32_t anim)
{
    int32_t x = TI32(t, 0x20) - MI32(GS + 0x0);
    int32_t y = TI32(t, 0x24) - MI32(GS + 0x8);
    int32_t cur = MI32(anim + 0x50);
    gfx_blit(0, x, y, 0x80, 0x80, MI32(anim + cur * 8), MI32(anim + cur * 8 + 4));
}

// Lane (0..2) to player x: 120*lane + 136
static inline int32_t lane_x(int32_t lane) { return lane * 120 + 0x88; }

// ---------------------------------------------------------------------------
// 0x401000: create the stage 1 background task (prio 1)
// ---------------------------------------------------------------------------
void f_401000()
{
    Task d; task_desc(&d);
    d.prio = 1;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_401040;
    task_create(&d);
}

// 0x401040: stage 1 background (surface 11 = data/002.dat) scrolled by the
// camera; sky color fill above it; random debris high above the stage.
void f_401040(Task* /*t*/)
{
    int32_t x = -MI32(GS + 0x0);
    int32_t camy = MI32(GS + 0x8);
    int32_t h = 0;          // esi: rows of sky above the background
    int32_t sy = camy;      // ebp
    if (camy < 0) {
        h = -camy;
        sy = 0;
    }
    if (camy > -0x1e0) {
        gfx_blit(0xb, x, h, 0x280, 0x1e0 - h, 0, sy);
        gfx_fill(0, 0, 0x280, h, MU32(GS + 0x50));
    } else {
        gfx_fill(0, 0, 0x280, 0x1e0, MU32(GS + 0x50));
    }
    if (camy < -0x5a0) {
        if (crt_rand() % 10 == 0) {
            int32_t y = camy - 0x78;
            int32_t rx = crt_rand() % 0x280;
            f_4085f0(rx, y, 0);
        }
    }
}

// 0x401100: create the stage 2 background task (prio 2)
void f_401100()
{
    Task d; task_desc(&d);
    d.prio = 2;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_401140;
    task_create(&d);
}

// 0x401140: stage 2 / endless background (surface 10 = bg.bmp 640x1440)
void f_401140(Task* /*t*/)
{
    int32_t x = -MI32(GS + 0x0);
    int32_t camy = MI32(GS + 0x8);
    int32_t h = 0;              // esi
    int32_t sy = camy + 0x3c0;  // edi
    if (sy < 0) {
        h = -sy;
        sy = 0;
    }
    if (camy > -0x5a0) {
        gfx_blit(0xa, x, h, 0x280, 0x1e0 - h, 0, sy);
        gfx_fill(0, 0, 0x280, h, MU32(GS + 0x48));
        return;
    }
    gfx_fill(0, 0, 0x280, 0x1e0, MU32(GS + 0x48));
    if (crt_rand() % 10 == 0) {
        int32_t y = camy - 0x78;
        int32_t rx = crt_rand() % 0x280;
        f_4085f0(rx, y, 0);
    }
}

// 0x401210: create the HUD task (prio 0x20) attached to the player
void f_401210(Task* parent)
{
    Task d; task_desc(&d);
    d.prio = 0x20;
    d.f04 = 0;
    d.wait = 0;
    d.parent = parent;
    d.fn = f_401250;
    TU8(&d, 0x20) = 0;
    task_create(&d);
}

// 0x401250: HUD: technique gauge (GS+0x1c, 0..300), defense gauge (player
// float +0x38, 0..100) and the "bougyo" label.
void f_401250(Task* t)
{
    int32_t defw = ftol((double)TF32(t->parent, 0x38));
    int32_t gauge = (MI32(GS + 0x1c) * 50) / 300;
    if (gauge == 50) {
        // full gauge blinks between wazabar and wazabar2
        MI32(0x41f460) = MI32(0x41f460) + 1;
        if (MI32(0x41f460) % 3 == 0)
            MU8(0x41e050) = (MU8(0x41e050) == 0);
        if (MU8(0x41e050))
            gfx_blit(0x1d, 0x41, 0x1c0, 0x32, 4, 0, 0);
        else
            gfx_blit(0x1c, 0x41, 0x1c0, 0x32, 4, 0, 0);
    } else {
        gfx_blit(0x1c, 0x41, 0x1c0, gauge, 4, 0, 0);
    }
    gfx_blit(0x1b, 0x38, 0x1ca, defw, 0x10, 0, 0);
    gfx_blit(7, 2, 0x1b2, 0xa0, 0x30, 0, 0);
}

// 0x401340: spawn a building (stage 1/2 style, 3 columns per floor).
//   gravity  : fall acceleration (fixed point, >>6)
//   floors   : number of random middle floors (roof + floors + ground)
//   kind     : graphic set 0..2 (surface 4+kind = bil10/20/30.bmp)
//   w1,w2,w3 : relative weights of floor types 1,2,3
// Returns the building task.
Task* f_401340(int32_t gravity, int32_t floors, int32_t kind, int32_t w1, int32_t w2, int32_t w3)
{
    int32_t y0 = MI32(GS + 0x8) - floors * 118 - 0xee;

    Task d; task_desc(&d);
    d.prio = 0x10;
    d.f04 = 0;
    d.wait = 0x1e;
    d.parent = nullptr;
    d.fn = f_401530;
    TI32(&d, 0x20) = y0;            // y (top)
    TI32(&d, 0x24) = 0;             // vy
    TI32(&d, 0x28) = y0 * 64;       // y << 6
    TI32(&d, 0x2c) = gravity;       // gravity
    TI32(&d, 0x30) = floors + 2;    // floors left (roof + floors + ground)
    TI32(&d, 0x34) = kind;          // graphic set
    TU8(&d, 0x38) = 0;              // hit flag
    TI32(&d, 0x3c) = 0;             // hit column
    TI32(&d, 0x40) = 0;             // hit damage
    Task* bld = task_create(&d);

    int32_t sum = w1 + w2 + w3;
    if (sum == 0) {
        sum = 1;
        w1 = 0;
        w2 = 1;
    }
    int32_t p1 = (w1 * 100) / sum;
    int32_t p2 = (w2 * 100) / sum;

    // floor descriptor: [0]=index, [1..3]=sprite y per column,
    // [4..6]=hit points per column, [7..9]=type per column
    int32_t S[10];
    std::memset(S, 0, sizeof(S));
    for (int i = 0; i < 3; i++) {
        S[4 + i] = 2;
        S[7 + i] = 2;
        S[1 + i] = 0;
    }
    S[0] = 0;
    f_4015c0(S, bld);   // roof

    for (int32_t k = floors; k > 0; k--) {
        for (int i = 0; i < 3; i++) {
            int32_t r = crt_rand() % 100;
            if (p1 > r)
                S[4 + i] = 1;
            else
                S[4 + i] = ((p2 + p1) <= r) + 2;
            S[1 + i] = (4 - S[4 + i]) * 120;
            S[7 + i] = S[4 + i];
        }
        S[0] = S[0] + 1;
        f_4015c0(S, bld);
    }

    for (int i = 0; i < 3; i++) {
        S[4 + i] = 2;
        S[7 + i] = 2;
        S[1 + i] = 0x1e0;
    }
    S[0] = S[0] + 1;
    f_4015c0(S, bld);   // ground floor
    return bld;
}

// 0x401530: building falling / landing (ground at y 428, floor height 118)
void f_401530(Task* t)
{
    int32_t floors = TI32(t, 0x30);
    if (floors == 0) {
        t->dead = 1;
        return;
    }
    if (t->wait > 0)
        return;
    int32_t vy = TI32(t, 0x24);
    int32_t yy = TI32(t, 0x28) + vy;
    TI32(t, 0x28) = yy;
    int32_t y = yy >> 6;
    TI32(t, 0x20) = y;
    if (y + floors * 118 + 2 >= 0x1ac) {
        if (vy > TI32(t, 0x2c)) {
            f_40aae0();
            f_401bc0();
        }
        TI32(t, 0x24) = 0;
        int32_t ny = 0x1aa - TI32(t, 0x30) * 118;
        TI32(t, 0x20) = ny;
        TI32(t, 0x28) = ny * 64;   // shl 6
    }
    TI32(t, 0x24) = TI32(t, 0x24) + TI32(t, 0x2c);
}

// 0x4015c0: create one floor task (prio 0x10) of building `parent`.
// S = 10 dwords copied to +0x20.
Task* f_4015c0(const int32_t* S, Task* parent)
{
    Task d; task_desc(&d);
    d.prio = 0x10;
    d.f04 = 0;
    d.wait = 0;
    d.parent = parent;
    d.fn = f_401620;
    for (int i = 0; i < 10; i++)
        TI32(&d, 0x20 + 4 * i) = S[i];
    return task_create(&d);
}

// 0x401620: floor of a building: takes hits on the bottom floor, scores,
// is destroyed when a column's hit points reach 0; draws 3 columns.
void f_401620(Task* t)
{
    Task* P = t->parent;
    if (TU8(P, 0x38) == 1 && TI32(P, 0x30) - 1 == TI32(t, 0x20)) {
        int32_t col = TI32(P, 0x3c);
        TI32(t, 0x30 + col * 4) = TI32(t, 0x30 + col * 4) - TI32(P, 0x40);
        TU8(P, 0x38) = 0;
        if (TI32(t, 0x30 + TI32(P, 0x3c) * 4) <= 0) {
            TI32(P, 0x30) = TI32(P, 0x30) - 1;
            t->dead = 1;
            int32_t c10 = MI32(GS + 0x14) * 10;
            int32_t col2 = TI32(P, 0x3c);
            double a = (double)c10 * (double)TI32(t, 0x3c + col2 * 4);
            double b = (double)TI32(P, 0x2c) * (double)1.5f;
            MI32(GS + 0x10) = ftol(a * b + (double)MI32(GS + 0x10));
            if (MI32(GS + 0x10) > MI32(GS + 0x3c))
                MI32(GS + 0x10) = MI32(GS + 0x3c);
            int32_t idx = TI32(t, 0x20);
            f_4040b0(0x140, TI32(P, 0x20) + idx * 118);
            snd_play(7);
            return;
        } else {
            double b = (double)TI32(P, 0x2c) * (double)1.5f;
            MI32(GS + 0x10) = ftol((double)MI32(GS + 0x14) * b + (double)MI32(GS + 0x10));
            if (MI32(GS + 0x10) > MI32(GS + 0x3c))
                MI32(GS + 0x10) = MI32(GS + 0x3c);
        }
    }
    int32_t surf = TI32(P, 0x34) + 4;
    int32_t py = TI32(P, 0x20);
    int32_t x0 = 0x50 - MI32(GS + 0x0);
    int32_t y = TI32(t, 0x20) * 118 - MI32(GS + 0x8) + py;
    int off = 0x24;
    for (int32_t i = 0; i < 0x1e0; i += 0xa0, off += 4)
        gfx_blit(surf, i + x0, y, 0xa0, 0x78, i, TI32(t, off));
}

// 0x4017e0: create the jump camera task (prio 0x40) following `parent`
void f_4017e0(Task* parent)
{
    Task d; task_desc(&d);
    d.prio = 0x40;
    d.f04 = 0;
    d.wait = 0;
    d.parent = parent;
    d.fn = f_401920;
    TI32(&d, 0x20) = 0;     // camera y (float 0.0)
    TI32(&d, 0x24) = 0;     // camera velocity (float 0.0)
    TU8(&d, 0x28) = 0;      // "locked" flag
    task_create(&d);
}

// 0x401850: camera while the player falls (follows player float y +0x40)
void f_401850(Task* t)
{
    Task* P = t->parent;
    if (t->wait > 0)
        return;
    if (TU8(t, 0x28)) {
        TF32(t, 0x20) = (float)((double)TF32(P, 0x40) - (double)304.0f);
    } else {
        TF32(t, 0x20) = (float)((double)TF32(t, 0x24) + (double)TF32(t, 0x20));
        float v = (float)((double)TF32(t, 0x24) + (double)0.05f);
        TF32(t, 0x24) = v;
        if ((double)TF32(P, 0x40) - (double)304.0f > (double)TF32(t, 0x20)) {
            TU8(t, 0x28) = 1;
        } else if (!((double)TF32(P, 0x40) - (double)200.0f >= (double)TF32(t, 0x20))) {
            TF32(t, 0x24) = (float)((double)v - (double)0.05f);
        }
    }
    if (TI32(P, 0x50) == 0) {
        TI32(t, 0x20) = 0;
        t->dead = 1;
    }
    if ((double)TF32(t, 0x20) > (double)0.0f)
        TI32(t, 0x20) = 0;
    MI32(GS + 0x8) = ftol((double)TF32(t, 0x20));
    MI32(GS + 0xc) = MI32(GS + 0x8) + 0x1e0;
}

// 0x401920: camera while the player rises (player vy +0x44 < -2)
void f_401920(Task* t)
{
    Task* P = t->parent;
    if (t->wait > 0)
        return;
    if (!((double)TF32(P, 0x44) >= (double)-2.0f)) {
        if (TU8(t, 0x28)) {
            TF32(t, 0x20) = (float)((double)TF32(P, 0x40) - (double)250.0f);
        } else {
            TF32(t, 0x20) = (float)((double)TF32(t, 0x24) + (double)TF32(t, 0x20));
            TF32(t, 0x24) = (float)((double)TF32(t, 0x24) - (double)0.4f);
            if (!((double)TF32(P, 0x40) - (double)250.0f >= (double)TF32(t, 0x20)))
                TU8(t, 0x28) = 1;
        }
    } else {
        TU8(t, 0x28) = 0;
        TI32(t, 0x24) = 0;
        t->fn = f_401850;
    }
    MI32(GS + 0x8) = ftol((double)TF32(t, 0x20));
    MI32(GS + 0xc) = MI32(GS + 0x8) + 0x1e0;
}

// 0x4019d0: start a "look at" camera scroll to y (pauses tasks with mask
// 0x7f while it runs). Returns nullptr if the camera is already above.
Task* f_4019d0(int32_t y)
{
    int32_t camx = MI32(GS + 0x0);
    int32_t camy = MI32(GS + 0x8);
    task_pause_mask(0x7f);
    if (camy < y - 0x96)
        return nullptr;
    Task d; task_desc(&d);
    d.prio = 0x20;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_401a70;
    TI32(&d, 0x20) = camx;
    TI32(&d, 0x24) = camy;          // camera y to return to
    TI32(&d, 0x28) = 0;             // hold counter
    TI32(&d, 0x2c) = 0;             // shake counter
    TU8(&d, 0x30) = 1;              // scrolling up
    TI32(&d, 0x34) = y - 0x96;      // target y
    return task_create(&d);
}

// 0x401a70: camera scroll task created by f_4019d0
void f_401a70(Task* t)
{
    task_pause_mask(0x7f);
    if (TI32(t, 0x2c) < 4) {
        TI32(t, 0x2c) = TI32(t, 0x2c) + 1;
        MI32(GS + 0x8) = MI32(GS + 0x8) + 0x14;
        if (MI32(GS + 0x8) > 0)
            MI32(GS + 0x8) = 0;
        MI32(GS + 0xc) = MI32(GS + 0x8) + 0x1e0;
    }
    if (TU8(t, 0x30)) {
        if (MI32(GS + 0x8) > TI32(t, 0x34)) {
            MI32(GS + 0x8) = MI32(GS + 0x8) - 0x14;
            MI32(GS + 0xc) = MI32(GS + 0x8) + 0x1e0;
            return;
        }
        TU8(t, 0x30) = 0;
        MI32(GS + 0x8) = TI32(t, 0x34);
        MI32(GS + 0xc) = TI32(t, 0x34) + 0x1e0;
        return;
    }
    if (TI32(t, 0x28) < 0x14) {
        TI32(t, 0x28) = TI32(t, 0x28) + 1;
        return;
    }
    if (MI32(GS + 0x8) < TI32(t, 0x24)) {
        MI32(GS + 0x8) = MI32(GS + 0x8) + 0x28;
        MI32(GS + 0xc) = MI32(GS + 0x8) + 0x1e0;
        if (MI32(GS + 0x8) <= 0)
            return;
        MI32(GS + 0x8) = 0;
        t->dead = 1;
    } else {
        t->dead = 1;
        MI32(GS + 0x8) = TI32(t, 0x24);
    }
    MI32(GS + 0xc) = MI32(GS + 0x8) + 0x1e0;
    task_resume_mask(0x7f);
}

// 0x401bc0: create a screen shake task (prio 0x10); 14 x offsets
Task* f_401bc0()
{
    static const int32_t D[16] = {
        -20, 2, -10, 20, -4, 13, 2, -10, 0, 8, -2, 4, -2, 1, 0, 0
    };
    Task d; task_desc(&d);
    d.prio = 0x10;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_401c80;
    for (int i = 0; i < 16; i++)
        TI32(&d, 0x20 + 4 * i) = D[i];
    return task_create(&d);
}

// 0x401c80: screen shake step (camera x = table[i], GS+4 = table[i]+640)
void f_401c80(Task* t)
{
    int32_t i = TI32(t, 0x5c) + 1;
    TI32(t, 0x5c) = i;
    if (i > 0xe) {
        t->dead = 1;
        return;
    }
    MI32(GS + 0x0) = TI32(t, 0x20 + i * 4);
    MI32(GS + 0x4) = TI32(t, 0x20 + TI32(t, 0x5c) * 4) + 0x280;
}

// 0x401cc0: create the player task (prio 8) and its animation tables.
// Animation records (0x68 bytes each, frames are (sx,sy) pairs in
// char.bmp): +0x50 frame, +0x54 tick, +0x58 ticks per frame, +0x5c loop,
// +0x5d finished, +0x60 loop frame, +0x64 last frame.
Task* f_401cc0()
{
    MI32(0x41f528) = 3;

    // anim 0 @0x41f468: ground slash (5 frames, row 0)
    MI32(0x41f468) = 0;     MI32(0x41f46c) = 0;
    MI32(0x41f470) = 0x80;  MI32(0x41f474) = 0;
    MI32(0x41f478) = 0x100; MI32(0x41f47c) = 0;
    MI32(0x41f480) = 0x180; MI32(0x41f484) = 0;
    MI32(0x41f488) = 0x200; MI32(0x41f48c) = 0;
    MI32(0x41f4bc) = 0;
    MI32(0x41f4b8) = 0;
    MI32(0x41f4c0) = 1;
    MU8(0x41f4c4) = 0;
    MU8(0x41f4c5) = 0;
    MI32(0x41f4c8) = 0;
    MI32(0x41f4cc) = 4;

    // anim 1 @0x41f4d0: jump crouch (3 frames, row 1)
    MI32(0x41f4d0) = 0;
    MI32(0x41f4d4) = 0x80;
    MI32(0x41f4d8) = 0x80;
    MI32(0x41f4dc) = 0x80;
    MI32(0x41f4e0) = 0x100;
    MI32(0x41f4e4) = 0x80;
    MI32(0x41f524) = 0;
    MI32(0x41f520) = 0;
    MU8(0x41f52c) = 0;
    MU8(0x41f52d) = 0;
    MI32(0x41f530) = 0;
    MI32(0x41f534) = 2;

    // anim 2 @0x41f538: air slash (5 frames, row 2)
    MI32(0x41f538) = 0;
    MI32(0x41f53c) = 0x100;
    MI32(0x41f540) = 0x80;
    MI32(0x41f544) = 0x100;
    MI32(0x41f548) = 0x100;
    MI32(0x41f54c) = 0x100;
    MI32(0x41f550) = 0x180;
    MI32(0x41f554) = 0x100;
    MI32(0x41f558) = 0x200;
    MI32(0x41f55c) = 0x100;
    MI32(0x41f58c) = 0;
    MI32(0x41f588) = 0;
    MI32(0x41f590) = 1;
    MU8(0x41f594) = 0;
    MU8(0x41f595) = 0;
    MI32(0x41f598) = 0;
    MI32(0x41f59c) = 4;

    // anim 3 @0x41f5a0: ground special technique (9 frames)
    MI32(0x41f5a0) = 0;
    MI32(0x41f5a4) = 0;
    MI32(0x41f5a8) = 0x80;
    MI32(0x41f5ac) = 0;
    MI32(0x41f5b0) = 0x100;
    MI32(0x41f5b4) = 0;
    MI32(0x41f5b8) = 0x180;
    MI32(0x41f5bc) = 0;
    MI32(0x41f5c0) = 0x200;
    MI32(0x41f5c4) = 0;
    MI32(0x41f5c8) = 0;
    MI32(0x41f5d0) = 0x80;
    MI32(0x41f5d8) = 0x100;
    MI32(0x41f5f4) = 0;
    MI32(0x41f5f0) = 0;
    MU8(0x41f5fc) = 0;
    MU8(0x41f5fd) = 0;
    MI32(0x41f600) = 0;

    // anim 4 @0x41f608: air special technique (9 frames)
    MI32(0x41f608) = 0;
    MI32(0x41f60c) = 0x100;
    MI32(0x41f610) = 0x80;
    MI32(0x41f614) = 0x100;
    MI32(0x41f618) = 0x100;
    MI32(0x41f61c) = 0x100;
    MI32(0x41f624) = 0x100;
    MI32(0x41f628) = 0x200;
    MI32(0x41f62c) = 0x100;
    MI32(0x41f630) = 0;
    MI32(0x41f638) = 0x80;
    MI32(0x41f640) = 0x100;
    MI32(0x41f65c) = 0;
    MI32(0x41f658) = 0;
    MU8(0x41f664) = 0;
    MU8(0x41f665) = 0;
    MI32(0x41f668) = 0;

    // player descriptor
    Task d; task_desc(&d);
    d.prio = 8;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_401fb0;
    TI32(&d, 0x20) = 0x100;     // x
    TI32(&d, 0x24) = 0x130;     // y
    TI32(&d, 0x28) = 1;         // lane 0..2
    TI32(&d, 0x2c) = 3;         // life
    TI32(&d, 0x30) = 0;         // invincibility / technique timer
    TI32(&d, 0x34) = 0;         // counter
    TF32(&d, 0x38) = 100.0f;    // defense gauge
    TF32(&d, 0x3c) = 256.0f;    // x (float)
    TF32(&d, 0x40) = 304.0f;    // y (float)
    TI32(&d, 0x44) = 0;         // vy (float 0.0)
    TI32(&d, 0x48) = 0;         // vx (float 0.0)
    TI32(&d, 0x4c) = 0;         // current animation index (uninitialised in the original)
    TI32(&d, 0x50) = 0;         // state
    TI32(&d, 0x54) = 0;         // counter

    // (stored between the push of the descriptor and the call)
    MI32(0x41f5cc) = 0x180;
    MI32(0x41f5d4) = 0x180;
    MI32(0x41f5dc) = 0x180;
    MI32(0x41f5e0) = 0x180;
    MI32(0x41f5e4) = 0x180;
    MI32(0x41f5f8) = 2;
    MI32(0x41f604) = 8;
    MI32(0x41f620) = 0x180;
    MI32(0x41f634) = 0x180;
    MI32(0x41f63c) = 0x180;
    MI32(0x41f644) = 0x180;
    MI32(0x41f648) = 0x180;
    MI32(0x41f64c) = 0x180;
    MI32(0x41f660) = 2;
    MI32(0x41f66c) = 8;

    Task* p = task_create(&d);
    f_4086e0(p);
    f_401210(p);
    return p;
}

// 0x401fb0: player standing (state 0): reads the controls
void f_401fb0(Task* t)
{
    if (t->wait != 0)
        goto draw;
    def_regen(t, 0.25f);
    if (in_pressed(5)) {                    // attack
        TI32(t, 0x50) = 1;
        TI32(t, 0x4c) = 0;
        f_402fd0(0x41f468);
        t->fn = f_402770;
    }
    if (in_held(7) && (double)TF32(t, 0x38) > (double)10.0f) {  // guard
        TI32(t, 0x50) = 7;
        t->fn = f_402450;
        goto draw;
    }
    if (in_held(6)) {                       // jump
        TI32(t, 0x50) = 2;
        TI32(t, 0x4c) = 1;
        TF32(t, 0x44) = -14.0f;
        TF32(t, 0x3c) = (float)(double)TI32(t, 0x20);
        TF32(t, 0x40) = (float)(double)TI32(t, 0x24);
        f_402fd0(0x41f4d0);
        t->fn = f_402820;
        goto draw;
    }
    if (TI32(t, 0x28) > 0 && in_pressed(2)) {   // move left
        TF32(t, 0x3c) = (float)(double)TI32(t, 0x20);
        TI32(t, 0x50) = 0xb;
        TF32(t, 0x48) = -4.0f;
        TI32(t, 0x28) = TI32(t, 0x28) - 1;
        t->fn = f_4022d0;
        f_408230(TI32(t, 0x20) + 0x19, TI32(t, 0x24) + 0x3c);
        snd_play(8);
        goto draw;
    }
    if (TI32(t, 0x28) < 2 && in_pressed(4)) {   // move right
        TF32(t, 0x3c) = (float)(double)TI32(t, 0x20);
        TI32(t, 0x50) = 0xb;
        TF32(t, 0x48) = 4.0f;
        TI32(t, 0x28) = TI32(t, 0x28) + 1;
        t->fn = f_402390;
        f_408230(TI32(t, 0x20) + 0x19, TI32(t, 0x24) + 0x3c);
        snd_play(8);
        goto draw;
    }
    if (in_held(8) && MI32(GS + 0x1c) >= 0x12c) {   // special technique
        MI32(GS + 0x1c) = 0;
        TI32(t, 0x50) = 5;
        TI32(t, 0x4c) = 3;
        TI32(t, 0x30) = 0x3c;
        f_402fd0(0x41f5a0);
        t->fn = f_402b60;
        snd_play(11);
    }
draw:
    draw_player(t, 0, 0);
}

// 0x4021d0: player standing variant (snapped to lane, faster defense
// regeneration, no drawing)
void f_4021d0(Task* t)
{
    TI32(t, 0x24) = 0x130;
    TF32(t, 0x40) = 304.0f;
    TI32(t, 0x20) = lane_x(TI32(t, 0x28));
    if (t->wait != 0)
        return;
    def_regen(t, 2.5f);
    if (in_pressed(5)) {
        TI32(t, 0x50) = 1;
        TI32(t, 0x4c) = 0;
        f_402fd0(0x41f468);
        t->fn = f_402770;
    }
    if (in_held(7) && (double)TF32(t, 0x38) > (double)10.0f) {
        TI32(t, 0x50) = 7;
        t->fn = f_402450;
    }
    if (in_pressed(8) && MI32(GS + 0x1c) >= 0x12c) {
        MI32(GS + 0x1c) = 0;
        TI32(t, 0x50) = 5;
        TI32(t, 0x4c) = 3;
        TI32(t, 0x30) = 0x3c;
        f_402fd0(0x41f5a0);
        t->fn = f_402b60;
    }
}

// 0x4022d0: player moving one lane to the left
void f_4022d0(Task* t)
{
    if (t->wait == 0) {
        def_regen(t, 0.25f);
        TF32(t, 0x3c) = (float)((double)TF32(t, 0x3c) - (double)TF32(t, 0x48));
        TF32(t, 0x48) = (float)((double)TF32(t, 0x48) + (double)5.0f);
        int32_t x = ftol((double)TF32(t, 0x3c));
        TI32(t, 0x20) = x;
        int32_t target = lane_x(TI32(t, 0x28));
        if (x < target) {
            TI32(t, 0x20) = target;
            TI32(t, 0x50) = 0;
            t->fn = f_401fb0;
        }
    }
    draw_player(t, 0x200, 0x80);
}

// 0x402390: player moving one lane to the right
void f_402390(Task* t)
{
    if (t->wait == 0) {
        def_regen(t, 0.25f);
        TF32(t, 0x3c) = (float)((double)TF32(t, 0x3c) + (double)TF32(t, 0x48));
        TF32(t, 0x48) = (float)((double)TF32(t, 0x48) + (double)5.0f);
        int32_t x = ftol((double)TF32(t, 0x3c));
        TI32(t, 0x20) = x;
        int32_t target = lane_x(TI32(t, 0x28));
        if (x > target) {
            TI32(t, 0x20) = target;
            TI32(t, 0x50) = 0;
            t->fn = f_401fb0;
        }
    }
    draw_player(t, 0x200, 0x80);
}

// 0x402450: player guarding on the ground (state 7), drains the gauge
void f_402450(Task* t)
{
    if (t->wait == 0) {
        double s = (double)TF32(t, 0x38) - (double)1.0f;
        TF32(t, 0x38) = (float)s;
        if (!(s >= 0.0))
            TF32(t, 0x38) = 0.0f;
        if (!in_held(7) || !((double)TF32(t, 0x38) < 0.0 || (double)TF32(t, 0x38) > 0.0)) {
            TI32(t, 0x50) = 0;
            t->fn = f_401fb0;
        }
    }
    draw_player(t, 0x200, 0x180);
}

// 0x4024f0: player blocked a hit on the ground (guard recoil)
void f_4024f0(Task* t)
{
    if (t->wait == 0) {
        double s = (double)TF32(t, 0x38) - (double)4.0f;
        TF32(t, 0x38) = (float)s;
        if (!(s >= 0.0)) {
            TF32(t, 0x38) = 0.0f;
            TI32(t, 0x34) = 0;
            TI32(t, 0x50) = 0;
            t->fn = f_401fb0;
        }
        int32_t c = TI32(t, 0x34) + 1;
        TI32(t, 0x34) = c;
        if (c > 8) {
            TI32(t, 0x34) = 0;
            TI32(t, 0x50) = 7;
            t->fn = f_402450;
        }
    }
    draw_player(t, 0, 0x200);
}

// 0x402590: player blocked a hit in the air (guard recoil)
void f_402590(Task* t)
{
    if (t->wait == 0) {
        double s = (double)TF32(t, 0x38) - (double)4.0f;
        TF32(t, 0x38) = (float)s;
        if (!(s >= 0.0)) {
            TF32(t, 0x38) = 0.0f;
            TI32(t, 0x34) = 0;
            TI32(t, 0x50) = 3;
            t->fn = f_4028f0;
        }
        TF32(t, 0x40) = (float)((double)TF32(t, 0x40) + (double)TF32(t, 0x44));
        TF32(t, 0x44) = (float)((double)TF32(t, 0x44) + (double)0.1f);
        if ((double)TF32(t, 0x40) > (double)304.0f)
            TF32(t, 0x40) = 304.0f;
        TI32(t, 0x24) = ftol((double)TF32(t, 0x40));
        int32_t c = TI32(t, 0x34) + 1;
        TI32(t, 0x34) = c;
        if (c > 8) {
            TI32(t, 0x34) = 0;
            TI32(t, 0x50) = 0xa;
            t->fn = f_402670;
        }
    }
    draw_player(t, 0, 0x200);
}

// 0x402670: player guarding in the air (state 10)
void f_402670(Task* t)
{
    if (t->wait == 0) {
        double s = (double)TF32(t, 0x38) - (double)1.0f;
        TF32(t, 0x38) = (float)s;
        if (!(s >= 0.0))
            TF32(t, 0x38) = 0.0f;
        TF32(t, 0x40) = (float)((double)TF32(t, 0x40) + (double)TF32(t, 0x44));
        TF32(t, 0x44) = (float)((double)TF32(t, 0x44) + (double)0.1f);
        TI32(t, 0x24) = ftol((double)TF32(t, 0x40));
        if (!in_held(7) || !((double)TF32(t, 0x38) < 0.0 || (double)TF32(t, 0x38) > 0.0)) {
            TI32(t, 0x50) = 3;
            t->fn = f_4028f0;
        }
        if (TI32(t, 0x24) > 0x127) {     // landed while guarding
            TI32(t, 0x50) = 7;
            TI32(t, 0x24) = 0x130;
            t->fn = f_402450;
            f_408230(TI32(t, 0x20) + 0x19, TI32(t, 0x24) + 0x3c);
            snd_play(10);
        }
    }
    draw_player(t, 0x200, 0x180);
}

// 0x402770: player ground slash (state 1), animation 0
void f_402770(Task* t)
{
    uint32_t anim = 0x41f468 + TI32(t, 0x4c) * 0x68;
    if (t->wait == 0) {
        def_regen(t, 0.25f);
        f_402f90(anim);
        if (MU8(anim + 0x5d)) {
            TI32(t, 0x50) = 0;
            t->fn = f_401fb0;
            MI32(anim + 0x50) = MI32(anim + 0x50) - 1;
        }
    }
    draw_player_anim(t, anim);
}

// 0x402820: player jump crouch (state 2), then takes off
void f_402820(Task* t)
{
    uint32_t anim = 0x41f468 + TI32(t, 0x4c) * 0x68;
    if (t->wait == 0) {
        def_regen(t, 0.25f);
        f_402f90(anim);
        if (MU8(anim + 0x5d)) {
            TI32(t, 0x50) = 3;
            t->fn = f_4028f0;
            f_408230(TI32(t, 0x20) + 0x19, TI32(t, 0x24) + 0x3c);
            f_4017e0(t);
            MI32(anim + 0x50) = MI32(anim + 0x50) - 1;
            snd_play(9);
        }
    }
    draw_player_anim(t, anim);
}

// 0x4028f0: player airborne (state 3)
void f_4028f0(Task* t)
{
    if (t->wait == 0) {
        def_regen(t, 0.25f);
        TF32(t, 0x40) = (float)((double)TF32(t, 0x40) + (double)TF32(t, 0x44));
        TF32(t, 0x44) = (float)((double)TF32(t, 0x44) + (double)0.1f);
        int32_t y = ftol((double)TF32(t, 0x40));
        TI32(t, 0x24) = y;
        if (y > 0x127) {                        // landed
            TI32(t, 0x50) = 0;
            TI32(t, 0x24) = 0x130;
            t->fn = f_401fb0;
            f_408230(TI32(t, 0x20) + 0x19, TI32(t, 0x24) + 0x3c);
            snd_play(10);
        } else if (in_pressed(5)) {             // air slash
            TI32(t, 0x50) = 4;
            TI32(t, 0x4c) = 2;
            f_402fd0(0x41f538);
            t->fn = f_402a80;
        } else if (in_held(8) && MI32(GS + 0x1c) >= 0x12c) {   // air technique
            MI32(GS + 0x1c) = 0;
            TI32(t, 0x50) = 6;
            TI32(t, 0x4c) = 4;
            TI32(t, 0x30) = 0x3c;
            f_402fd0(0x41f608);
            t->fn = f_402c30;
            snd_play(11);
        } else if (in_held(7) && (double)TF32(t, 0x38) > (double)10.0f) {  // air guard
            TI32(t, 0x50) = 0xa;
            t->fn = f_402670;
        }
    }
    draw_player(t, 0x180, 0x80);
}

// 0x402a80: player air slash (state 4), animation 2
void f_402a80(Task* t)
{
    uint32_t anim = 0x41f468 + TI32(t, 0x4c) * 0x68;
    if (t->wait == 0) {
        def_regen(t, 0.25f);
        TF32(t, 0x40) = (float)((double)TF32(t, 0x44) + (double)TF32(t, 0x40));
        TF32(t, 0x44) = (float)((double)TF32(t, 0x44) + (double)0.1f);
        if ((double)TF32(t, 0x40) > (double)304.0f)
            TF32(t, 0x40) = 304.0f;
        TI32(t, 0x24) = ftol((double)TF32(t, 0x40));
        f_402f90(anim);
        if (MU8(anim + 0x5d)) {
            TI32(t, 0x50) = 3;
            t->fn = f_4028f0;
            MI32(anim + 0x50) = MI32(anim + 0x50) - 1;
        }
    }
    draw_player_anim(t, anim);
}

// 0x402b60: player ground special technique (state 5), animation 3
void f_402b60(Task* t)
{
    uint32_t anim = 0x41f468 + TI32(t, 0x4c) * 0x68;
    if (t->wait == 0) {
        def_regen(t, 0.25f);
        f_402f90(anim);
        if (MU8(anim + 0x5d)) {
            TI32(t, 0x50) = 0;
            t->fn = f_401fb0;
            MI32(anim + 0x50) = MI32(anim + 0x50) - 1;
            if (MU8(GS + 0x29))
                f_40b0c0(TI32(t, 0x20) + 0xa, TI32(t, 0x24) - 0x32);
        }
    }
    draw_player_anim(t, anim);
}

// 0x402c30: player air special technique (state 6), animation 4
void f_402c30(Task* t)
{
    uint32_t anim = 0x41f468 + TI32(t, 0x4c) * 0x68;
    if (t->wait == 0) {
        def_regen(t, 0.25f);
        f_402f90(anim);
        TF32(t, 0x40) = (float)((double)TF32(t, 0x44) + (double)TF32(t, 0x40));
        TF32(t, 0x44) = (float)((double)TF32(t, 0x44) + (double)0.1f);
        if ((double)TF32(t, 0x40) > (double)304.0f)
            TF32(t, 0x40) = 304.0f;
        TI32(t, 0x24) = ftol((double)TF32(t, 0x40));
        if (MU8(anim + 0x5d)) {
            TI32(t, 0x50) = 3;
            t->fn = f_4028f0;
            MI32(anim + 0x50) = MI32(anim + 0x50) - 1;
            if (MU8(GS + 0x29))
                f_40b0c0(TI32(t, 0x20) + 0xa, TI32(t, 0x24) - 0x32);
        }
    }
    draw_player_anim(t, anim);
}

// 0x402d40: player stunned on the ground (30 frames)
void f_402d40(Task* t)
{
    TI32(t, 0x24) = 0x130;
    TF32(t, 0x40) = 304.0f;
    TI32(t, 0x20) = lane_x(TI32(t, 0x28));
    if (t->wait == 0) {
        def_regen(t, 2.5f);
        int32_t c = TI32(t, 0x54) + 1;
        TI32(t, 0x54) = c;
        if (c > 0x1e) {
            TI32(t, 0x54) = 0;
            TI32(t, 0x30) = 0;
            TI32(t, 0x50) = 0;
            t->fn = f_401fb0;
        }
    }
    draw_player(t, 0x80, 0x200);
}

// 0x402e00: player stunned in the air (30 frames)
void f_402e00(Task* t)
{
    if (t->wait == 0) {
        def_regen(t, 2.5f);
        int32_t c = TI32(t, 0x54) + 1;
        TI32(t, 0x54) = c;
        if (c > 0x1e) {
            TI32(t, 0x54) = 0;
            TI32(t, 0x30) = 0;
            TI32(t, 0x50) = 3;
            t->fn = f_4028f0;
        }
        TF32(t, 0x40) = (float)((double)TF32(t, 0x40) + (double)TF32(t, 0x44));
        TF32(t, 0x44) = (float)((double)TF32(t, 0x44) + (double)0.1f);
        if ((double)TF32(t, 0x40) > (double)304.0f)
            TF32(t, 0x40) = 304.0f;
        TI32(t, 0x24) = ftol((double)TF32(t, 0x40));
    }
    draw_player(t, 0x80, 0x200);
}

// 0x402ee0: player falling to the ground without control (stage end)
void f_402ee0(Task* t)
{
    if (!((double)TF32(t, 0x44) >= (double)0.0f))
        TF32(t, 0x44) = 0.0f;
    double v = (double)TF32(t, 0x44) + (double)0.1f;   // st0, kept unrounded
    TF32(t, 0x44) = (float)v;
    double w = v + (double)TF32(t, 0x40);
    TF32(t, 0x40) = (float)w;
    if (w > (double)304.0f) {
        TF32(t, 0x40) = 304.0f;
        TI32(t, 0x24) = 0x130;
    } else {
        TI32(t, 0x24) = ftol(w);
    }
    int32_t x = TI32(t, 0x20) - MI32(GS + 0x0);
    int32_t py = TI32(t, 0x24);
    int32_t y = py - MI32(GS + 0x8);
    if (py == 0x130)
        gfx_blit(0, x, y, 0x80, 0x80, 0, 0);
    else
        gfx_blit(0, x, y, 0x80, 0x80, 0x180, 0x80);
}

// 0x402f90: advance an animation record by one tick
void f_402f90(uint32_t anim)
{
    if (MU8(anim + 0x5d))
        return;
    int32_t tick = MI32(anim + 0x54) + 1;
    MI32(anim + 0x54) = tick;
    if (tick % MI32(anim + 0x58) != 0)
        return;
    int32_t f = MI32(anim + 0x50) + 1;
    MI32(anim + 0x50) = f;
    if (f <= MI32(anim + 0x64))
        return;
    if (MU8(anim + 0x5c)) {
        MI32(anim + 0x50) = MI32(anim + 0x60);
        return;
    }
    MU8(anim + 0x5d) = 1;
}

// 0x402fd0: rewind an animation record
void f_402fd0(uint32_t anim)
{
    MI32(anim + 0x54) = 0;
    MI32(anim + 0x50) = 0;
    MU8(anim + 0x5d) = 0;
}

// 0x402fe0: create the stage banner task (surface 56 = data/019.dat)
void f_402fe0()
{
    Task d; task_desc(&d);
    d.prio = 0x20;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_403040;
    TI32(&d, 0x20) = 0xbe;      // y
    TI32(&d, 0x24) = 0x280;     // x
    TI32(&d, 0x28) = 0x2800;    // x << 4
    TI32(&d, 0x2c) = 0;         // hold counter
    task_create(&d);
}

// 0x403040: banner slides in (eases to x = 100) and holds for 180 frames
void f_403040(Task* t)
{
    int32_t a = (30 * (TI32(t, 0x28) - 0x640)) / 31 + 0x640;
    TI32(t, 0x28) = a;
    int32_t x = a >> 4;
    TI32(t, 0x24) = x;
    if (x == 100) {
        int32_t c = TI32(t, 0x2c) + 1;
        TI32(t, 0x2c) = c;
        if (c > 0xb4)
            t->fn = f_4030c0;
    }
    gfx_blit(0x38, TI32(t, 0x24), TI32(t, 0x20), 0x1b8, 0x64, 0, 0);
}

// 0x4030c0: banner slides out to the left
void f_4030c0(Task* t)
{
    int32_t a = TI32(t, 0x28) - 0xa0;
    TI32(t, 0x28) = a;
    int32_t x = a >> 4;
    TI32(t, 0x24) = x;
    if (!(x > -0x1b8))
        t->dead = 1;
    gfx_blit(0x38, TI32(t, 0x24), TI32(t, 0x20), 0x1b8, 0x64, 0, 0);
}

// 0x403110: spawn a flying "斬 + combo count" popup at (x,y)
void f_403110(int32_t x, int32_t y)
{
    int32_t combo = MI32(GS + 0x14);
    int32_t r1 = crt_rand();
    int32_t vx = r1 % 256 - 0x80;
    int32_t r2 = crt_rand();
    int32_t vy = (-r2) % 64;
    Task d; task_desc(&d);
    d.prio = 0x20;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_4031c0;
    TI32(&d, 0x20) = combo;
    TI32(&d, 0x24) = x;
    TI32(&d, 0x28) = y;
    TI32(&d, 0x2c) = vx;
    TI32(&d, 0x30) = vy;
    TI32(&d, 0x34) = x * 16;   // shl 4
    TI32(&d, 0x38) = y * 16;   // shl 4
    task_create(&d);
}

// 0x4031c0: combo popup: moves with gravity, draws 斬 (zan.bmp) and up to
// three digits (combo.bmp)
void f_4031c0(Task* t)
{
    int32_t value = TI32(t, 0x20);
    int32_t xx = TI32(t, 0x34) + TI32(t, 0x2c);
    TI32(t, 0x34) = xx;
    int32_t vy = TI32(t, 0x30);
    int32_t yy = TI32(t, 0x38) + vy;
    TI32(t, 0x38) = yy;
    TI32(t, 0x30) = vy + 4;
    TI32(t, 0x24) = xx >> 4;
    TI32(t, 0x28) = yy >> 4;
    if (TI32(t, 0x28) > 0x1e0) {
        t->dead = 1;
        return;
    }
    gfx_blit(0x16, TI32(t, 0x24) + 0xa0, TI32(t, 0x28) - 8, 0x50, 0x50, 0, 0);
    int32_t div = 1;
    for (int32_t ox = 0x6c; ox > 0; ox -= 0x24) {
        int32_t q = value / div;
        if (q > 0)
            gfx_blit(0x15, TI32(t, 0x24) + ox, TI32(t, 0x28), 0x40, 0x40, (q % 10) << 6, 0);
        div = div * 10;
    }
}

// 0x4032a0: title screen left arrow at (x,y) (slides in from x+50)
void f_4032a0(int32_t x, int32_t y)
{
    Task d; task_desc(&d);
    d.prio = 0x10;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_403320;
    TI32(&d, 0x20) = x + 0x32;  // current x
    TI32(&d, 0x24) = x;         // home x
    TI32(&d, 0x28) = y;
    TI32(&d, 0x2c) = y;
    TI32(&d, 0x30) = 3;         // state: 3 slide in, 1/2 bounce, 0 idle
    TU8(&d, 0x34) = 0;          // "decided" flag
    task_create(&d);
}

// 0x403320: left arrow (data/018.dat lower half)
void f_403320(Task* t)
{
    if (TI32(t, 0x30) == 3) {
        int32_t x = TI32(t, 0x20) - 8;
        TI32(t, 0x20) = x;
        if (x < TI32(t, 0x24)) {
            TI32(t, 0x20) = TI32(t, 0x24);
            TI32(t, 0x30) = 0;
        }
    }
    if (in_pressed(2)) {
        TI32(t, 0x30) = 1;
        TI32(t, 0x20) = TI32(t, 0x24);
    }
    if (in_pressed(9))
        TU8(t, 0x34) = 1;
    if (TU8(t, 0x34) && TI32(t, 0x30) == 0)
        t->dead = 1;
    int32_t st = TI32(t, 0x30);
    if (st == 1) {
        int32_t x = TI32(t, 0x20) - 2;
        TI32(t, 0x20) = x;
        if (x < TI32(t, 0x24) - 0xa)
            TI32(t, 0x30) = 2;
    } else if (st == 2) {
        int32_t x = TI32(t, 0x20) + 2;
        TI32(t, 0x20) = x;
        if (x > TI32(t, 0x24)) {
            TI32(t, 0x20) = TI32(t, 0x24);
            TI32(t, 0x30) = 0;
        }
    }
    gfx_blit(0x37, TI32(t, 0x20), TI32(t, 0x28), 0x20, 0x20, 0, 0x20);
}

// 0x4033f0: title screen right arrow at (x,y) (slides in from x-50)
void f_4033f0(int32_t x, int32_t y)
{
    Task d; task_desc(&d);
    d.prio = 0x10;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_403470;
    TI32(&d, 0x20) = x - 0x32;
    TI32(&d, 0x24) = x;
    TI32(&d, 0x28) = y;
    TI32(&d, 0x2c) = y;
    TI32(&d, 0x30) = 3;
    TU8(&d, 0x34) = 0;
    task_create(&d);
}

// 0x403470: right arrow (data/018.dat upper half)
void f_403470(Task* t)
{
    if (TI32(t, 0x30) == 3) {
        int32_t x = TI32(t, 0x20) + 8;
        TI32(t, 0x20) = x;
        if (x > TI32(t, 0x24)) {
            TI32(t, 0x20) = TI32(t, 0x24);
            TI32(t, 0x30) = 0;
        }
    }
    if (in_pressed(4)) {
        TI32(t, 0x30) = 1;
        TI32(t, 0x20) = TI32(t, 0x24);
    }
    if (in_pressed(9))
        TU8(t, 0x34) = 1;
    if (TU8(t, 0x34) && TI32(t, 0x30) == 0)
        t->dead = 1;
    int32_t st = TI32(t, 0x30);
    if (st == 1) {
        int32_t x = TI32(t, 0x20) + 2;
        TI32(t, 0x20) = x;
        if (x > TI32(t, 0x24) + 0xa)
            TI32(t, 0x30) = 2;
    } else if (st == 2) {
        int32_t x = TI32(t, 0x20) - 2;
        TI32(t, 0x20) = x;
        if (x < TI32(t, 0x24)) {
            TI32(t, 0x20) = TI32(t, 0x24);
            TI32(t, 0x30) = 0;
        }
    }
    gfx_blit(0x37, TI32(t, 0x20), TI32(t, 0x28), 0x20, 0x20, 0, 0);
}

// 0x403540: spawn the 6-block tower (surface 41 = data/004.dat, 480x120
// blocks, 5 hit points each). Returns the tower task.
Task* f_403540()
{
    Task d; task_desc(&d);
    d.prio = 0x10;
    d.f04 = 1;
    d.wait = 0x96;
    d.parent = nullptr;
    d.fn = f_403610;
    TI32(&d, 0x20) = -800;          // y
    TI32(&d, 0x24) = 0;             // vy
    TI32(&d, 0x28) = -800 * 64;     // y << 6 (0xffff3800)
    TI32(&d, 0x2c) = 0x16;          // gravity
    TI32(&d, 0x30) = 6;             // blocks left
    TU8(&d, 0x34) = 0;              // hit flag
    TI32(&d, 0x38) = 0;             // hit column
    TI32(&d, 0x3c) = 0;             // hit damage
    Task* tw = task_create(&d);

    int32_t S[5];
    for (int32_t i = 0, sy = 0; sy < 0x2d0; sy += 0x78, i++) {
        S[0] = i;       // index
        S[1] = sy;      // sprite y
        S[2] = 5;       // hit points per column
        S[3] = 5;
        S[4] = 5;
        f_403690(S, tw);
    }
    return tw;
}

// 0x403610: tower falling / landing (ground y 429, block height 120)
void f_403610(Task* t)
{
    int32_t n = TI32(t, 0x30);
    if (n == 0) {
        t->dead = 1;
        return;
    }
    if (t->wait > 0)
        return;
    int32_t vy = TI32(t, 0x24);
    int32_t yy = TI32(t, 0x28) + vy;
    TI32(t, 0x28) = yy;
    int32_t y = yy >> 6;
    TI32(t, 0x20) = y;
    if (y + n * 120 >= 0x1ad) {
        if (vy > TI32(t, 0x2c)) {
            f_40aae0();
            f_401bc0();
        }
        TI32(t, 0x24) = 0;
        int32_t ny = 0x1ad - TI32(t, 0x30) * 120;
        TI32(t, 0x20) = ny;
        TI32(t, 0x28) = ny * 64;   // shl 6
    }
    TI32(t, 0x24) = TI32(t, 0x24) + TI32(t, 0x2c);
}

// 0x403690: create one tower block task (prio 0x10); S = 5 dwords
Task* f_403690(const int32_t* S, Task* parent)
{
    Task d; task_desc(&d);
    d.prio = 0x10;
    d.f04 = 0;
    d.wait = 0;
    d.parent = parent;
    d.fn = f_4036f0;
    for (int i = 0; i < 5; i++)
        TI32(&d, 0x20 + 4 * i) = S[i];
    return task_create(&d);
}

// 0x4036f0: tower block: takes hits on the bottom block, scores, draws
void f_4036f0(Task* t)
{
    Task* P = t->parent;
    if (TU8(P, 0x34) == 1 && TI32(P, 0x30) - 1 == TI32(t, 0x20)) {
        int32_t col = TI32(P, 0x38);
        TI32(t, 0x28 + col * 4) = TI32(t, 0x28 + col * 4) - TI32(P, 0x3c);
        TU8(P, 0x34) = 0;
        if (TI32(t, 0x28 + TI32(P, 0x38) * 4) <= 0) {
            TI32(P, 0x30) = TI32(P, 0x30) - 1;
            t->dead = 1;
            int32_t c10 = MI32(GS + 0x14) * 10;
            double b = (double)TI32(P, 0x2c) * (double)1.5f;
            double a = (double)c10 * b;
            a = a * (double)100.0f;
            MI32(GS + 0x10) = ftol(a + (double)MI32(GS + 0x10));
            if (MI32(GS + 0x10) > MI32(GS + 0x3c))
                MI32(GS + 0x10) = MI32(GS + 0x3c);
            int32_t idx = TI32(t, 0x20);
            f_4040b0(0x140, TI32(P, 0x20) + idx * 120);
            snd_play(7);
            return;
        } else {
            double b = (double)TI32(P, 0x2c) * (double)1.5f;
            MI32(GS + 0x10) = ftol((double)MI32(GS + 0x14) * b + (double)MI32(GS + 0x10));
            if (MI32(GS + 0x10) > MI32(GS + 0x3c))
                MI32(GS + 0x10) = MI32(GS + 0x3c);
        }
    }
    int32_t py = TI32(P, 0x20);
    int32_t x = 0x50 - MI32(GS + 0x0);
    int32_t y = TI32(t, 0x20) * 120 - MI32(GS + 0x8) + py;
    gfx_blit(0x29, x, y, 0x1e0, 0x78, 0, TI32(t, 0x24));
}

// 0x403870: create the final "big slash" prompt task (prio 0x40,
// surface 40 = data/003.dat 600x120)
void f_403870()
{
    Task d; task_desc(&d);
    d.prio = 0x40;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_4038e0;
    TI32(&d, 0x20) = 0x280;     // x
    TI32(&d, 0x24) = 0xb4;      // y
    TI32(&d, 0x28) = 0x2800;    // x << 4
    TI32(&d, 0x2c) = 0;         // (uninitialised in the original)
    TTASK(&d, 0x30) = nullptr;  // spawned task (set later)
    TU8(&d, 0x34) = 0;          // started flag
    task_create(&d);
}

// 0x4038e0: prompt slides in to x = 20; on decide (button 9) spawns the
// task returned by f_40b350 and waits for it to die, then sets GS+0x32.
void f_4038e0(Task* t)
{
    task_pause_mask(0x7f);
    int32_t a = (15 * (TI32(t, 0x28) - 0x140)) / 16 + 0x140;
    TI32(t, 0x28) = a;
    int32_t x = a >> 4;
    TI32(t, 0x20) = x;
    if (x == 0x14 && in_pressed(9) && !TU8(t, 0x34)) {
        TU8(t, 0x34) = 1;
        TTASK(t, 0x30) = f_40b350();
    }
    if (TU8(t, 0x34) && (TTASK(t, 0x30)->dead & 0xff)) {
        t->dead = 1;
        MU8(GS + 0x32) = 1;
    }
    gfx_blit(0x28, TI32(t, 0x20), TI32(t, 0x24), 0x258, 0x78, 0, 0);
}

// 0x403980: create a full screen flash task (prio 0x20) with a COLORREF
void f_403980(uint32_t colorref)
{
    Task d; task_desc(&d);
    d.prio = 0x20;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_4039d0;
    TI32(&d, 0x20) = 0;         // frame counter
    TU8(&d, 0x24) = 1;          // visible
    TU32(&d, 0x28) = colorref;
    task_create(&d);
}

// 0x4039d0: flash: toggles every 3 frames, ends after 9 frames
void f_4039d0(Task* t)
{
    int32_t c = TI32(t, 0x20) + 1;
    TI32(t, 0x20) = c;
    if (c % 3 == 0)
        TU8(t, 0x24) = (TU8(t, 0x24) == 0);
    if (c > 9)
        t->dead = 1;
    if (TU8(t, 0x24))
        gfx_fill_rgb(0, 0, 0x280, 0x1e0, TU32(t, 0x28));
}
