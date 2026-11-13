// Part f: original functions 0x40d560 .. 0x410690.
//
//  * the in-game scenes of the three modes (第一幕 / 第二幕 / とことん), each
//    in a "record" flavour (normal play, every held button is appended to
//    the replay buffer) and a "playback" flavour (inputs are injected from
//    the replay buffer, the real keyboard only controls skip / fast forward);
//  * the result screen;
//  * replay files save/replay<N>.rep, the three high score tables and
//    save/game.sav.
//
// See docs/notes/part_f.md for the file formats and the meaning of the
// globals used here.
#include "game.h"

// (functions of other parts are declared in game.h)

// ---------------------------------------------------------------------------
// Globals used in this part (original addresses, see mem.h)
// ---------------------------------------------------------------------------
namespace {
constexpr uint32_t G_SCROLL_X     = 0x41f688; // GS+0x00
constexpr uint32_t G_VIEW_W       = 0x41f68c; // GS+0x04
constexpr uint32_t G_SCROLL_Y     = 0x41f690; // GS+0x08
constexpr uint32_t G_SCROLL_Y2    = 0x41f694; // GS+0x0c
constexpr uint32_t G_SCORE        = 0x41f698; // GS+0x10
constexpr uint32_t G_COMBO        = 0x41f69c; // GS+0x14
constexpr uint32_t G_MAX_COMBO    = 0x41f6a0; // GS+0x18
constexpr uint32_t G_6A4          = 0x41f6a4; // GS+0x1c
constexpr uint32_t G_STAGE_TIME   = 0x41f6a8; // GS+0x20 frames while the player is not frozen
constexpr uint32_t G_FRAME        = 0x41f6ac; // GS+0x24 frame counter (replay time base)
constexpr uint32_t G_END          = 0x41f6b0; // GS+0x28 byte: run is over
constexpr uint32_t G_EVENT_BUSY   = 0x41f6b1; // GS+0x29 byte: current stage event still running
constexpr uint32_t G_EVENT_TIMER  = 0x41f6b4; // GS+0x2c frames since the last event finished
constexpr uint32_t G_FAST         = 0x41f6b8; // GS+0x30 byte: fast forward (Tab during playback)
constexpr uint32_t G_BONUS_DONE   = 0x41f6b9; // GS+0x31 byte: 9,999,999 event already fired
constexpr uint32_t G_TO_ACT2      = 0x41f6ba; // GS+0x32 byte: switch 第一幕 -> 第二幕
constexpr uint32_t G_STEP         = 0x41f6bc; // GS+0x34 stage event index
constexpr uint32_t G_MODE         = 0x41f6c0; // GS+0x38 0/1 = 第一幕/第二幕 (set elsewhere), 2 = とことん
constexpr uint32_t G_SCORE_CAP    = 0x41f6c4; // GS+0x3c
constexpr uint32_t G_OBJECT       = 0x41f6c8; // GS+0x40 Task* current stage object
constexpr uint32_t G_PLAYER       = 0x41f6cc; // GS+0x44 Task* player
constexpr uint32_t G_COLOR_BG     = 0x41f6d4; // GS+0x4c pixel value of RGB(0xa0,0x20,0x20)
constexpr uint32_t G_UNLOCK       = 0x41f6dc; // 0 = 第一幕, 1 = +第二幕, 2 = +とことん
constexpr uint32_t G_PLAY_TIME    = 0x41f6e0; // accumulated play seconds
constexpr uint32_t G_RANK         = 0x41f6e4; // rank of the last run (11 = not ranked)
constexpr uint32_t G_SCENE_ID     = 0x41f798;
constexpr uint32_t G_SCENE_FN     = 0x41f79c;
constexpr uint32_t G_REPLAY_SEED  = 0x41f7b0; // srand() seed stored in the replay
// std::vector<RepEvent> at 0x41f7b4: +0 allocator, +4 _First, +8 _Last, +0xc _End
constexpr uint32_t G_VEC_FIRST    = 0x41f7b8;
constexpr uint32_t G_VEC_LAST     = 0x41f7bc;
constexpr uint32_t G_VEC_END      = 0x41f7c0;
// skip-replay state of the three playback scenes
constexpr uint32_t G_PB1_HOLD     = 0x41f85c; // 第一幕 playback: frames since start
constexpr uint32_t G_PB1_ARMED    = 0x41f860; // byte: decide held after 120 frames
constexpr uint32_t G_PB2_HOLD     = 0x41f864;
constexpr uint32_t G_PB2_ARMED    = 0x41f868;
constexpr uint32_t G_PB3_HOLD     = 0x41f86c;
constexpr uint32_t G_PB3_ARMED    = 0x41f870;

static_assert(sizeof(RepEvent) == 8, "RepEvent must stay 8 bytes (file format)");

inline RepEvent* vfirst() { return static_cast<RepEvent*>(MPTR(G_VEC_FIRST)); }
inline RepEvent* vlast()  { return static_cast<RepEvent*>(MPTR(G_VEC_LAST)); }
inline RepEvent* vend()   { return static_cast<RepEvent*>(MPTR(G_VEC_END)); }
inline void set_vfirst(RepEvent* p) { MPTR(G_VEC_FIRST) = p; }
inline void set_vlast(RepEvent* p)  { MPTR(G_VEC_LAST) = p; }
inline void set_vend(RepEvent* p)   { MPTR(G_VEC_END) = p; }

// (ptr difference) >> 3 as the original computes vector sizes (signed sar)
inline int32_t vdiff(const RepEvent* a, const RepEvent* b) { return (int32_t)(a - b); }

// Restore the palette that was loaded from image/game.pal (0x411760 + vtbl+0x1c).
void restore_palette() {
    uint32_t pal[256];
    gfx_get_palette(pal);
    gfx_set_palette(pal);
}

// Append every held game button to the replay buffer, in the original
// order (6 up, 7 down, 5 attack, 8 X, 2 left, 4 right, 9 decide).  The
// original inlines some of these push_backs; all are vector::insert(end,1,v).
void record_inputs() {
    static const uint8_t kOrder[7] = {6, 7, 5, 8, 2, 4, 9};
    for (uint8_t b : kOrder) {
        if (in_held(b)) {
            RepEvent v;
            v.frame = MI32(G_FRAME);
            v.button = b;
            v.pad[0] = v.pad[1] = v.pad[2] = 0;
            f_4103d0(vlast(), 1, &v);
        }
    }
}

// Playback: forget the real inputs and inject every recorded event whose
// frame equals the current frame counter, popping it from the front.
void inject_inputs() {
    in_clear();
    for (;;) {
        RepEvent* first = vfirst();
        if (!first) break;
        if ((uint32_t)vdiff(vlast(), first) < 1u) break;
        if (first->frame != MI32(G_FRAME)) break;
        in_force(first->button);
        // erase(begin()): shift the remaining elements down by one
        RepEvent* last = vlast();
        RepEvent* dst = vfirst();
        for (RepEvent* src = dst + 1; src != last; ++src, ++dst) *dst = *src;
        set_vlast(last - 1);
    }
}

// Random building used by とことん (and the act 1 table entry 0):
// f_401340(floors_group, hardness, rand%3, 6, 3, 1).
Task* spawn_random_building() {
    int32_t lvl = MI32(G_STAGE_TIME) / 600;
    int32_t a = crt_rand() % (lvl + 1) + lvl + 1;
    if (a > 100) a = 100;
    int32_t b = crt_rand() % 3;
    int32_t c = MI32(G_STAGE_TIME) / 1500 + 1;
    return f_401340(c, a, b, 6, 3, 1);
}

// Switch 第一幕 -> 第二幕 inside the act 1 scenes (record and playback).
void start_act2_music_and_stage() {
    mus_stop(0);
    mus_load(STR_41e5a8, 0);                 // "data/000.dat" (MIDI)
    mus_set_loop(0, 0x7b00, 0x3a500);
    mus_play(0);
    task_kill_all();
    task_run_all();
    f_40e750();
    f_40b450();
}

// 8-digit 64x64 number (combo.bmp, surface 21) right aligned at x=384.
void draw_big_number(int32_t value, int32_t y) {
    int32_t div = 1;
    int32_t x = 384;
    do {
        int32_t q = value / div;
        int32_t d = q % 10;
        if (q > 0 || x == 384) gfx_blit(21, x, y, 64, 64, d * 64, 0);
        div *= 10;
        x -= 48;
    } while (x > 0);
}

// Stage events of 第一幕 (also used after the switch to 第二幕).  `ev` is
// G_STEP + 1; `playback` selects the playback variant of the table.
// Returns the new stage object (nullptr for events without one; the
// original stored an unrelated stack value there, which is never read).
Task* act1_event(uint32_t ev, Task* player, bool playback) {
    Task* obj = nullptr;
    switch (ev) {
    case 0: // とことん style endless building
        obj = spawn_random_building();
        f_406840(MTASK(G_PLAYER), obj);
        MI32(G_STEP) -= 1;
        break;
    case 1:
        obj = f_403540();
        f_404300(MTASK(G_PLAYER), obj);
        f_408780(0);
        break;
    case 2:
        obj = f_403a30();
        f_4049a0(MTASK(G_PLAYER), obj);
        f_408780(1);
        break;
    case 3:
        obj = f_408330();
        f_408780(2);
        break;
    case 4:
        obj = f_407c50();
        f_408780(3);
        break;
    case 5:
        f_40b350();
        f_408f50(STR_41e620);                // "data/023.dat"
        MTASK(G_PLAYER)->fn = f_402ee0;
        break;
    case 6:
        MI32(G_COMBO) = 0;
        player->fn = f_401fb0;
        TI32(MTASK(G_PLAYER), 0x2c) = 3;
        if (!playback) MI32(G_6A4) = 0;      // the playback copy lacks this store
        f_408df0();
        f_40b450();
        obj = f_409540();
        f_406f00(MTASK(G_PLAYER), obj);
        f_408780(4);
        break;
    case 7:
        f_402fe0();
        MTASK(G_PLAYER)->fn = f_402ee0;
        f_408880();
        break;
    case 8:
        task_pause_mask(0x7f);
        f_40b350();
        f_408f50(STR_41e610);                // "data/024.dat"
        break;
    case 9:
        if (!playback && MI32(G_UNLOCK) <= 1) MI32(G_UNLOCK) = 2;
        MU8(G_END) = 1;
        break;
    default:
        break;
    }
    return obj;
}

// Stage events of the dedicated 第二幕 scenes (index = G_STEP).
Task* act2_event(uint32_t ev, Task* player, bool playback) {
    Task* obj = nullptr;
    switch (ev) {
    case 0:
        obj = f_403540();
        f_404300(MTASK(G_PLAYER), obj);
        f_408780(0);
        break;
    case 1:
        obj = f_403a30();
        f_4049a0(MTASK(G_PLAYER), obj);
        f_408780(1);
        break;
    case 2:
        obj = f_408330();
        f_408780(2);
        break;
    case 3:
        obj = f_407c50();
        f_408780(3);
        break;
    case 4:
        f_40b350();
        f_408f50(STR_41e620);                // "data/023.dat"
        MTASK(G_PLAYER)->fn = f_402ee0;
        break;
    case 5:
        MI32(G_COMBO) = 0;
        player->fn = f_401fb0;
        TI32(MTASK(G_PLAYER), 0x2c) = 3;
        if (!playback) MI32(G_6A4) = 0;      // the playback copy lacks this store
        f_408df0();
        f_40b450();
        obj = f_409540();
        f_406f00(MTASK(G_PLAYER), obj);
        f_408780(4);
        break;
    case 6:
        f_402fe0();
        MTASK(G_PLAYER)->fn = f_402ee0;
        f_408880();
        break;
    case 7:
        task_pause_mask(0x7f);
        f_40b350();
        f_408f50(STR_41e610);                // "data/024.dat"
        break;
    case 8:
        if (!playback && MI32(G_UNLOCK) <= 1) MI32(G_UNLOCK) = 2;
        MU8(G_END) = 1;
        break;
    default:
        break;
    }
    return obj;
}

// The "wait 45 frames after the previous event, then start the next one"
// logic shared by the act scenes.  Returns true if an event was started.
bool event_timer_elapsed() {
    if (MU8(G_EVENT_BUSY)) return false;
    MI32(G_EVENT_TIMER) += 1;
    if (MI32(G_EVENT_TIMER) <= 45) return false;
    MU8(G_EVENT_BUSY) = 1;
    MI32(G_EVENT_TIMER) = 0;
    return true;
}

// Common end of a frame in the scenes: frame counter and stage timer.
void advance_frame_counters() {
    MI32(G_FRAME) += 1;
    if (TI32(MTASK(G_PLAYER), 0x30) == 0) MI32(G_STAGE_TIME) += 1;
}

// Skip handling of the playback scenes: holding decide for more than 120
// frames arms the skip; releasing it afterwards ends the replay.  Tab
// (button 11) runs the scene 4x per frame (main loop, 0x40bf50).
void playback_skip_keys(uint32_t hold, uint32_t armed) {
    MI32(hold) += 1;
    if (in_held(9) && MI32(hold) > 120) MU8(armed) = 1;
    if (!in_held(9) && MU8(armed)) MU8(G_END) = 1;
    MU8(G_FAST) = in_held(11) != 0;
}

// Value left in ECX by 0x410280 (its `pop ecx` restores the dword it read
// from save/game.sav).  0x410310 is called right after it with no argument
// and uses that register value as its uninitialised local, which is what it
// returns when save/game.sav is only 4 bytes long (as written by 0x4102d0).
int32_t s_ecx_after_410280 = 0;

void replay_name(char* buf, int32_t n) {
    // wsprintfA(buf, "save/replay%d.rep", n) into a 20 byte stack buffer
    std::snprintf(buf, 20, STR_41e5fc, (int)n);
}
} // namespace

