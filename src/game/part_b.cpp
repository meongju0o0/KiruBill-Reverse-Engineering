// Translation of the original game code 0x403a30 .. 0x405f70.
//
// Contents: the 110 floor tower of the last stage, chimney smoke, the "終演"
// game over banner, building debris, and the collision handlers between the
// player and the falling buildings / rocks / lightning.
//
// Player task data used here (task created in part a, pointer in GS+0x44):
//   +0x20 x   +0x24 y   +0x28 attack column   +0x2c lives
//   +0x30 invincibility timer   +0x34 Task* (helper)   +0x38 float gauge
//   +0x40 float y   +0x44 float vy   +0x50 state   +0x54 flag
//   (A->wait > 0 is required for the crush check)
#include "game.h"

// (functions of other parts are declared in game.h)

namespace {

// The original writes `mov BYTE PTR [task+0x18], 1` (low byte of `dead`).
inline void set_dead(Task* t) { reinterpret_cast<uint8_t*>(&t->dead)[0] = 1; }
inline bool is_dead(Task* t) { return reinterpret_cast<uint8_t*>(&t->dead)[0] != 0; }

// x86 `shl` on a possibly negative value (well defined in two's complement).
inline int32_t shl(int32_t v, int n) { return (int32_t)((uint32_t)v << n); }

// Combo counter / gauge update used by every successful slash:
//   GS+0x14 combo (max 999), GS+0x1c gauge (max 300).
inline void combo_add()
{
    MI32(GS + 0x14) += 1;
    if (MI32(GS + 0x14) > 999)
        MI32(GS + 0x14) = 999;
    if ((MI32(GS + 0x14) + 1) / 2 > 30)
        MI32(GS + 0x1c) += 30;
    else
        MI32(GS + 0x1c) += (MI32(GS + 0x14) + 1) / 2;
    if (MI32(GS + 0x1c) > 300)
        MI32(GS + 0x1c) = 300;
}

// End of a successful slash: slash effect, hit spark, hit stop, slash sound.
inline void slash_tail(Task* A)
{
    f_408c10(TI32(A, 0x20), TI32(A, 0x24) - 30);
    f_403110(TI32(A, 0x20) - 70, TI32(A, 0x24) - MI32(GS + 8) - 30);
    task_set_wait_mask(0x7f, 3);
    snd_play(crt_rand() % 3);
}

// Max combo bookkeeping: GS+0x18 = max(GS+0x18, GS+0x14); GS+0x14 = 0.
inline void combo_break()
{
    if (MI32(GS + 0x14) > MI32(GS + 0x18))
        MI32(GS + 0x18) = MI32(GS + 0x14);
    MI32(GS + 0x14) = 0;
}

// Gauge float +30 (max 100) as `fld; fadd 30.0f; fst; fcomp 100.0f`.
inline void gauge_add30(Task* A, int off)
{
    double v = (double)TF32(A, off) + (double)30.0f;
    TF32(A, off) = (float)v;
    if (v > (double)100.0f)
        TU32(A, off) = 0x42c80000; // 100.0f
}

inline void bonus_add90()
{
    MI32(GS + 0x1c) += 90;
    if (MI32(GS + 0x1c) > 300)
        MI32(GS + 0x1c) = 300;
}

// Score for destroying / damaging a wall column: GS+0x10 += ..., capped by GS+0x3c.
inline void score_cap()
{
    if (MI32(GS + 0x10) > MI32(GS + 0x3c))
        MI32(GS + 0x10) = MI32(GS + 0x3c);
}

// GS+0x40 holds a Task* in the original (cleared with a 32-bit store).
inline void clear_gs40()
{
    MI32(GS + 0x40) = 0;
    MTASK(GS + 0x40) = nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
// 0x403a30: spawn the 110 floor tower (falls with f_401530) and its floors.
Task* f_403a30()
{
    Task d;
    task_desc(&d);
    int32_t y = MI32(GS + 8) - 12982;
    d.prio = 16;
    d.f04 = 2;
    d.wait = 150;
    d.parent = nullptr;
    d.fn = f_401530;
    TI32(&d, 0x20) = y;          // top y
    TI32(&d, 0x24) = 0;          // fall speed
    TI32(&d, 0x28) = y << 6;     // y (fixed 26.6)
    TI32(&d, 0x2c) = 5;          // acceleration / score factor
    TI32(&d, 0x30) = 110;        // floors left
    TU8(&d, 0x34) = 0;           // hit flag (only the low byte is written)
    TI32(&d, 0x38) = 0;          // hit column
    TI32(&d, 0x3c) = 0;          // hit damage
    Task* b = task_create(&d);

    // floor descriptor: index, image row (sy), hp[3], score factor
    int32_t f[6];
    for (int i = 0; i < 3; i++) f[2 + i] = 20;
    f[5] = 20;
    f[1] = 0;
    f[0] = 0;
    f_403b70(f, b);
    for (int k = 108; k != 0; k--) {
        for (int i = 0; i < 3; i++) f[2 + i] = 1;
        f[1] = 120;
        f[5] = 1;
        f[0] = f[0] + 1;
        f_403b70(f, b);
    }
    for (int i = 0; i < 3; i++) f[2 + i] = 10;
    f[5] = 10;
    f[1] = 240;
    f[0] = f[0] + 1;
    f_403b70(f, b);
    return b;
}

// 0x403b70
void f_403b70(const int32_t* floor, Task* building)
{
    Task d;
    task_desc(&d);
    d.prio = 16;
    d.f04 = 0;
    d.wait = 0;
    d.parent = building;
    d.fn = f_403bd0;
    for (int i = 0; i < 6; i++)
        TI32(&d, 0x20 + 4 * i) = floor[i];
    task_create(&d);
}

// 0x403bd0: one floor of the tower.  data: +0x20 floor index, +0x24 image
// row, +0x28..+0x30 hp of the 3 columns, +0x34 score factor.
void f_403bd0(Task* t)
{
    Task* P = t->parent;
    if (TU8(P, 0x34) == 1 && TI32(P, 0x30) - 1 == TI32(t, 0x20)) {
        int col = TI32(P, 0x38);
        TI32(t, 0x28 + col * 4) = TI32(t, 0x28 + col * 4) - TI32(P, 0x3c);
        col = TI32(P, 0x38);
        TU8(P, 0x34) = 0;
        if (TI32(t, 0x28 + col * 4) <= 0) {
            TI32(P, 0x30) = TI32(P, 0x30) - 1;
            set_dead(t);
            int32_t a = MI32(GS + 0x14) * 200;
            MI32(GS + 0x10) = ftol((double)a * ((double)TI32(P, 0x2c) * (double)1.5f)
                                   * (double)TI32(t, 0x34) + (double)MI32(GS + 0x10));
            score_cap();
            f_4040b0(320, TI32(P, 0x20) + TI32(t, 0x20) * 118);
            snd_play(7);
            return;
        }
        MI32(GS + 0x10) = ftol((double)MI32(GS + 0x14) * ((double)TI32(P, 0x2c) * (double)1.5f)
                               + (double)MI32(GS + 0x10));
        score_cap();
    }
    int32_t py = TI32(P, 0x20);
    int32_t x = 80 - MI32(GS + 0);
    int32_t y = TI32(t, 0x20) * 118 - MI32(GS + 8) + py;
    gfx_blit(42, x, y, 480, 120, 0, TI32(t, 0x24));
}

// 0x403d60: chimney smoke emitter.
void f_403d60()
{
    Task d;
    task_desc(&d);
    d.prio = 2;
    d.f04 = 4;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_403db0;
    TI32(&d, 0x20) = 1800;  // frames left
    task_create(&d);
}

// 0x403db0
void f_403db0(Task* t)
{
    if (TI32(t, 0x20) > 0) {
        TI32(t, 0x20) -= 1;
        if (crt_rand() % 8 == 0)
            f_403e20();
    } else {
        set_dead(t);
    }
    int32_t y = 184 - MI32(GS + 8);
    int32_t x = 366 - MI32(GS + 0);
    gfx_blit(50, x, y, 70, 22, 0, 0);
}

// 0x403e20: one smoke puff.
void f_403e20()
{
    int32_t f[6];
    f[0] = 185;            // y
    f[1] = 2960;           // y << 4
    f[2] = crt_rand() % 40 + 370;  // x
    f[3] = f[2] << 4;      // x << 4
    f[4] = -40 - crt_rand() % 60;  // vy
    f[5] = crt_rand() % 40 - 20;   // vx
    Task d;
    task_desc(&d);
    d.prio = 4;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_403ed0;
    for (int i = 0; i < 6; i++)
        TI32(&d, 0x20 + 4 * i) = f[i];
    task_create(&d);
}

// 0x403ed0
void f_403ed0(Task* t)
{
    int32_t fx = TI32(t, 0x2c) + TI32(t, 0x34);
    int32_t fy = TI32(t, 0x24) + TI32(t, 0x30);
    TI32(t, 0x2c) = fx;
    TI32(t, 0x24) = fy;
    TI32(t, 0x28) = fx >> 4;
    TI32(t, 0x20) = fy >> 4;
    if (TI32(t, 0x20) < MI32(GS + 8) - 60)
        set_dead(t);
    int32_t x = TI32(t, 0x28) - MI32(GS + 0);
    int32_t y = TI32(t, 0x20) - MI32(GS + 8);
    gfx_blit(0x33, x, y, 16, 12, 0, 0);
}

// 0x403f50: the "終演" banner (surface 23) dropping onto the screen.
void f_403f50()
{
    Task d;
    task_desc(&d);
    d.prio = 0x20;
    d.f04 = 0;
    d.wait = 2;
    d.parent = nullptr;
    d.fn = f_403fc0;
    TU32(&d, 0x20) = 0xc2c80000;  // float y = -100.0
    TU32(&d, 0x24) = 0;           // float vy = 0.0
    TU8(&d, 0x28) = 0;            // bounced
    TU8(&d, 0x29) = 0;            // settled
    TU8(&d, 0x30) = 0;            // end task spawned
    task_create(&d);
}

// 0x403fc0
void f_403fc0(Task* t)
{
    task_pause_mask(127);
    TF32(t, 0x20) = (float)((double)TF32(t, 0x24) + (double)TF32(t, 0x20));
    double v = (double)TF32(t, 0x24) + (double)0.4f;
    if (TU8(t, 0x29) == 0) {
        TF32(t, 0x24) = (float)v;
        if ((double)TF32(t, 0x20) > (double)190.0f) {
            if (v < (double)3.0f) {
                TU8(t, 0x29) = 1;
                TU32(t, 0x24) = 0x41400000;  // 12.0f
            }
            double n = -(double)TF32(t, 0x24) * 0.5;
            TU32(t, 0x20) = 0x433e0000;      // 190.0f
            TU8(t, 0x28) = 1;
            TF32(t, 0x24) = (float)n;
        }
    } else {
        TF32(t, 0x24) = (float)v;
        MI32(0x41f670) += 1;
    }
    if (MI32(0x41f670) == 60 && TU8(t, 0x30) == 0) {
        TTASK(t, 0x2c) = f_40b350();
        MI32(0x41f670) = 0;
        TU8(t, 0x30) = 1;
    }
    if (TU8(t, 0x30) != 0) {
        if (is_dead(TTASK(t, 0x2c)))
            MU8(GS + 0x28) = 1;
    }
    gfx_blit(0x17, 210, ftol((double)TF32(t, 0x20)), 220, 100, 0, 0);
}

// 0x4040b0: debris burst at (x, y).
void f_4040b0(int32_t x, int32_t y)
{
    Task d;
    task_desc(&d);
    d.prio = 16;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_404100;
    TI32(&d, 0x20) = x;
    TI32(&d, 0x24) = y;
    TI32(&d, 0x28) = 0;   // frame counter
    task_create(&d);
}

// 0x404100
void f_404100(Task* t)
{
    TI32(t, 0x28) += 1;
    if (TI32(t, 0x28) > 3) {
        set_dead(t);
        return;
    }
    for (int32_t off = -200; off < 200; off += 50) {
        int32_t type = crt_rand() % 3;
        int32_t y = crt_rand() % 120 + TI32(t, 0x24);
        int32_t x = TI32(t, 0x20) - crt_rand() % 20 + off;
        f_404170(x, y, type);
    }
}

// 0x404170: one debris piece. data: +0x20 x, +0x24 y, +0x28 vx, +0x2c vy,
// +0x30 x<<4, +0x34 y<<4, +0x38 size, +0x3c type.
void f_404170(int32_t x, int32_t y, int32_t type)
{
    int32_t f[8];
    f[0] = x;
    f[1] = y;
    f[4] = x << 4;
    f[5] = y << 4;
    f[7] = type;
    int32_t m = type + 3;
    f[2] = ((crt_rand() % 128) - 64) * m / 3;
    f[3] = (-32 - crt_rand() % 64) * m / 3;
    switch (type) {
    case 0: f[6] = 50; break;
    case 1: f[6] = 30; break;
    case 2: f[6] = 16; break;
    default: f[6] = 0; break; // uninitialised in the original (type is always 0..2)
    }
    Task d;
    task_desc(&d);
    d.prio = 16;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_404270;
    for (int i = 0; i < 8; i++)
        TI32(&d, 0x20 + 4 * i) = f[i];
    task_create(&d);
}

// 0x404270
void f_404270(Task* t)
{
    int32_t vy = TI32(t, 0x2c);
    int32_t fy = TI32(t, 0x34) + vy;
    int32_t fx = TI32(t, 0x30) + TI32(t, 0x28);
    TI32(t, 0x34) = fy;
    TI32(t, 0x30) = fx;
    TI32(t, 0x20) = fx >> 4;
    TI32(t, 0x24) = fy >> 4;
    TI32(t, 0x2c) = vy + 4;
    int32_t sx = TI32(t, 0x20) - MI32(GS + 0);
    int32_t sy = TI32(t, 0x24) - MI32(GS + 8);
    int32_t sz = TI32(t, 0x38);
    gfx_blit(TI32(t, 0x3c) + 13, sx, sy, sz, sz, 0, 0);
    if (TI32(t, 0x24) > 0x260 || TI32(t, 0x20) < -64 || TI32(t, 0x20) > 0x2c0)
        set_dead(t);
}

// 0x404300
void f_404300(Task* player, Task* building)
{
    Task d;
    task_desc(&d);
    d.prio = 0x20;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_404350;
    TTASK(&d, 0x20) = player;
    TTASK(&d, 0x24) = building;
    task_create(&d);
}

// 0x404350: player vs. the f_403a30 tower (floors of 120 px).
// B (+0x20 top y, +0x24 speed, +0x28 y<<6, +0x2c factor, +0x30 floors,
//    +0x34 hit flag, +0x38 hit column, +0x3c hit damage).
void f_404350(Task* t)
{
    Task* A = TTASK(t, 0x20);
    Task* B = TTASK(t, 0x24);
    if (is_dead(B)) {
        MU8(GS + 0x29) = 0;
        clear_gs40();
        set_dead(t);
        return;
    }
    int32_t bottom = TI32(B, 0x20) + TI32(B, 0x30) * 120;
    if (bottom == 0x1ad) {
        // the tower reached the ground: crush the player
        if (TI32(A, 0x30) == 0 && A->wait > 0 && TI32(A, 0x2c) > 0) {
            task_resume_mask(8);
            TI32(A, 0x2c) -= 1;
            TI32(A, 0x30) = 180;
            TI32(A, 0x50) = 12;
            A->fn = f_4021d0;
            combo_break();
            gauge_add30(A, 0x38);
            bonus_add90();
            snd_play(6);
            if (TI32(A, 0x2c) <= 0)
                f_403f50();
        }
    }
    switch ((uint32_t)TI32(A, 0x50)) {
    case 0:
    case 2:
        if (bottom > TI32(A, 0x24) + 90 && TI32(B, 0x24) > 0 && TI32(A, 0x30) == 0)
            task_pause_mask(8);
        return;
    case 3:
    case 6:
        if (bottom > TI32(A, 0x24) + 48) {
            TF32(A, 0x44) = (float)((double)TI32(B, 0x24) * (double)0.0078125f);
            TF32(A, 0x40) = (float)(bottom - 48);
        }
        return;
    case 11:
        if (bottom > TI32(A, 0x24) + 80 && TI32(B, 0x24) > 0)
            task_pause_mask(8);
        return;
    case 1:
        if (bottom > TI32(A, 0x24) + 10) {
            TI32(A, 0x50) = 0;
            TU8(B, 0x34) = 1;
            TI32(B, 0x3c) = 1;
            if (TI32(A, 0x30) > 0)
                TI32(B, 0x3c) = 5;
            TI32(A, 0x30) = 0;
            TI32(B, 0x38) = TI32(A, 0x28);
            TI32(B, 0x24) += -10 * TI32(B, 0x2c);
            combo_add();
            slash_tail(A);
            return;
        }
        if (bottom > TI32(A, 0x24) + 90 && TI32(B, 0x24) > 0)
            task_pause_mask(8);
        return;
    case 4:
        if ((double)TF32(A, 0x44) * (double)6.0f + (double)TI32(A, 0x24) < (double)bottom
            || bottom > TI32(A, 0x24) + 10) {
            TI32(A, 0x50) = 3;
            TU8(B, 0x34) = 1;
            TI32(B, 0x3c) = 1;
            if (TI32(A, 0x30) > 0)
                TI32(B, 0x3c) = 5;
            TI32(A, 0x30) = 0;
            TI32(B, 0x38) = TI32(A, 0x28);
            TI32(B, 0x24) += -10 * TI32(B, 0x2c);
            combo_add();
            slash_tail(A);
        }
        return;
    case 7:
        if (bottom > TI32(A, 0x24) + 50) {
            TI32(A, 0x50) = 8;
            TI32(A, 0x30) = 0;
            A->fn = f_4024f0;
            TI32(B, 0x24) = shl(-TI32(B, 0x2c), 5);
            combo_break();
            snd_play(5);
        }
        return;
    case 8:
        if (bottom > TI32(A, 0x24) + 60)
            TI32(B, 0x28) += (-512) / TI32(B, 0x2c);
        return;
    case 10:
        if (bottom > TI32(A, 0x24) + 50) {
            TI32(A, 0x50) = 9;
            TI32(A, 0x30) = 0;
            TU32(A, 0x44) = 0x41000000;  // 8.0f
            TF32(A, 0x40) = (float)(bottom - 50);
            A->fn = f_402590;
            TI32(B, 0x24) = shl(-TI32(B, 0x2c), 4);
            snd_play(5);
        }
        return;
    default:  // 5, 9, > 11
        return;
    }
}

// 0x404870
void f_404870(Task* player, Task* building)
{
    if (MU8(GS + 0x29) == 0)
        return;
    Task d;
    task_desc(&d);
    d.prio = 0x20;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_4048d0;
    TTASK(&d, 0x20) = player;
    TTASK(&d, 0x24) = building;
    task_create(&d);
}

// 0x4048d0: while the special mode (GS+0x29) runs, every floor that passes
// the player is hit automatically (tower with 120 px floors).
void f_4048d0(Task* t)
{
    Task* A = TTASK(t, 0x20);
    Task* B = TTASK(t, 0x24);
    if (is_dead(A)) {
        set_dead(t);
        return;
    }
    if (is_dead(B)) {
        MU8(GS + 0x29) = 0;
        clear_gs40();
        set_dead(t);
        return;
    }
    int32_t bottom = TI32(B, 0x20) + TI32(B, 0x30) * 120;
    if (TI32(A, 0x24) + 150 < bottom && TU8(B, 0x34) == 0) {
        TI32(B, 0x3c) = 5;
        TU8(B, 0x34) = 1;
        TI32(B, 0x38) = 1;
        MI32(GS + 0x14) += 1;
        snd_play(crt_rand() % 3);
        if (MI32(GS + 0x14) > 999)
            MI32(GS + 0x14) = 999;
        f_403110(0xba, bottom - MI32(GS + 8) - 30);
    }
}

// 0x4049a0
void f_4049a0(Task* player, Task* building)
{
    Task d;
    task_desc(&d);
    d.prio = 0x20;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_404a00;
    TTASK(&d, 0x20) = player;
    TI32(&d, 0x24) = 0;     // uninitialised stack garbage in the original (unused)
    TTASK(&d, 0x28) = building;
    TI32(&d, 0x2c) = 0;     // uninitialised stack garbage in the original (unused)
    task_create(&d);
}

// 0x404a00: player vs. an f_401530 building (floors of 118 px, +2).
void f_404a00(Task* t)
{
    Task* A = TTASK(t, 0x20);
    Task* B = TTASK(t, 0x28);
    if (is_dead(B)) {
        MU8(GS + 0x29) = 0;
        clear_gs40();
        set_dead(t);
        return;
    }
    int32_t bottom = TI32(B, 0x20) + TI32(B, 0x30) * 118 + 2;
    if (bottom == 0x1ac) {
        if (TI32(A, 0x30) == 0 && A->wait > 0 && TI32(A, 0x2c) > 0) {
            task_resume_mask(8);
            TI32(A, 0x2c) -= 1;
            TI32(A, 0x30) = 180;
            TI32(A, 0x50) = 12;
            A->fn = f_4021d0;
            combo_break();
            gauge_add30(A, 0x38);
            bonus_add90();
            snd_play(6);
            if (TI32(A, 0x2c) <= 0)
                f_403f50();
        }
    }
    switch ((uint32_t)TI32(A, 0x50)) {
    case 0:
    case 2:
        if (bottom > TI32(A, 0x24) + 90 && TI32(B, 0x24) > 0 && TI32(A, 0x30) == 0)
            task_pause_mask(8);
        return;
    case 3:
    case 6:
        if (bottom > TI32(A, 0x24) + 48) {
            TF32(A, 0x44) = (float)((double)TI32(B, 0x24) * (double)0.0078125f);
            TF32(A, 0x40) = (float)(bottom - 48);
        }
        return;
    case 11:
        if (bottom > TI32(A, 0x24) + 80 && TI32(B, 0x24) > 0)
            task_pause_mask(8);
        return;
    case 1:
        if (bottom > TI32(A, 0x24) + 10) {
            TI32(A, 0x50) = 0;
            TU8(B, 0x34) = 1;
            TI32(B, 0x3c) = 1;
            if (TI32(A, 0x30) > 0)
                TI32(B, 0x3c) = 5;
            TI32(A, 0x30) = 0;
            TI32(B, 0x38) = TI32(A, 0x28);
            TI32(B, 0x24) += -10 * TI32(B, 0x2c);
            combo_add();
            slash_tail(A);
            return;
        }
        if (bottom > TI32(A, 0x24) + 90 && TI32(B, 0x24) > 0)
            task_pause_mask(8);
        return;
    case 4:
        if ((double)TF32(A, 0x44) * (double)6.0f + (double)TI32(A, 0x24) < (double)bottom
            || bottom > TI32(A, 0x24) + 10) {
            TI32(A, 0x50) = 3;
            TU8(B, 0x34) = 1;
            TI32(B, 0x3c) = 1;
            if (TI32(A, 0x30) > 0)
                TI32(B, 0x3c) = 5;
            TI32(A, 0x30) = 0;
            TI32(B, 0x38) = TI32(A, 0x28);
            TI32(B, 0x24) += -10 * TI32(B, 0x2c);
            combo_add();
            slash_tail(A);
        }
        return;
    case 7:
        if (bottom > TI32(A, 0x24) + 50) {
            TI32(A, 0x50) = 8;
            TI32(A, 0x30) = 0;
            A->fn = f_4024f0;
            TI32(B, 0x24) = shl(-TI32(B, 0x2c), 5);
            combo_break();
            snd_play(5);
        }
        return;
    case 8:
        if (bottom > TI32(A, 0x24) + 60)
            TI32(B, 0x28) += (-512) / TI32(B, 0x2c);
        return;
    case 10:
        if (bottom > TI32(A, 0x24) + 50) {
            TI32(A, 0x50) = 9;
            TI32(A, 0x30) = 0;
            TU32(A, 0x44) = 0x41000000;  // 8.0f
            TF32(A, 0x40) = (float)(bottom - 50);
            A->fn = f_402590;
            TI32(B, 0x24) = shl(-TI32(B, 0x2c), 4);
            snd_play(5);
        }
        return;
    default:  // 5, 9, > 11
        return;
    }
}

// 0x404f30
void f_404f30(Task* player, Task* b2, Task* building)
{
    if (MU8(GS + 0x29) == 0)
        return;
    Task d;
    task_desc(&d);
    d.prio = 0x20;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_404fb0;
    TTASK(&d, 0x20) = player;
    TTASK(&d, 0x24) = b2;
    TTASK(&d, 0x28) = building;
    TI32(&d, 0x2c) = 5;     // hits left
    task_create(&d);
}

// 0x404fb0: special mode auto hits (5 hits) against an f_401530 building.
void f_404fb0(Task* t)
{
    Task* A = TTASK(t, 0x20);
    Task* C = TTASK(t, 0x28);
    if (TI32(t, 0x2c) <= 0) {
        set_dead(t);
        set_dead(TTASK(t, 0x20));
        set_dead(TTASK(t, 0x24));
        f_403980(0xffffff);
        Task* h = TTASK(A, 0x34);
        if (h != nullptr) {
            TU8(h, 0x30) = 0;
        } else {
            task_resume_mask(0x7f);
        }
        TI32(MTASK(GS + 0x44), 0x30) = 0;
        return;
    }
    if (is_dead(A)) {
        set_dead(t);
        return;
    }
    if (is_dead(C)) {
        MU8(GS + 0x29) = 0;
        clear_gs40();
        set_dead(t);
        return;
    }
    int32_t bottom = TI32(C, 0x20) + TI32(C, 0x30) * 118 + 2;
    if (TI32(A, 0x24) + 150 < bottom && TU8(C, 0x34) == 0) {
        TU8(C, 0x34) = 1;
        TI32(C, 0x3c) = 1;
        TI32(C, 0x38) = 1;
        TI32(t, 0x2c) -= 1;
        MI32(GS + 0x14) += 1;
        snd_play(crt_rand() % 3);
        if (MI32(GS + 0x14) > 999)
            MI32(GS + 0x14) = 999;
        f_403110(0xba, bottom - MI32(GS + 8) - 30);
    }
}

// 0x4050f0
void f_4050f0(Task* player, Task* obj)
{
    Task d;
    task_desc(&d);
    d.prio = 16;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_405140;
    TTASK(&d, 0x20) = player;
    TTASK(&d, 0x24) = obj;
    task_create(&d);
}

// 0x405140: object B (child of carrier P = B->parent) vs. the player's slashes.
void f_405140(Task* t)
{
    Task* B = TTASK(t, 0x24);
    Task* A = TTASK(t, 0x20);
    Task* P = B->parent;
    if (is_dead(B)) {
        set_dead(t);
        return;
    }
    int32_t ey = TI32(B, 0x24) + TI32(P, 0x20) + 0x1b8;
    int32_t bx = TI32(B, 0x20);
    if (bx >= TI32(A, 0x20) + 0x58)
        return;
    if (bx + 0x50 <= TI32(A, 0x20) + 0x28)
        return;
    switch (TI32(A, 0x50)) {
    case 3:
    case 14:
        if (ey > TI32(A, 0x24) + 48) {
            TF32(A, 0x44) = (float)((double)TI32(P, 0x24) * (double)0.0078125f + (double)1.0f);
            TF32(A, 0x40) = (float)(ey - 47);
        }
        return;
    case 1:
        if (ey > TI32(A, 0x24) + 10) {
            TI32(A, 0x50) = 0;
            TU8(B, 0x30) = 1;
            TI32(B, 0x34) = 1;
            combo_add();
            slash_tail(A);
        }
        return;
    case 4:
        if ((double)TF32(A, 0x44) * (double)6.0f + (double)TI32(A, 0x24) < (double)ey
            || ey > TI32(A, 0x24) + 10) {
            TI32(A, 0x50) = 3;
            TU8(B, 0x30) = 1;
            TI32(B, 0x34) = 1;
            combo_add();
            slash_tail(A);
        }
        return;
    case 6:
        if (ey > TI32(A, 0x24) + 48) {
            TF32(A, 0x44) = (float)((double)TI32(P, 0x24) * (double)0.0078125f);
            TF32(A, 0x40) = (float)(ey - 48);
        }
        return;
    default:  // 2, 5, 7..13, others
        return;
    }
}

// 0x405440
void f_405440(Task* player, Task* rock)
{
    Task d;
    task_desc(&d);
    d.prio = 0x20;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_405490;
    TTASK(&d, 0x20) = player;
    TTASK(&d, 0x24) = rock;
    task_create(&d);
}

// 0x405490: rock vs. player.  Rock B: +0x20 y, +0x24 x speed, +0x28 x,
// +0x2c fall speed, +0x30 weight, +0x34 landed flag, +0x38 hp,
// +0x3c hit flag, +0x40 column.
void f_405490(Task* t)
{
    Task* A = TTASK(t, 0x20);
    Task* B = TTASK(t, 0x24);
    if (is_dead(B)) {
        set_dead(t);
        return;
    }
    int32_t bottom = TI32(B, 0x20) + 0x78;
    if (TI32(B, 0x28) + 0x28 >= TI32(A, 0x20) + 0x58)
        return;
    if (TI32(B, 0x28) + 0x50 <= TI32(A, 0x20) + 0x28)
        return;
    if (bottom == 0x1ad) {
        if (TI32(A, 0x30) == 0 && A->wait > 0 && TI32(A, 0x2c) > 0) {
            task_resume_mask(8);
            TI32(A, 0x2c) -= 1;
            TI32(A, 0x30) = 180;
            TI32(A, 0x50) = 12;
            TI32(A, 0x28) = TI32(B, 0x40);
            A->fn = f_4021d0;
            combo_break();
            gauge_add30(A, 0x38);
            bonus_add90();
            snd_play(6);
            TU8(B, 0x34) = 1;
            if (TI32(A, 0x2c) <= 0)
                f_403f50();
        }
    }
    switch ((uint32_t)TI32(A, 0x50)) {
    case 0:
    case 2:
        if (bottom > TI32(A, 0x24) + 90 && TI32(B, 0x2c) > 0 && TI32(A, 0x30) == 0)
            task_pause_mask(8);
        return;
    case 3:
    case 6:
        if (bottom > TI32(A, 0x24) + 48) {
            TF32(A, 0x44) = (float)((double)TI32(B, 0x2c) * (double)0.0078125f);
            TF32(A, 0x40) = (float)(bottom - 48);
        }
        return;
    case 11:
        if (bottom > TI32(A, 0x24) + 80 && TI32(B, 0x2c) > 0)
            task_pause_mask(8);
        return;
    case 1:
        if (bottom > TI32(A, 0x24) + 10) {
            TI32(A, 0x50) = 0;
            TI32(B, 0x38) = TI32(B, 0x38) - 1;
            TU8(B, 0x3c) = 1;
            if (TI32(A, 0x30) > 0)
                TI32(B, 0x38) = 0;
            TI32(A, 0x30) = 0;
            TI32(B, 0x2c) += -10 * TI32(B, 0x30);
            combo_add();
            slash_tail(A);
            return;
        }
        if (bottom > TI32(A, 0x24) + 90 && TI32(B, 0x2c) > 0)
            task_pause_mask(8);
        return;
    case 4:
        if ((double)TF32(A, 0x44) * (double)6.0f + (double)TI32(A, 0x24) < (double)bottom
            || bottom > TI32(A, 0x24) + 10) {
            TI32(A, 0x50) = 3;
            TI32(B, 0x38) = TI32(B, 0x38) - 1;
            TU8(B, 0x3c) = 1;
            if (TI32(A, 0x30) > 0)
                TI32(B, 0x38) = 0;
            TI32(A, 0x30) = 0;
            TI32(B, 0x2c) += -10 * TI32(B, 0x30);
            combo_add();
            slash_tail(A);
        }
        return;
    case 7:
        if (bottom > TI32(A, 0x24) + 50) {
            TI32(A, 0x50) = 8;
            TI32(A, 0x30) = 0;
            A->fn = f_4024f0;
            TI32(B, 0x2c) = shl(-TI32(B, 0x30), 5);
            combo_break();
            snd_play(5);
        }
        return;
    case 8:
        if (bottom > TI32(A, 0x24) + 60)
            TI32(B, 0x24) += (-512) / TI32(B, 0x30);
        return;
    case 10:
        if (bottom > TI32(A, 0x24) + 50) {
            TI32(A, 0x50) = 9;
            TI32(A, 0x30) = 0;
            TU32(A, 0x44) = 0x41000000;  // 8.0f
            TF32(A, 0x40) = (float)(bottom - 50);
            A->fn = f_402590;
            TI32(B, 0x2c) = shl(-TI32(B, 0x30), 4);
            snd_play(5);
        }
        return;
    default:  // 5, 9, > 11
        return;
    }
}

// 0x405a00
void f_405a00(Task* player, Task* rock)
{
    if (MU8(GS + 0x29) == 0)
        return;
    Task d;
    task_desc(&d);
    d.prio = 0x20;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_405a60;
    TTASK(&d, 0x20) = player;
    TTASK(&d, 0x24) = rock;
    task_create(&d);
}

// 0x405a60: special mode auto hit against a rock.
void f_405a60(Task* t)
{
    Task* A = TTASK(t, 0x20);
    Task* B = TTASK(t, 0x24);
    if (is_dead(A)) {
        set_dead(t);
        return;
    }
    if (is_dead(B)) {
        set_dead(t);
        return;
    }
    int32_t bottom = TI32(B, 0x20) + 0x78;
    if (TI32(A, 0x24) + 0x96 < bottom && TU8(B, 0x3c) == 0) {
        TU8(B, 0x3c) = 1;
        TI32(B, 0x38) = 0;
        MI32(GS + 0x14) += 1;
        snd_play(crt_rand() % 3);
        if (MI32(GS + 0x14) > 999)
            MI32(GS + 0x14) = 999;
        f_403110(0xba, bottom - MI32(GS + 8) - 30);
    }
}

// 0x405b10
void f_405b10(Task* player, Task* bolt)
{
    Task d;
    task_desc(&d);
    d.prio = 0x20;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_405b60;
    TTASK(&d, 0x20) = player;
    TTASK(&d, 0x24) = bolt;
    task_create(&d);
}

// 0x405b60: lightning bolt B (+0x20 top y, +0x24 x, +0x2c hit flag,
// +0x30 column, +0x34 harmless flag) vs. the player.
void f_405b60(Task* t)
{
    Task* B = TTASK(t, 0x24);
    Task* A = TTASK(t, 0x20);
    if (is_dead(B)) {
        set_dead(t);
        return;
    }
    int32_t top = TI32(B, 0x20);
    int32_t state = TI32(A, 0x50);
    int32_t ey = top + 0x140;
    switch ((uint32_t)state) {
    case 0: case 2: case 3: case 7: case 10: case 11: {
        int32_t py = TI32(A, 0x24) + 0x6e;
        if (ey <= py) return;
        if (top >= py) return;
        if (TI32(A, 0x30) != 0) return;
        if (TI32(B, 0x24) + 0x14 >= TI32(A, 0x20) + 0x58) return;
        if (TI32(B, 0x24) + 0x3c <= TI32(A, 0x20) + 0x28) return;
        if (TI32(A, 0x2c) <= 0) return;
        if (TU8(B, 0x34) != 0) return;
        TI32(A, 0x30) = 180;
        TI32(A, 0x2c) = TI32(A, 0x2c) - 1;
        TU8(B, 0x2c) = 1;
        if (TI32(A, 0x50) == 3 || TI32(A, 0x50) == 10) {
            TI32(A, 0x50) = 14;
            A->fn = f_402e00;
        } else {
            TI32(A, 0x50) = 13;
            A->fn = f_402d40;
        }
        combo_break();
        gauge_add30(A, 0x38);
        bonus_add90();
        snd_play(13);
        if (TI32(A, 0x2c) <= 0)
            f_403f50();
        return;
    }
    case 13: case 14: {
        int32_t py = TI32(A, 0x24) + 0x6e;
        if (ey <= py) return;
        if (top >= py) return;
        if (TI32(B, 0x24) + 0x14 >= TI32(A, 0x20) + 0x58) return;
        if (TI32(B, 0x24) + 0x3c <= TI32(A, 0x20) + 0x28) return;
        if (TU8(B, 0x34) != 0) return;
        TI32(A, 0x54) = 0;
        return;
    }
    case 1:
        if (ey <= TI32(A, 0x24) + 10) return;
        if (TI32(B, 0x30) != TI32(A, 0x28)) return;
        if (TU8(B, 0x34) != 0) return;
        TI32(A, 0x50) = 0;
        TU8(B, 0x2c) = 1;
        TI32(A, 0x30) = 0;
        break;
    case 4:
        if (!((double)TF32(A, 0x44) * (double)6.0f + (double)TI32(A, 0x24) < (double)ey)
            && ey <= TI32(A, 0x24) + 10)
            return;
        if (TI32(A, 0x28) != TI32(B, 0x30)) return;
        if (TU8(B, 0x34) != 0) return;
        TI32(A, 0x50) = 3;
        TU8(B, 0x2c) = 1;
        TI32(A, 0x30) = 0;
        break;
    default:  // 5, 6, 8, 9, 12, > 14
        return;
    }
    // slashed the bolt
    combo_add();
    f_403110(TI32(A, 0x20) - 70, TI32(A, 0x24) - MI32(GS + 8) - 30);
    task_set_wait_mask(0x7f, 3);
    snd_play(1);
}

// 0x405f20
void f_405f20(Task* player, Task* obj)
{
    Task d;
    task_desc(&d);
    d.prio = 0x10;
    d.f04 = 0;
    d.wait = 0;
    d.parent = nullptr;
    d.fn = f_405f70;
    TTASK(&d, 0x20) = player;
    TTASK(&d, 0x24) = obj;
    task_create(&d);
}
