// Part C: original functions 0x405f70 .. 0x408920.
//
// Mostly collision tasks between the player and the various falling /
// attacking objects (buildings, boulders, rubble, lightning...), the stage
// object spawners and a few small effect tasks.  See docs/notes/part_c.md.
//
// Player task layout used here (pl = player task, offsets of the record):
//   0x20 x   0x24 y   0x28 lane/column   0x2c lives   0x30 invincibility timer
//   0x38 float gauge (0..100)   0x40 float y   0x44 float vy   0x50 state
//   0x54 flag cleared when touching an object while in state 13/14
// Game state (GS = 0x41f688): +0x00 camera x, +0x08 camera y, +0x10 score,
//   +0x14 combo, +0x18 max combo, +0x1c special gauge (0..300),
//   +0x29 byte "stage gimmick active", +0x34, +0x3c score cap,
//   +0x40, +0x44 player task pointer.
#include "game.h"

// (functions of other parts are declared in game.h)

// ---------------------------------------------------------------------------
// Shared sequences (inlined many times in the original, order preserved)
// ---------------------------------------------------------------------------

// Player got hurt: remember the best combo and reset the current one.
static inline void c_combo_break() {
    if (MI32(GS + 0x14) > MI32(GS + 0x18)) MI32(GS + 0x18) = MI32(GS + 0x14);
    MI32(GS + 0x14) = 0;
}

// Damage gauge +30 (max 100) and special gauge +90 (max 300).
static inline void c_damage_gauges(Task* pl) {
    double g = (double)TF32(pl, 0x38) + (double)30.0f;   // fld/fadd/fst
    TF32(pl, 0x38) = (float)g;
    if (g > (double)100.0f) TF32(pl, 0x38) = 100.0f;    // fcomp st(0)
    MI32(GS + 0x1c) += 0x5a;
    if (MI32(GS + 0x1c) > 300) MI32(GS + 0x1c) = 300;
}

// Successful hit: combo +1 (max 999), special gauge += min(30,(combo+1)/2).
static inline void c_combo_add() {
    MI32(GS + 0x14) += 1;
    if (MI32(GS + 0x14) > 999) MI32(GS + 0x14) = 999;
    if ((MI32(GS + 0x14) + 1) / 2 > 30) MI32(GS + 0x1c) += 30;
    else MI32(GS + 0x1c) += (MI32(GS + 0x14) + 1) / 2;
    if (MI32(GS + 0x1c) > 300) MI32(GS + 0x1c) = 300;
}

// Hit feedback: spark, combo popup, hit stop and a random slash sound.
static inline void c_hit_feedback(int32_t px, int32_t py) {
    f_408c10(px, py - 30);
    f_403110(px - 70, py - MI32(GS + 8) - 30);
    task_set_wait_mask(0x7f, 3);
    snd_play(crt_rand() % 3);
}

static inline int32_t c_abs(int32_t v) {
    return v < 0 ? (int32_t)(0u - (uint32_t)v) : v;
}

// ---------------------------------------------------------------------------
// 0x405f70: collision task "player vs small object" (12x? object at
// TTASK(t,0x24)); reacts to the player state.
void f_405f70(Task* t) {
    Task* pr = TTASK(t, 0x24);
    Task* pl = TTASK(t, 0x20);
    if ((uint8_t)pr->dead) { t->dead = 1; return; }
    int32_t oy = TI32(pr, 0x24);
    int32_t oy12 = oy + 0xc;
    uint32_t st = (uint32_t)TI32(pl, 0x50);
    if (st > 0xe) return;
    int32_t ox, px, py;
    switch (st) {
    case 0: case 2: case 11:
        ox = TI32(pr, 0x20); px = TI32(pl, 0x20);
        if (ox + 5 >= px + 0x44) return;
        if (ox + 0xc <= px + 0x26) return;
        py = TI32(pl, 0x24);
        if (oy12 <= py + 0x36) return;
        if (TI32(pr, 0x24) >= py + 0x6e) return;
        if (TI32(pl, 0x30) != 0) return;
        TI32(pl, 0x30) = 0xb4;
        TI32(pl, 0x50) = 0xd;
        TI32(pl, 0x2c) -= 1;
        TTASK(t, 0x20)->fn = f_402d40;
        TU8(pr, 0x30) = 1;
        c_combo_break();
        c_damage_gauges(pl);
        snd_play(13);
        if (TI32(pl, 0x2c) <= 0) f_403f50();
        return;
    case 3:
        ox = TI32(pr, 0x20); px = TI32(pl, 0x20);
        if (ox + 5 >= px + 0x44) return;
        if (ox + 0xc <= px + 0x26) return;
        py = TI32(pl, 0x24);
        if (oy12 <= py + 0x36) return;
        if (TI32(pr, 0x24) >= py + 0x6e) return;
        TI32(pl, 0x30) = 0xb4;
        TI32(pl, 0x50) = 0xe;
        TI32(pl, 0x2c) -= 1;
        TTASK(t, 0x20)->fn = f_402e00;
        TU8(pr, 0x30) = 1;
        c_combo_break();
        c_damage_gauges(pl);
        snd_play(13);
        if (TI32(pl, 0x2c) <= 0) f_403f50();
        return;
    case 13: case 14:
        ox = TI32(pr, 0x20); px = TI32(pl, 0x20);
        if (ox + 5 >= px + 0x44) return;
        if (ox + 0xc <= px + 0x26) return;
        py = TI32(pl, 0x24);
        if (oy12 <= py + 0x36) return;
        if (oy >= py + 0x6e) return;
        TI32(pl, 0x54) = 0;
        return;
    case 1:
        ox = TI32(pr, 0x20); px = TI32(pl, 0x20);
        if (ox + 5 >= px + 0x62) return;
        if (ox + 0xc <= px + 8) return;
        py = TI32(pl, 0x24);
        if (oy12 <= py + 0xa) return;
        if (oy >= py + 0x6e) return;
        TI32(pl, 0x50) = 0;
        TU8(pr, 0x30) = 1;
        TI32(pr, 0x34) = 1;
        c_combo_add();
        c_hit_feedback(TI32(pl, 0x20), TI32(pl, 0x24));
        return;
    case 4:
        ox = TI32(pr, 0x20); px = TI32(pl, 0x20);
        if (ox + 5 >= px + 0x62) return;
        if (ox + 0xc <= px + 8) return;
        py = TI32(pl, 0x24);
        // fild oy12 ; fld vy ; fmul 6.0f ; fiadd py ; fcompp
        if (!((double)TF32(pl, 0x44) * (double)6.0f + (double)py < (double)oy12)) {
            if (oy12 <= py + 0xa) return;
        }
        TI32(pl, 0x50) = 3;
        TU8(pr, 0x30) = 1;
        TI32(pr, 0x34) = 1;
        c_combo_add();
        c_hit_feedback(TI32(pl, 0x20), TI32(pl, 0x24));
        return;
    case 7:
        ox = TI32(pr, 0x20); px = TI32(pl, 0x20);
        if (ox + 5 >= px + 0x62) return;
        if (ox + 0xc <= px + 8) return;
        if (oy12 <= TI32(pl, 0x24) + 0x32) return;
        TI32(pl, 0x50) = 8;
        TI32(pl, 0x30) = 0;
        TTASK(t, 0x20)->fn = f_4024f0;
        TU8(pr, 0x30) = 1;
        c_combo_break();
        snd_play(5);
        return;
    case 8:
        ox = TI32(pr, 0x20); px = TI32(pl, 0x20);
        if (ox + 5 >= px + 0x62) return;
        if (ox + 0xc <= px + 8) return;
        if (oy12 <= TI32(pl, 0x24) + 0x3c) return;
        TU8(pr, 0x30) = 1;
        return;
    case 10:
        ox = TI32(pr, 0x20); px = TI32(pl, 0x20);
        if (ox + 5 >= px + 0x62) return;
        if (ox + 0xc <= px + 8) return;
        if (oy12 <= TI32(pl, 0x24) + 0x32) return;
        TI32(pl, 0x50) = 9;
        TI32(pl, 0x30) = 0;
        TF32(pl, 0x44) = 8.0f;
        TTASK(t, 0x20)->fn = f_402590;
        TU8(pr, 0x30) = 1;
        snd_play(5);
        return;
    default: // 5, 6, 9, 12
        return;
    }
}