// ===========================================================================
// 0x40d560: 第一幕 in play (recording).  Also continues into 第二幕 when
// G_TO_ACT2 is raised (f_40e750 resets the stage table).
// ===========================================================================
void f_40d560() {
    in_update();
    record_inputs();
    task_run_all();
    advance_frame_counters();
    Task* player = MTASK(G_PLAYER);

    if (MU8(G_END)) {
        MI32(G_SCENE_ID) = 8;
        MFN(G_SCENE_FN) = f_40df30;
        task_kill_all();
        task_run_all();
        mus_stop(0);
        MI32(G_RANK) = f_40fe10();
        f_40b450();
        int32_t t = MI32(G_PLAY_TIME) + MI32(G_FRAME) / 60;
        MI32(G_PLAY_TIME) = t;
        if (t > 14400) {
            if (MI32(G_UNLOCK) == 0) MI32(G_UNLOCK) = 1;
        } else if (t > 28800) {              // unreachable in the original as well
            if (MI32(G_UNLOCK) <= 1) MI32(G_UNLOCK) = 2;
        }
        return;
    }
    if (MI32(G_SCORE) == 9999999 && !MU8(G_BONUS_DONE)) {
        MU8(G_BONUS_DONE) = 1;
        f_403870();
        return;
    }
    if (MU8(G_TO_ACT2)) {
        if (MI32(G_UNLOCK) == 0) MI32(G_UNLOCK) = 1;
        MU8(G_TO_ACT2) = 0;
        start_act2_music_and_stage();
        return;
    }
    if (event_timer_elapsed()) {
        Task* obj = act1_event((uint32_t)(MI32(G_STEP) + 1), player, false);
        MTASK(G_OBJECT) = obj;
        MI32(G_STEP) += 1;
    }
    if (in_pressed(10)) plat_quit();
}

// ===========================================================================
// 0x40df30: result screen (scene 8).  Decide -> title, Esc -> quit.
// ===========================================================================
void f_40df30() {
    in_update();
    task_run_all();
    if (in_pressed(9)) {
        MI32(G_SCENE_ID) = 1;
        MFN(G_SCENE_FN) = f_40c080;
        int32_t saved = f_410280();
        if (MI32(G_UNLOCK) > saved) f_4102d0(MI32(G_UNLOCK));
        task_kill_all();
        task_run_all();
        restore_palette();
    }
    if (in_pressed(10)) plat_quit();

    gfx_fill(0, 0, 640, 480, MU32(G_COLOR_BG));
    gfx_blit(26, 220, 20, 200, 100, 0, 0);    // 成果 (seika.bmp)
    gfx_blit(24, 448, 142, 200, 100, 0, 0);   // 連斬 (renzan.bmp)
    draw_big_number(MI32(G_MAX_COMBO), 150);
    gfx_blit(25, 448, 242, 100, 100, 0, 0);   // 点 (ten.bmp)
    draw_big_number(MI32(G_SCORE), 250);

    if (MI32(G_RANK) < 11) {
        gfx_blit(0x22, 600, 10, 32, 32, 0, 0); // 位 (i.bmp)
        int32_t div = 1;
        int32_t x = 580;
        do {
            int32_t q = MI32(G_RANK) / div;
            int32_t d = q % 10;
            if (q > 0 || x == 580) gfx_blit(31, x, 12, 20, 24, d * 20, 0);
            div *= 10;
            x -= 20;
        } while (x > 540);
    }

    if (MI32(G_UNLOCK) > f_410280()) {
        // GetDC on the back buffer, transparent background, game font,
        // black text.  Every TextOutA is guarded by the draw-enabled flag.
        if (MI32(G_UNLOCK) == 1) {
            if (gfx_draw_enabled())             // "タイトル画面で"
                gfx_text(222, 350, STR_41e678, (int)std::strlen(STR_41e678), 0);
            if (gfx_draw_enabled())             // "第二幕が選べるようになりました！"
                gfx_text(96, 400, STR_41e654, (int)std::strlen(STR_41e654), 0);
        } else {
            if (gfx_draw_enabled())
                gfx_text(222, 350, STR_41e678, (int)std::strlen(STR_41e678), 0);
            if (gfx_draw_enabled())             // "とことんが選べるようになりました！"
                gfx_text(82, 400, STR_41e630, (int)std::strlen(STR_41e630), 0);
        }
    }
}