// 0x406590: spawn f_4065e0 (player vs 48x48 object collision), prio 0x10.
void f_406590(Task* pl, Task* obj) {
    Task d; task_desc(&d);
    d.prio = 0x10; d.fn = f_4065e0;
    TTASK(&d, 0x20) = pl;
    TTASK(&d, 0x24) = obj;
    task_create(&d);
}

// 0x4065e0: collision "player vs 48x48 object" (TTASK(t,0x24)); only hurts.
void f_4065e0(Task* t) {
    Task* obj = TTASK(t, 0x24);
    Task* pl = TTASK(t, 0x20);
    if ((uint8_t)obj->dead) { t->dead = 1; return; }
    uint32_t st = (uint32_t)TI32(pl, 0x50);
    int32_t oy = TI32(obj, 0x24);
    if (st > 0xb) return;
    static const uint8_t kind[12] = {0, 0, 0, 1, 1, 2, 2, 0, 0, 2, 1, 0};
    int k = kind[st];
    if (k == 2) return;
    int32_t ox = TI32(obj, 0x20), px = TI32(pl, 0x20);
    if (ox + 0x10 >= px + 0x44) return;
    if (ox + 0x30 <= px + 0x26) return;
    int32_t py = TI32(pl, 0x24);
    if (oy + 0x30 <= py + 0x36) return;
    if (oy >= py + 0x6e) return;
    if (k == 0) {
        if (TI32(pl, 0x30) != 0) return;
        TI32(pl, 0x30) = 0xb4;
        TI32(pl, 0x2c) -= 1;
        TI32(pl, 0x50) = 0xd;
        TTASK(t, 0x20)->fn = f_402d40;
    } else {
        TI32(pl, 0x30) = 0xb4;
        TI32(pl, 0x2c) -= 1;
        TI32(pl, 0x50) = 0xe;
        TTASK(t, 0x20)->fn = f_402e00;
    }
    TU8(obj, 0x30) = 1;
    c_combo_break();
    c_damage_gauges(pl);
    snd_play(13);
    if (TI32(pl, 0x2c) <= 0) f_403f50();
}

// 0x406840: spawn f_406890 (player vs falling building collision), prio 0x20.
void f_406840(Task* pl, Task* bld) {
    Task d; task_desc(&d);
    d.prio = 0x20; d.fn = f_406890;
    TTASK(&d, 0x20) = pl;
    TTASK(&d, 0x24) = bld;
    task_create(&d);
}