// ===========================================================================
// 0x40e2d0: 第一幕 replay playback.  Ends -> 第一幕 replay menu (scene 9).
// ===========================================================================
void f_40e2d0() {
    in_update();
    playback_skip_keys(G_PB1_HOLD, G_PB1_ARMED);
    if (in_pressed(10)) {
        plat_quit();
        return;
    }
    inject_inputs();
    task_run_all();
    advance_frame_counters();
    Task* player = MTASK(G_PLAYER);

    if (!MU8(G_END) && TI32(player, 0x2c) > 0) {
        if (MI32(G_SCORE) == 9999999 && !MU8(G_BONUS_DONE)) {
            MU8(G_BONUS_DONE) = 1;
            f_403870();
            return;
        }
        if (MU8(G_TO_ACT2)) {
            MU8(G_TO_ACT2) = 0;
            start_act2_music_and_stage();
            return;
        }
        if (event_timer_elapsed()) {
            Task* obj = act1_event((uint32_t)(MI32(G_STEP) + 1), player, true);
            MTASK(G_OBJECT) = obj;
            MI32(G_STEP) += 1;
        }
        gfx_blit(35, 380, 20, 250, 60, 0, 0);  // "DEMO" (demo.bmp)
        return;
    }

    task_kill_all();
    task_run_all();
    mus_stop(0);
    MU8(G_PB1_ARMED) = 0;
    MI32(G_PB1_HOLD) = 0;
    f_40c420();
    MI32(G_SCENE_ID) = 9;
    MFN(G_SCENE_FN) = f_40c4a0;
    restore_palette();
}