// 0x406890: collision "player vs falling building" (TTASK(t,0x24) is a
// building task: 0x20 y, 0x24 vy, 0x28 y*64, 0x2c weight, 0x30 floors,
// 0x38 byte hit, 0x3c, 0x40 hit power).
void f_406890(Task* t) {
    Task* pl = TTASK(t, 0x20);
    Task* b = TTASK(t, 0x24);
    if ((uint8_t)b->dead) {
        MU8(GS + 0x29) = 0;
        MI32(GS + 0x40) = 0;
        t->dead = 1;
        return;
    }
    int32_t bot = TI32(b, 0x20) + TI32(b, 0x30) * 118 + 2;
    if (bot == 0x1ac && TI32(pl, 0x30) == 0 && pl->wait > 0 && TI32(pl, 0x2c) > 0) {
        // crushed by the building landing on the ground
        task_resume_mask(8);
        TI32(pl, 0x2c) -= 1;
        TI32(pl, 0x30) = 0xb4;
        TI32(pl, 0x50) = 0xc;
        TTASK(t, 0x20)->fn = f_4021d0;
        c_combo_break();
        c_damage_gauges(pl);
        snd_play(6);
        if (TI32(pl, 0x2c) <= 0) f_403f50();
    }
    uint32_t st = (uint32_t)TI32(pl, 0x50);
    if (st > 0xb) return;
    int32_t py = TI32(pl, 0x24);
    switch (st) {
    case 0: case 2:
        if (bot <= py + 0x5a) return;
        if (TI32(b, 0x24) <= 0) return;
        if (TI32(pl, 0x30) != 0) return;
        task_pause_mask(8);
        return;
    case 11:
        if (bot <= py + 0x50) return;
        if (TI32(b, 0x24) <= 0) return;
        task_pause_mask(8);
        return;
    case 1:
        if (bot <= py + 0xa) {
            if (bot <= py + 0x5a) return;
            if (TI32(b, 0x24) <= 0) return;
            task_pause_mask(8);
            return;
        }
        TI32(pl, 0x50) = 0;
        goto hit;
    case 4:
        if (!((double)TF32(pl, 0x44) * (double)6.0f + (double)py < (double)bot)) {
            if (bot <= py + 0xa) return;
        }
        TI32(pl, 0x50) = 3;
        goto hit;
    case 3: case 6:
        if (bot <= py + 0x30) return;
        TF32(pl, 0x44) = (float)((double)TI32(b, 0x24) * (double)0.0078125f);
        TF32(pl, 0x40) = (float)(double)(bot - 0x30);
        return;
    case 7:
        if (bot <= py + 0x32) return;
        TI32(pl, 0x50) = 8;
        TI32(pl, 0x30) = 0;
        TTASK(t, 0x20)->fn = f_4024f0;
        TI32(b, 0x24) = (int32_t)((uint32_t)(-TI32(b, 0x2c)) << 5);
        c_combo_break();
        snd_play(5);
        return;
    case 8:
        if (bot <= py + 0x3c) return;
        TI32(b, 0x28) += (int32_t)-0x200 / TI32(b, 0x2c);
        return;
    case 10:
        if (bot <= py + 0x32) return;
        TI32(pl, 0x50) = 9;
        TI32(pl, 0x30) = 0;
        TF32(pl, 0x40) = (float)(double)(bot - 0x32);
        TF32(pl, 0x44) = 8.0f;
        TTASK(t, 0x20)->fn = f_402590;
        TI32(b, 0x24) = (int32_t)((uint32_t)(-TI32(b, 0x2c)) << 4);
        snd_play(5);
        return;
    default: // 5, 9
        return;
    }
hit:
    // the player's attack hits the building
    TU8(b, 0x38) = 1;
    TI32(b, 0x40) = 1;
    if (TI32(pl, 0x30) > 0) TI32(b, 0x40) = 5;
    TI32(pl, 0x30) = 0;
    {
        int32_t w = TI32(b, 0x2c);
        TI32(b, 0x3c) = TI32(pl, 0x28);
        TI32(b, 0x24) = TI32(b, 0x24) + w * -10;
    }
    c_combo_add();
    c_hit_feedback(TI32(pl, 0x20), TI32(pl, 0x24));
}

// 0x406dc0: if the stage gimmick is active, spawn f_406e20 (prio 0x20).
void f_406dc0(Task* a, Task* b) {
    if (MU8(GS + 0x29) == 0) return;
    Task d; task_desc(&d);
    d.prio = 0x20; d.fn = f_406e20;
    TTASK(&d, 0x20) = a;
    TTASK(&d, 0x24) = b;
    task_create(&d);
}

// 0x406e20: "a" (TTASK(t,0x20)) reaching building "b" (TTASK(t,0x24)):
// marks the building as hit by the special attack.
void f_406e20(Task* t) {
    Task* a = TTASK(t, 0x20);
    Task* b = TTASK(t, 0x24);
    if ((uint8_t)a->dead) { t->dead = 1; return; }
    if ((uint8_t)b->dead) {
        MU8(GS + 0x29) = 0;
        MI32(GS + 0x40) = 0;
        t->dead = 1;
        return;
    }
    int32_t bot = TI32(b, 0x20) + TI32(b, 0x30) * 118 + 2;
    if (TI32(a, 0x24) + 0x96 >= bot) return;
    if (TU8(b, 0x38) != 0) return;
    TU8(b, 0x38) = 1;
    TI32(b, 0x40) = 5;
    TI32(b, 0x3c) = TI32(MTASK(GS + 0x44), 0x28);
    MI32(GS + 0x14) += 1;
    snd_play(crt_rand() % 3);
    if (MI32(GS + 0x14) > 999) MI32(GS + 0x14) = 999;
    f_403110(0xba, bot - MI32(GS + 8) - 0x1e);
}

// 0x406f00: spawn f_406f60 (player vs big object collision), prio 0x20.
void f_406f00(Task* pl, Task* obj) {
    Task d; task_desc(&d);
    d.prio = 0x20; d.fn = f_406f60;
    TTASK(&d, 0x20) = pl;
    TTASK(&d, 0x28) = obj;   // +0x24/+0x2c are uninitialised in the original
    task_create(&d);
}

// 0x406f60: collision "player vs big falling object" (TTASK(t,0x28):
// 0x20 y (bottom = y + 468), 0x24 vy, 0x28, 0x2c weight, 0x34 byte hit,
// 0x38 hit power).
void f_406f60(Task* t) {
    Task* pl = TTASK(t, 0x20);
    Task* o = TTASK(t, 0x28);
    if ((uint8_t)o->dead) {
        MU8(GS + 0x29) = 0;
        MI32(GS + 0x40) = 0;
        t->dead = 1;
        return;
    }
    int32_t bot = TI32(o, 0x20) + 0x1d4;
    if (bot == 0x1ad && TI32(pl, 0x30) == 0 && pl->wait > 0 && TI32(pl, 0x2c) > 0) {
        task_resume_mask(8);
        TI32(pl, 0x2c) -= 1;
        TI32(pl, 0x30) = 0xb4;
        TI32(pl, 0x50) = 0xc;
        TTASK(t, 0x20)->fn = f_4021d0;
        c_combo_break();
        c_damage_gauges(pl);
        snd_play(6);
        if (TI32(pl, 0x2c) > 0) return;
        f_403f50();
        return;
    }
    uint32_t st = (uint32_t)TI32(pl, 0x50);
    if (st > 0xe) return;
    int32_t py = TI32(pl, 0x24);
    switch (st) {
    case 12:
        if (TI32(o, 0x24) >= 0) return;
        TI32(pl, 0x50) = 0;
        TTASK(t, 0x20)->fn = f_401fb0;
        TI32(pl, 0x30) = 0;
        return;
    case 0: case 2:
        if (bot <= py + 0x5a) return;
        if (TI32(o, 0x24) <= 0) return;
        if (TI32(pl, 0x30) != 0) return;
        task_pause_mask(8);
        return;
    case 3: case 14:
        if (bot <= py + 0x30) return;
        TF32(pl, 0x44) = (float)((double)TI32(o, 0x24) * (double)0.0078125f);
        TF32(pl, 0x40) = (float)(double)(bot - 0x2d);
        return;
    case 11:
        if (bot <= py + 0x50) return;
        if (TI32(o, 0x24) <= 0) return;
        task_pause_mask(8);
        return;
    case 1:
        if (bot <= py + 0xa) {
            if (bot <= py + 0x5a) return;
            if (TI32(o, 0x24) <= 0) return;
            task_pause_mask(8);
            return;
        }
        TI32(pl, 0x50) = 0;
        TU8(o, 0x34) = 1;
        TI32(o, 0x38) = 1;
        if (TI32(pl, 0x30) > 0) {
            int32_t w = TI32(o, 0x2c);
            int32_t y = TI32(o, 0x20) - 0x20;
            TI32(o, 0x38) = 5;
            TI32(o, 0x20) = y;
            TI32(o, 0x24) = (w * 3) * -9;          // -(3w) * 9
            TI32(o, 0x28) = (int32_t)((uint32_t)y << 6);
        }
        TI32(pl, 0x30) = 0;
        TI32(o, 0x24) = TI32(o, 0x24) + TI32(o, 0x2c) * -5;
        c_combo_add();
        c_hit_feedback(TI32(pl, 0x20), TI32(pl, 0x24));
        return;
    case 4:
        if (!((double)TF32(pl, 0x44) * (double)6.0f + (double)py < (double)bot)) {
            if (bot <= py + 0xa) return;
        }
        TI32(pl, 0x50) = 3;
        TU8(o, 0x34) = 1;
        TI32(o, 0x38) = 1;
        if (TI32(pl, 0x30) > 0) TI32(o, 0x38) = 5;
        TI32(pl, 0x30) = 0;
        TI32(o, 0x24) = TI32(o, 0x24) + TI32(o, 0x2c) * -5;
        c_combo_add();
        c_hit_feedback(TI32(pl, 0x20), TI32(pl, 0x24));
        return;
    case 6:
        if (bot <= py + 0x30) return;
        TF32(pl, 0x44) = (float)((double)TI32(o, 0x24) * (double)0.0078125f);
        TF32(pl, 0x40) = (float)(double)(bot - 0x30);
        return;
    case 7:
        if (bot <= py + 0x32) return;
        TI32(pl, 0x50) = 8;
        TI32(pl, 0x30) = 0;
        TTASK(t, 0x20)->fn = f_4024f0;
        TI32(o, 0x24) = (int32_t)((uint32_t)(-TI32(o, 0x2c)) << 5);
        c_combo_break();
        snd_play(5);
        return;
    case 8:
        if (bot <= py + 0x3c) return;
        TI32(o, 0x28) += (int32_t)-0x200 / TI32(o, 0x2c);
        return;
    case 10:
        if (bot <= py + 0x32) return;
        TI32(pl, 0x50) = 9;
        TI32(pl, 0x30) = 0;
        TF32(pl, 0x40) = (float)(double)(bot - 0x32);
        TF32(pl, 0x44) = 8.0f;
        TTASK(t, 0x20)->fn = f_402590;
        TI32(o, 0x24) = (int32_t)((uint32_t)(-TI32(o, 0x2c)) << 4);
        snd_play(5);
        return;
    default: // 5, 9, 13
        return;
    }
}

// 0x4074a0: if the stage gimmick is active spawn f_407520 (prio 0x20) with
// a hit counter of 5.
void f_4074a0(Task* a, Task* b, Task* c) {
    if (MU8(GS + 0x29) == 0) return;
    Task d; task_desc(&d);
    d.prio = 0x20; d.fn = f_407520;
    TTASK(&d, 0x20) = a;
    TTASK(&d, 0x24) = b;
    TTASK(&d, 0x28) = c;
    TI32(&d, 0x2c) = 5;
    task_create(&d);
}

// 0x407520: special attack "a" vs big object "c"; after 5 hits destroys
// a, b and itself, flashes the screen white and releases the hit stop.
void f_407520(Task* t) {
    Task* a = TTASK(t, 0x20);
    Task* c = TTASK(t, 0x28);
    if (TI32(t, 0x2c) <= 0) {
        t->dead = 1;
        TTASK(t, 0x20)->dead = 1;
        TTASK(t, 0x24)->dead = 1;
        f_403980(0xffffff);
        Task* x = TTASK(a, 0x34);
        if (x != nullptr) {
            TU8(x, 0x30) = 0;
            TI32(MTASK(GS + 0x44), 0x30) = 0;
            return;
        }
        task_resume_mask(0x7f);
        TI32(MTASK(GS + 0x44), 0x30) = 0;
        return;
    }
    if ((uint8_t)a->dead) { t->dead = 1; return; }
    if ((uint8_t)c->dead) {
        MU8(GS + 0x29) = 0;
        MI32(GS + 0x40) = 0;
        t->dead = 1;
        return;
    }
    if (TI32(a, 0x24) + 0x96 >= TI32(c, 0x20) + 0x1d4) return;
    if (TU8(c, 0x34) != 0) return;
    TU8(c, 0x34) = 1;
    TI32(c, 0x38) = 1;
    TI32(t, 0x2c) -= 1;
    MI32(GS + 0x14) += 1;
    snd_play(crt_rand() % 3);
    if (MI32(GS + 0x14) > 999) MI32(GS + 0x14) = 999;
    f_403110(TI32(a, 0x20) - 0x64, TI32(a, 0x24) - MI32(GS + 8) + 0x96);
}