// ===========================================================================
// 0x40e750: set up a 第二幕 run.
// ===========================================================================
void f_40e750() {
    f_401000();
    f_4085f0(480, 100, 1);
    f_4085f0(240, -900, 1);
    f_4085f0(120, -700, 1);
    f_4085f0(640, -500, 1);
    f_4085f0(-100, -300, 1);
    f_4085f0(300, -100, 1);
    MTASK(G_PLAYER) = f_401cc0();
    f_408ae0();
    MI32(G_VIEW_W) = 640;
    MI32(G_SCROLL_X) = 0;
    MI32(G_SCROLL_Y) = 0;
    MI32(G_SCROLL_Y2) = 480;
    f_408df0();
    MI32(G_STAGE_TIME) = 0;
    MU8(G_END) = 0;
    MU8(G_EVENT_BUSY) = 0;
    MTASK(G_OBJECT) = nullptr;
    MI32(G_OBJECT) = 0;
    MI32(G_6A4) = 0;
    MU8(G_FAST) = 0;
    MI32(G_STEP) = 0;
    MI32(G_SCORE_CAP) = 99999999;
    MI32(G_EVENT_TIMER) = 45;
    MI32(G_MODE) = 1;
}

// ===========================================================================
// 0x40e840: 第二幕 in play (recording), scene 3.
// ===========================================================================
void f_40e840() {
    in_update();
    record_inputs();
    task_run_all();
    advance_frame_counters();
    Task* player = MTASK(G_PLAYER);

    if (MU8(G_END)) {
        MI32(G_SCENE_ID) = 8;
        MFN(G_SCENE_FN) = f_40df30;
        task_kill_all();
        task_run_all();
        mus_stop(0);
        MI32(G_RANK) = f_40ff80();
        f_40b450();
        int32_t t = MI32(G_PLAY_TIME) + MI32(G_FRAME) / 60;
        MI32(G_PLAY_TIME) = t;
        if (t > 28800 && MI32(G_UNLOCK) <= 1) MI32(G_UNLOCK) = 2;
        return;
    }
    if (event_timer_elapsed()) {
        Task* obj = act2_event((uint32_t)MI32(G_STEP), player, false);
        MTASK(G_OBJECT) = obj;
        MI32(G_STEP) += 1;
    }
    if (in_pressed(10)) plat_quit();
}