// 0x407640: spawn f_407690 (player vs object attached to a parent), prio 0x10.
void f_407640(Task* pl, Task* obj) {
    Task d; task_desc(&d);
    d.prio = 0x10; d.fn = f_407690;
    TTASK(&d, 0x20) = pl;
    TTASK(&d, 0x24) = obj;
    task_create(&d);
}

// 0x407690: collision "player vs 0x50 wide object whose y is relative to its
// parent" (obj 0x20 x, 0x24 y offset, 0x3c byte hit, 0x40 hit power;
// parent 0x20 y, 0x24 vy).
void f_407690(Task* t) {
    Task* obj = TTASK(t, 0x24);
    Task* pl = TTASK(t, 0x20);
    Task* par = obj->parent;
    if ((uint8_t)obj->dead) { t->dead = 1; return; }
    int32_t bot = TI32(obj, 0x24) + TI32(par, 0x20) + 0x1c2;
    int32_t ox = TI32(obj, 0x20), px = TI32(pl, 0x20);
    if (ox >= px + 0x58) return;
    if (ox + 0x50 <= px + 0x28) return;
    uint32_t sel = (uint32_t)(TI32(pl, 0x50) - 1);
    if (sel > 0xd) return;
    static const uint8_t kind[14] = {0, 4, 1, 2, 4, 3, 4, 4, 4, 4, 4, 4, 4, 1};
    int32_t py;
    switch (kind[sel]) {
    case 0: // state 1
        if (bot <= TI32(pl, 0x24) + 0xa) return;
        TI32(pl, 0x50) = 0;
        TU8(obj, 0x3c) = 1;
        TI32(obj, 0x40) = 1;
        c_combo_add();
        c_hit_feedback(TI32(pl, 0x20), TI32(pl, 0x24));
        return;
    case 1: // state 3, 14
        if (bot <= TI32(pl, 0x24) + 0x30) return;
        TF32(pl, 0x44) = (float)((double)TI32(par, 0x24) * (double)0.0078125f);
        TF32(pl, 0x40) = (float)(double)(bot - 0x30);
        return;
    case 2: // state 4
        py = TI32(pl, 0x24);
        if (!((double)TF32(pl, 0x44) * (double)6.0f + (double)py < (double)bot)) {
            if (bot <= py + 0xa) return;
        }
        TI32(pl, 0x50) = 3;
        TU8(obj, 0x3c) = 1;
        TI32(obj, 0x40) = 1;
        c_combo_add();
        c_hit_feedback(TI32(pl, 0x20), TI32(pl, 0x24));
        return;
    case 3: // state 6
        if (bot <= TI32(pl, 0x24) + 0x30) return;
        TF32(pl, 0x44) = (float)((double)TI32(par, 0x24) * (double)0.0078125f);
        TF32(pl, 0x40) = (float)(double)(bot - 0x30);
        return;
    default:
        return;
    }
}

// 0x407990: spawn a destructible part (f_407a20, prio 8) at x = `x` on top of
// `parent`, with 6 hit points, and register it with the player collision.
void f_407990(int32_t x, Task* parent) {
    Task d; task_desc(&d);
    d.prio = 8; d.parent = parent; d.fn = f_407a20;
    TI32(&d, 0x20) = x;
    TI32(&d, 0x24) = 0;
    TI32(&d, 0x28) = 6;
    TI32(&d, 0x2c) = 0;
    TU8(&d, 0x30) = 0;
    TI32(&d, 0x34) = 0;
    Task* n = task_create(&d);
    f_4050f0(MTASK(GS + 0x44), n);
}

// 0x407a20: destructible part riding on its parent (surface 57, 100x100).
//   0x20 x, 0x24 y offset (rises 80 frames, sinks after 250), 0x28 hp,
//   0x2c age, 0x30 byte damaged flag, 0x34 damage.
void f_407a20(Task* t) {
    Task* par = t->parent;
    int32_t age = ++TI32(t, 0x2c);
    if (age < 0x50) TI32(t, 0x24) += 1;
    if (age > 0xfa) {
        if (--TI32(t, 0x24) < 0) { t->dead = 1; return; }
    }
    if (TU8(t, 0x30)) {
        int32_t dmg = TI32(t, 0x34);
        TU8(t, 0x30) = 0;
        TI32(t, 0x28) -= dmg;
        TI32(t, 0x34) = 0;
    }
    int32_t sx = TI32(t, 0x20) - MI32(GS + 0);
    int32_t py0 = TI32(par, 0x20);
    int32_t sy = TI32(t, 0x24) - MI32(GS + 8) + py0 + 0x154;
    bool destroy = false;
    if (TI32(t, 0x28) <= 0 || (uint8_t)par->dead) destroy = true;
    else if (TI32(par, 0x30) <= TI32(par, 0x38) && TU8(par, 0x34) != 0) destroy = true;
    if (!destroy) {
        if (TI32(t, 0x2c) == 0xd2) {
            f_4093e0(TI32(t, 0x20) + 0x12, py0 + 0x1d6, 0);
            snd_play(6);
        }
        if (TI32(t, 0x2c) == 0xd7) {
            f_4093e0(TI32(t, 0x20) + 0x12, TI32(par, 0x20) + 0x1d6, 0);
            snd_play(6);
        }
        if (TI32(t, 0x2c) == 0xdc) {
            f_4093e0(TI32(t, 0x20) + 0x12, TI32(par, 0x20) + 0x1d6, 0);
            snd_play(6);
        }
        gfx_blit(0x39, sx, sy, 100, 100, 0, 0);
        return;
    }
    t->dead = 1;
    f_404170(MI32(GS + 0) + sx + 0x28, MI32(GS + 8) + sy + 0x32, 1);
    f_404170(MI32(GS + 0) + sx + 0x14, MI32(GS + 8) + sy + 0x28, 1);
    f_404170(MI32(GS + 0) + sx + 0x3c, MI32(GS + 8) + sy + 0x1e, 1);
    f_404170(MI32(GS + 0) + sx + 0xa, MI32(GS + 8) + sy + 0xa, 1);
    f_404170(MI32(GS + 0) + sx + 0x50, MI32(GS + 8) + sy + 0x50, 1);
    f_404170(MI32(GS + 0) + sx + 0x1e, MI32(GS + 8) + sy + 0x46, 1);
    snd_play(7);
}

// 0x407c50: create the "stacked rubble" manager (f_407ce0, prio 0x10,
// f04 = 4, wait 180) lasting 1800 frames with 3 column spawners.
Task* f_407c50() {
    Task d; task_desc(&d);
    d.prio = 0x10; d.f04 = 4; d.wait = 0xb4; d.parent = nullptr; d.fn = f_407ce0;
    TI32(&d, 0x20) = 0x708;
    Task* n = task_create(&d);
    for (int32_t i = 0; i < 3; i++) {
        Task* c = f_407d40(n, i);
        TTASK(n, 0x24 + 4 * i) = c;
        c->wait = 0xb4;
    }
    f_403d60();
    return n;
}

// 0x407ce0: rubble manager; counts down 0x20 then waits until no column has
// pieces left (child 0x28 == 0) and ends the gimmick.
void f_407ce0(Task* t) {
    if (t->wait > 0) return;
    if (TI32(t, 0x20) > 0) { TI32(t, 0x20) -= 1; return; }
    int32_t i = 0;
    for (;;) {
        if (TI32(TTASK(t, 0x24 + 4 * i), 0x28) != 0) break;
        i++;
        if (i >= 3) break;
    }
    if (i != 3) return;
    t->dead = 1;
    MU8(GS + 0x29) = 0;
    MI32(GS + 0x40) = 0;
}

// 0x407d40: rubble column spawner for column `col` (f_407db0, prio 0x10).
//   0x20 last piece, 0x24 first piece, 0x28 piece count, 0x2c delay, 0x30 col
Task* f_407d40(Task* parent, int32_t col) {
    Task d; task_desc(&d);
    d.prio = 0x10; d.parent = parent; d.fn = f_407db0;
    TTASK(&d, 0x20) = nullptr;
    TTASK(&d, 0x24) = nullptr;
    TI32(&d, 0x28) = 0;
    TI32(&d, 0x2c) = 0;
    TI32(&d, 0x30) = col;
    return task_create(&d);
}

// 0x407db0: rubble column spawner: drops a new piece every 30..129 frames
// while the manager is running and links it on top of the previous piece.
void f_407db0(Task* t) {
    Task* par = t->parent;
    if ((uint8_t)par->dead) { t->dead = 1; return; }
    if (t->wait > 0) return;
    if (--TI32(t, 0x2c) >= 0) return;
    if (TI32(par, 0x20) <= 0) return;
    TI32(t, 0x28) += 1;
    Task* c = f_407e30(t, TI32(t, 0x30));
    if (TTASK(t, 0x20) != nullptr) {
        TTASK(TTASK(t, 0x20), 0x44) = c;
        f_408160(c, TTASK(t, 0x20));
        TTASK(t, 0x20) = c;
    } else {
        TTASK(t, 0x20) = c;
        TTASK(t, 0x24) = c;
    }
    TI32(t, 0x2c) = crt_rand() % 100 + 0x1e;
}

// 0x407e30: create a falling rubble piece (f_407ef0, prio 0x10) in column
// `col` above the camera and register it with the player collision.
//   0x20 y, 0x24 y*16, 0x28 x, 0x2c vy, 0x30 gravity (3), 0x34 byte,
//   0x38 hits (2), 0x3c byte hit, 0x40 column, 0x44 next piece
Task* f_407e30(Task* parent, int32_t col) {
    Task d; task_desc(&d);
    int32_t y = MI32(GS + 8) - 0xc8;
    d.prio = 0x10; d.parent = parent; d.fn = f_407ef0;
    TI32(&d, 0x20) = y;
    TI32(&d, 0x24) = (int32_t)((uint32_t)y << 4);
    TI32(&d, 0x28) = (col * 3 + 3) * 5 * 8;
    TI32(&d, 0x2c) = 0;
    TI32(&d, 0x30) = 3;
    TU8(&d, 0x34) = 0;
    TI32(&d, 0x38) = 2;
    TU8(&d, 0x3c) = 0;
    TI32(&d, 0x40) = col;
    TTASK(&d, 0x44) = nullptr;
    Task* n = task_create(&d);
    f_405440(MTASK(GS + 0x44), n);
    return n;
}

// 0x407ef0: falling rubble piece (surface 44, 160x120).
void f_407ef0(Task* t) {
    Task* par = t->parent;
    int32_t vy = TI32(t, 0x2c);
    if (vy < 0) TU8(t, 0x34) = 0;
    bool landed = (TI32(t, 0x20) == 0x135);
    bool die = false;
    int32_t yy = TI32(t, 0x24) + vy;
    TI32(t, 0x24) = yy;
    TI32(t, 0x20) = yy >> 4;
    TI32(t, 0x2c) = TI32(t, 0x30) + vy;
    if (TU8(t, 0x3c)) {
        MI32(GS + 0x10) = ftol((double)MI32(GS + 0x14) * (double)TI32(t, 0x30) * (double)45.0f
                               + (double)MI32(GS + 0x10));
        if (MI32(GS + 0x10) > MI32(GS + 0x3c)) MI32(GS + 0x10) = MI32(GS + 0x3c);
        if (TI32(t, 0x38) <= 0) {
            MI32(GS + 0x10) = ftol((double)MI32(GS + 0x14) * (double)TI32(t, 0x30) * (double)405.0f
                                   + (double)MI32(GS + 0x10));
            if (MI32(GS + 0x10) > MI32(GS + 0x3c)) MI32(GS + 0x10) = MI32(GS + 0x3c);
            die = true;
        }
    }
    if (TI32(t, 0x20) + 0x78 >= 0x1ad) {
        TI32(t, 0x24) = 0x1350;
        TI32(t, 0x20) = 0x135;
        if (landed && TU8(t, 0x34) == 0) die = true;
        TI32(t, 0x2c) = 0;
    }
    if (die) {
        t->dead = 1;
        snd_play(6);
        f_4080a0(TI32(t, 0x28), TI32(t, 0x20) + 0x32);
        TI32(par, 0x28) -= 1;
        Task* nx = TTASK(t, 0x44);
        if (nx != nullptr) {
            TTASK(par, 0x24) = nx;
        } else {
            TTASK(par, 0x24) = nullptr;
            TTASK(par, 0x20) = nullptr;
        }
        return;
    }
    gfx_blit(0x2c, TI32(t, 0x28) - MI32(GS + 0), TI32(t, 0x20) - MI32(GS + 8), 0xa0, 0x78, 0, 0);
}