// ===========================================================================
// 0x40ef60: 第二幕 replay playback.  Ends -> 第二幕 replay menu (scene 10).
// ===========================================================================
void f_40ef60() {
    in_update();
    playback_skip_keys(G_PB2_HOLD, G_PB2_ARMED);
    if (in_pressed(10)) {
        plat_quit();
        return;
    }
    inject_inputs();
    task_run_all();
    advance_frame_counters();
    Task* player = MTASK(G_PLAYER);

    if (!MU8(G_END) && TI32(player, 0x2c) > 0) {
        if (event_timer_elapsed()) {
            Task* obj = act2_event((uint32_t)MI32(G_STEP), player, true);
            MTASK(G_OBJECT) = obj;
            MI32(G_STEP) += 1;
        }
        gfx_blit(35, 380, 20, 250, 60, 0, 0);  // "DEMO"
        return;
    }

    task_kill_all();
    task_run_all();
    mus_stop(0);
    MU8(G_PB2_ARMED) = 0;
    MI32(G_PB2_HOLD) = 0;
    f_40c420();
    MI32(G_SCENE_ID) = 10;
    MFN(G_SCENE_FN) = f_40ca00;
    restore_palette();
}

// ===========================================================================
// 0x40f2d0: set up a とことん run (scene 4 = f_40f420).
// ===========================================================================
void f_40f2d0() {
    MI32(G_SCENE_ID) = 4;
    MFN(G_SCENE_FN) = f_40f420;
    f_401100();
    f_4085f0(480, 5, 1);
    f_4085f0(240, -900, 1);
    f_4085f0(120, -700, 1);
    f_4085f0(640, -500, 1);
    f_4085f0(-100, -300, 1);
    f_4085f0(300, -100, 1);
    MTASK(G_PLAYER) = f_401cc0();
    f_408ae0();
    MI32(G_VIEW_W) = 640;
    MI32(G_SCROLL_X) = 0;
    MI32(G_SCROLL_Y) = 0;
    MI32(G_SCROLL_Y2) = 480;
    f_408df0();
    MI32(G_SCORE) = 0;
    MI32(G_SCORE_CAP) = 99999999;
    MI32(G_COMBO) = 0;
    MI32(G_MAX_COMBO) = 0;
    MI32(G_STAGE_TIME) = 0;
    MI32(G_FRAME) = 0;
    MU8(G_END) = 0;
    MU8(G_EVENT_BUSY) = 0;
    MI32(G_EVENT_TIMER) = 0;
    MI32(G_6A4) = 0;
    set_vlast(vfirst());                      // replay.clear() (erase(begin, end))
    MU32(G_REPLAY_SEED) = 0;
    MU8(G_FAST) = 0;
    MU8(G_BONUS_DONE) = 0;
    MU8(G_TO_ACT2) = 0;
    MI32(G_STEP) = -1;
    MI32(G_MODE) = 2;
}

// ===========================================================================
// 0x40f420: とことん in play (recording).  A new random building appears 45
// frames after the previous one is finished.
// ===========================================================================
void f_40f420() {
    in_update();
    record_inputs();
    task_run_all();
    advance_frame_counters();

    if (MU8(G_END)) {
        MI32(G_SCENE_ID) = 8;
        MFN(G_SCENE_FN) = f_40df30;
        task_kill_all();
        task_run_all();
        mus_stop(0);
        MI32(G_RANK) = f_410100();
        f_40b450();
        return;
    }
    if (event_timer_elapsed()) {
        Task* obj = spawn_random_building();
        f_406840(MTASK(G_PLAYER), obj);
        MTASK(G_OBJECT) = obj;
    }
    if (in_pressed(10)) plat_quit();
}

// ===========================================================================
// 0x40f850: とことん replay playback.  Ends -> とことん replay menu (scene 11).
// ===========================================================================
void f_40f850() {
    in_update();
    playback_skip_keys(G_PB3_HOLD, G_PB3_ARMED);
    if (in_pressed(10)) {
        plat_quit();
        return;
    }
    inject_inputs();
    task_run_all();
    advance_frame_counters();
    Task* player = MTASK(G_PLAYER);

    if (!MU8(G_END) && TI32(player, 0x2c) > 0) {
        if (event_timer_elapsed()) {
            Task* obj = spawn_random_building();
            f_406840(MTASK(G_PLAYER), obj);
            MTASK(G_OBJECT) = obj;
        }
        gfx_blit(35, 380, 20, 250, 60, 0, 0);  // "DEMO"
        return;
    }

    task_kill_all();
    task_run_all();
    mus_stop(0);
    MU8(G_PB3_ARMED) = 0;
    MI32(G_PB3_HOLD) = 0;
    f_40c420();
    MI32(G_SCENE_ID) = 11;
    MFN(G_SCENE_FN) = f_40cf10;
    restore_palette();
}