// 0x4080a0: spawn the rubble break effect (f_4080f0, prio 0x10).
void f_4080a0(int32_t x, int32_t y) {
    Task d; task_desc(&d);
    d.prio = 0x10; d.fn = f_4080f0;
    TI32(&d, 0x20) = x;
    TI32(&d, 0x24) = y;
    TI32(&d, 0x28) = 0;
    task_create(&d);
}

// 0x4080f0: rubble break effect: 4 frames of 8 debris particles each.
void f_4080f0(Task* t) {
    if (++TI32(t, 0x28) > 3) { t->dead = 1; return; }
    for (int32_t i = 0; i < 0xa0; i += 0x14) {
        int32_t kind = crt_rand() % 2 + 1;
        int32_t y = crt_rand() % 0x3c + TI32(t, 0x24);
        int32_t x = crt_rand() % 10 + TI32(t, 0x20) + i - 0x14;
        f_404170(x, y, kind);
    }
}

// 0x408160: spawn f_4081b0 (piece `a` landing on piece `b`), prio 0x20.
void f_408160(Task* a, Task* b) {
    Task d; task_desc(&d);
    d.prio = 0x20; d.fn = f_4081b0;
    TTASK(&d, 0x20) = a;
    TTASK(&d, 0x24) = b;
    task_create(&d);
}

// 0x4081b0: rubble piece `a` stacking on piece `b`: when it touches, it is
// placed 85 px above and both share the velocity.
void f_4081b0(Task* t) {
    Task* b = TTASK(t, 0x24);
    Task* a = TTASK(t, 0x20);
    if ((uint8_t)b->dead) { t->dead = 1; return; }
    if (TI32(a, 0x20) + 0x78 <= TI32(b, 0x20) + 0x22) return;
    int32_t va = TI32(a, 0x2c);
    int32_t vb = TI32(b, 0x2c);
    if (c_abs(va - vb) > 0xae) snd_play(5);
    TI32(a, 0x24) = TI32(b, 0x24) - 0x550;
    TI32(a, 0x2c) = vb;
    TI32(b, 0x2c) = vb - (vb - va) / 6;
}

// 0x408230: spawn the jump dust effect (f_4082c0, prio 0x10) at (x, y).
//   0x28.. frame source rects: (0,0) (0x40,0) (0x80,0)
Task* f_408230(int32_t x, int32_t y) {
    Task d; task_desc(&d);
    d.prio = 0x10; d.fn = f_4082c0;
    TI32(&d, 0x20) = x;
    TI32(&d, 0x24) = y;
    TI32(&d, 0x28) = 0;
    TI32(&d, 0x2c) = 0;
    TI32(&d, 0x30) = 0x40;
    TI32(&d, 0x34) = 0;
    TI32(&d, 0x38) = 0x80;
    TI32(&d, 0x3c) = 0;
    TI32(&d, 0x40) = 0;
    TI32(&d, 0x44) = 0;
    return task_create(&d);
}

// 0x4082c0: jump dust animation (surface 9, 64x64, 3 frames x 3 ticks).
void f_4082c0(Task* t) {
    int32_t c = ++TI32(t, 0x44);
    if (c % 3 == 0) {
        if (++TI32(t, 0x40) > 2) { t->dead = 1; return; }
    }
    int32_t x = TI32(t, 0x20) - MI32(GS + 0);
    int32_t y = TI32(t, 0x24) - MI32(GS + 8);
    int32_t f = TI32(t, 0x40);
    gfx_blit(9, x, y, 0x40, 0x40, TI32(t, 0x28 + f * 8), TI32(t, 0x2c + f * 8));
}

// 0x408330: create the thunderstorm gimmick (f_408390, prio 0x10, f04 = 3,
// wait 120) and play the rumble sound.
Task* f_408330() {
    Task d; task_desc(&d);
    d.prio = 0x10; d.f04 = 3; d.wait = 0x78; d.parent = nullptr; d.fn = f_408390;
    TI32(&d, 0x20) = 0;
    TI32(&d, 0x24) = 0;
    Task* n = task_create(&d);
    snd_play(14);
    return n;
}

// 0x408390: thunderstorm: for 1800 frames, randomly drops lightning on the
// player's lane (40%) or a random lane, with a light blue screen flash.
void f_408390(Task* t) {
    if (t->wait > 0) return;
    int32_t age = ++TI32(t, 0x20);
    if (age > 0x708) {
        t->dead = 1;
        MU8(GS + 0x29) = 0;
        MI32(GS + 0x40) = 0;
        return;
    }
    int32_t c = ++TI32(t, 0x24);
    if (age >= 0x6a4) return;
    if (c <= 0x28) return;
    if (crt_rand() % 0x28 != 0) return;
    int32_t lane;
    if (crt_rand() % 100 < 0x28) lane = TI32(MTASK(GS + 0x44), 0x28);
    else lane = crt_rand() % 3;
    f_408440(lane);
    f_403980(0x93fdfd);
    TI32(t, 0x24) = 0;
}

// 0x408440: spawn a lightning bolt (f_4084f0, prio 0x10) on lane `lane`.
//   0x20 y, 0x24 x, 0x28 vy, 0x2c byte hit by the player, 0x30 lane,
//   0x34 byte grounded, 0x38 frame, 0x3c tick
void f_408440(int32_t lane) {
    Task d; task_desc(&d);
    d.prio = 0x10; d.fn = f_4084f0;
    TI32(&d, 0x20) = MI32(GS + 8) - 0x320;
    TI32(&d, 0x24) = (lane * 65) * 2 + 0x96;
    TI32(&d, 0x28) = 0x19;
    TU8(&d, 0x2c) = 0;
    TI32(&d, 0x30) = lane;
    TU8(&d, 0x34) = 0;
    TI32(&d, 0x38) = 0;
    TI32(&d, 0x3c) = 0;
    Task* n = task_create(&d);
    f_405b10(MTASK(GS + 0x44), n);
    snd_play(6);
}

// 0x4084f0: lightning bolt (surface 43, 80x320, 5 frames).
void f_4084f0(Task* t) {
    if (TU8(t, 0x2c) && !TU8(t, 0x34)) {
        // cut by the player: bonus = combo * 3000
        MI32(GS + 0x10) = ftol((double)MI32(GS + 0x14) * (double)3000.0f + (double)MI32(GS + 0x10));
        if (MI32(GS + 0x10) > MI32(GS + 0x3c)) MI32(GS + 0x10) = MI32(GS + 0x3c);
        TU8(t, 0x34) = 1;
    }
    if (TU8(t, 0x34)) {
        int32_t c = ++TI32(t, 0x3c);
        if (c % 3 == 0) {
            if (++TI32(t, 0x38) > 4) {
                t->dead = 1;
                snd_play(12);
            }
        }
    } else {
        TI32(t, 0x20) += TI32(t, 0x28);
    }
    if (TI32(t, 0x20) + 0x12c >= 0x1ad) {
        TI32(t, 0x20) = 0x81;
        TU8(t, 0x34) = 1;
    }
    int32_t x = TI32(t, 0x24) - MI32(GS + 0);
    int32_t y = TI32(t, 0x20) - MI32(GS + 8);
    gfx_blit(0x2b, x, y, 0x50, 0x140, TI32(t, 0x38) * 0x50, 0);
}

// 0x4085f0: spawn a cloud (f_408660, prio 4) at (x, y); `loop` non zero
// makes it wrap around instead of disappearing.
void f_4085f0(int32_t x, int32_t y, int32_t loop) {
    Task d; task_desc(&d);
    d.prio = 4; d.fn = f_408660;
    TI32(&d, 0x20) = (int32_t)((uint32_t)x << 4);
    TI32(&d, 0x24) = y;
    TU8(&d, 0x28) = (uint8_t)loop;
    task_create(&d);
}

// 0x408660: drifting cloud (surface 18, 175x78); x in 1/16 px.
void f_408660(Task* t) {
    uint8_t loop = TU8(t, 0x28);
    TI32(t, 0x20) -= 4;
    if (!loop) TI32(t, 0x20) -= 0xc;
    if (TI32(t, 0x20) < -0xaf0) {
        TI32(t, 0x20) = 0x2800;
        if (loop) return;
        t->dead = 1;
        return;
    }
    int32_t x = (TI32(t, 0x20) >> 4) - MI32(GS + 0);
    int32_t y = TI32(t, 0x24) - MI32(GS + 8);
    gfx_blit(0x12, x, y, 0xaf, 0x4e, 0, 0);
}

// 0x4086e0: spawn the lives display (f_408720, prio 0x20) for `parent`.
Task* f_4086e0(Task* parent) {
    Task d; task_desc(&d);
    d.prio = 0x20; d.parent = parent; d.fn = f_408720;
    TU8(&d, 0x20) = 0;
    return task_create(&d);
}

// 0x408720: lives display: 3 icons (surface 1) at y 432.
void f_408720(Task* t) {
    int32_t lives = TI32(t->parent, 0x2c);
    int32_t i = 0;
    for (int32_t x = 0xaa; x < 0x13a; x += 0x30, i++) {
        if (i < lives) gfx_blit(1, x, 0x1b0, 0x30, 0x30, 0, 0);
        else           gfx_blit(1, x, 0x1b0, 0x30, 0x30, 0x30, 0);
    }
}

// 0x408780: spawn a sliding banner (f_4087f0, prio 0x40) showing frame
// `frame` of surface 52.
void f_408780(int32_t frame) {
    Task d; task_desc(&d);
    d.prio = 0x40; d.fn = f_4087f0;
    TI32(&d, 0x20) = 0x280;
    TI32(&d, 0x24) = 0x2800;
    TI32(&d, 0x28) = MI32(GS + 0x34);
    TI32(&d, 0x2c) = frame;
    task_create(&d);
}

// 0x4087f0: banner (surface 52, 160x50): eases in from the right while
// GS+0x34 - 1 equals the creation value, otherwise slides out to the left.
void f_4087f0(Task* t) {
    if (TI32(t, 0x28) != MI32(GS + 0x34) - 1) {
        int32_t v = TI32(t, 0x24) - 0xa0;
        TI32(t, 0x24) = v;
        TI32(t, 0x20) = v >> 4;
        if (TI32(t, 0x20) > 0x2bc) t->dead = 1;
    } else {
        int32_t v = (int32_t)((uint32_t)TI32(t, 0x24) * 12u) / 13;
        TI32(t, 0x24) = v;
        TI32(t, 0x20) = v >> 4;
    }
    gfx_blit(0x34, TI32(t, 0x20), 0x186, 0xa0, 0x32, 0, TI32(t, 0x2c) * 0x32);
}

// 0x408880: create the meteor shower gimmick (f_4088d0, prio 0x10, f04 = 3,
// wait 120).
Task* f_408880() {
    Task d; task_desc(&d);
    d.prio = 0x10; d.f04 = 3; d.wait = 0x78; d.parent = nullptr; d.fn = f_4088d0;
    TI32(&d, 0x20) = 0;
    TI32(&d, 0x24) = 0;
    return task_create(&d);
}

// 0x4088d0: meteor shower: 1200 frames, calls f_40aa30 at frame 200 and
// spawns f_408920 with probability 1/3 each frame.
void f_4088d0(Task* t) {
    int32_t age = ++TI32(t, 0x20);
    if (age > 0x4b0) {
        MU8(GS + 0x29) = 0;
        t->dead = 1;
        return;
    }
    if (age == 0xc8) f_40aa30();
    if (crt_rand() % 3 == 0) f_408920();
}