// ===========================================================================
// 0x40fad0: save the replay buffer as save/replay<n>.rep.
// Format: u32 seed, i32 count, count * {i32 frame, u8 button, u8 pad[3]}.
// ===========================================================================
void f_40fad0(int32_t n) {
    char name[20];
    replay_name(name, n);
    FILE* f = std::fopen(name, "wb");
    if (!f) return;
    std::fwrite(&MU32(G_REPLAY_SEED), 4, 1, f);
    int32_t count = vfirst() ? vdiff(vlast(), vfirst()) : 0;
    std::fwrite(&count, 4, 1, f);
    for (RepEvent* p = vfirst(); p != vlast(); ++p) std::fwrite(p, 8, 1, f);
    std::fflush(f);
    std::fclose(f);
}

// ===========================================================================
// 0x40fb90: load save/replay<n>.rep into the replay buffer (and its seed).
// ===========================================================================
bool f_40fb90(int32_t n) {
    char name[20];
    replay_name(name, n);
    FILE* f = std::fopen(name, "rb");
    if (!f) return false;
    set_vlast(vfirst());                      // clear()
    std::fread(&MU32(G_REPLAY_SEED), 4, 1, f);
    int32_t count = 0;
    std::fread(&count, 4, 1, f);
    RepEvent e;                               // one stack slot reused by every iteration
    std::memset(&e, 0, sizeof e);
    for (int32_t i = 0; i < count; i++) {
        std::fread(&e, 8, 1, f);
        f_4103d0(vlast(), 1, &e);             // push_back
    }
    std::fclose(f);
    return true;
}

// ===========================================================================
// Score tables: 10 entries {int32 max_combo, int32 score}, sorted by score,
// best first.  Inserting a result moves the lower entries and their replay
// files down by one rank; the run's replay is saved under the new rank.
// ===========================================================================
namespace {
int32_t insert_score(int32_t* tab, const char* score_file, const char* last_replay,
                     int32_t replay_base) {
    int32_t idx = 0;
    for (; idx < 10; idx++)
        if (MI32(G_SCORE) >= tab[2 * idx + 1]) break;
    if (idx < 10) {
        std::remove(last_replay);             // DeleteFileA("save/replay<base+10>.rep")
        for (int32_t k = 9; k > idx; k--) {
            tab[2 * k + 1] = tab[2 * (k - 1) + 1];
            tab[2 * k] = tab[2 * (k - 1)];
            char from[20], to[20];
            replay_name(from, replay_base + k);
            replay_name(to, replay_base + k + 1);
            FILE* f = std::fopen(from, "rb");
            if (f) {
                std::fclose(f);
                std::rename(from, to);
            }
        }
        tab[2 * idx + 1] = MI32(G_SCORE);
        tab[2 * idx] = MI32(G_MAX_COMBO);
    }
    FILE* f = std::fopen(score_file, "wb");  // original mode "wbc" (MSVC commit flag)
    if (!f) return 11;
    std::fwrite(tab, 8, 10, f);
    std::fflush(f);
    std::fclose(f);
    idx += 1;
    if (idx < 11) f_40fad0(idx + replay_base);
    return idx;
}

void load_score_table(int32_t* tab, const char* file) {
    FILE* f = std::fopen(file, "rb");
    if (!f) {
        for (int i = 0; i < 10; i++) { tab[2 * i] = 0; tab[2 * i + 1] = 0; }
        return;
    }
    std::fread(tab, 8, 10, f);
    std::fclose(f);
}
} // namespace

// 0x40fe10: 第一幕 ranking (save/score.sav, replays 1..10)
int32_t f_40fe10() {
    int32_t tab[20] = {0};
    f_40ff30(tab);
    return insert_score(tab, STR_41e68c, STR_41e6a0, 0);
}

// 0x40ff30: read save/score.sav
void f_40ff30(int32_t* table) { load_score_table(table, STR_41e68c); }

// 0x40ff80: 第二幕 ranking (save/daini_score.sav, replays 11..20)
int32_t f_40ff80() {
    int32_t tab[20] = {0};
    f_4100b0(tab);
    return insert_score(tab, STR_41e5e0, STR_41e6b4, 10);
}

// 0x4100b0: read save/daini_score.sav
void f_4100b0(int32_t* table) { load_score_table(table, STR_41e5e0); }

// 0x410100: とことん ranking (save/endless_score.sav, replays 21..30)
int32_t f_410100() {
    int32_t tab[20] = {0};
    f_410230(tab);
    return insert_score(tab, STR_41e5c8, STR_41e6c8, 20);
}

// 0x410230: read save/endless_score.sav
void f_410230(int32_t* table) { load_score_table(table, STR_41e5c8); }

// ===========================================================================
// save/game.sav: dword 0 = unlock level, dword 1 = play time (never written)
// ===========================================================================
// 0x410280
int32_t f_410280() {
    FILE* f = std::fopen(STR_41e6dc, "rb");
    if (!f) return 0;
    int32_t v = 0;
    std::fread(&v, 4, 1, f);
    std::fclose(f);
    s_ecx_after_410280 = v;
    return v;
}

// 0x4102d0: rewrites the file with only the unlock level (4 bytes).
void f_4102d0(int32_t level) {
    FILE* f = std::fopen(STR_41e6dc, "wb");
    if (!f) return;
    std::fwrite(&level, 4, 1, f);
    std::fclose(f);
}

// 0x410310: play time at offset 4.  When the file has only 4 bytes the read
// fails and the original returns its uninitialised local, which holds the
// unlock level left in ECX by the preceding f_410280() call (see above).
int32_t f_410310() {
    FILE* f = std::fopen(STR_41e6dc, "rb");
    if (!f) return 0;
    std::fseek(f, 4, SEEK_SET);
    int32_t v = s_ecx_after_410280;
    std::fread(&v, 4, 1, f);
    std::fclose(f);
    return v;
}

// 0x410360: meant to store the play time at offset 4, but the original opens
// the file with "rb", so its fwrite fails and nothing is written.  The port
// keeps that behaviour (open/seek/close without writing).
void f_410360(int32_t seconds) {
    (void)seconds;
    FILE* f = std::fopen(STR_41e6dc, "rb");
    if (!f) return;
    std::fseek(f, 4, SEEK_SET);
    // original: fwrite(&seconds, 4, 1, f) on a read-only stream -> fails
    std::fclose(f);
}

// ===========================================================================
// std::vector<RepEvent> members (this = 0x41f7b4)
// ===========================================================================
// 0x4103a0: size()
int32_t f_4103a0() {
    if (!vfirst()) return 0;
    return vdiff(vlast(), vfirst());
}

// 0x4103c0: _Destroy(first, last) - trivial for POD elements
void f_4103c0(RepEvent*, RepEvent*) {}

// 0x410620: uninitialized_copy(first, last, dst) -> dst + (last - first)
RepEvent* f_410620(RepEvent* first, RepEvent* last, RepEvent* dst) {
    for (; first != last; ++first, ++dst)
        if (dst) *dst = *first;
    return dst;
}

// 0x410660: uninitialized_fill_n(dst, n, v)
void f_410660(RepEvent* dst, uint32_t n, const RepEvent* v) {
    for (; n > 0; --n, ++dst)
        if (dst) *dst = *v;
}

// 0x4103d0: insert(pos, n, v) (VC6 <vector>)
void f_4103d0(RepEvent* pos, uint32_t n, const RepEvent* v) {
    RepEvent val = *v;
    RepEvent* first = vfirst();
    RepEvent* last = vlast();
    RepEvent* end = vend();
    if ((uint32_t)vdiff(end, last) < n) {
        // reallocate: capacity = size + max(size, n)
        uint32_t grow = n;
        if (first) {
            uint32_t sz = (uint32_t)vdiff(last, first);
            if (n < sz) grow = sz;
        }
        int32_t size = first ? vdiff(last, first) : 0;
        int32_t cap = size + (int32_t)grow;
        if (cap < 0) cap = 0;
        RepEvent* nb = static_cast<RepEvent*>(::operator new((size_t)cap * 8 + 8)); // 0x414368
        RepEvent* p = f_410620(first, pos, nb);
        f_410660(p, n, &val);
        f_410620(pos, last, p + n);
        f_4103c0(first, last);
        ::operator delete(first);             // 0x413efe (matches f_40b550 in part e)
        set_vend(nb + cap);
        set_vlast(nb + (size + (int32_t)n));  // size is 0 when there was no buffer
        set_vfirst(nb);
    } else if ((uint32_t)vdiff(last, pos) < n) {
        f_410620(pos, last, pos + n);
        f_410660(last, n - (uint32_t)vdiff(last, pos), &val);
        for (RepEvent* q = pos; q != last; ++q) *q = val;
        set_vlast(last + n);
    } else if (n > 0) {
        f_410620(last - n, last, last);
        // copy_backward(pos, last - n, last)
        RepEvent* src = last - n;
        RepEvent* dst = last;
        while (src != pos) *--dst = *--src;
        for (RepEvent* q = pos; q != pos + n; ++q) *q = val;
        set_vlast(last + n);
    }
}
