// prototypes for part_f (0x40d560 .. 0x410690): game scenes (record /
// playback for 第一幕, 第二幕, とことん), the result screen, replay and score
// file I/O and save/game.sav.
#pragma once

#include <cstdint>

struct Task;

// --- scenes (stored in 0x41f79c) -------------------------------------------
void f_40d560();            // 第一幕 (and its 第二幕 continuation) in play, records replay
void f_40df30();            // result screen (scene 8)
void f_40e2d0();            // 第一幕 replay playback
void f_40e750();            // set up a 第二幕 run (background, player, game state)
void f_40e840();            // 第二幕 in play (scene 3), records replay
void f_40ef60();            // 第二幕 replay playback
void f_40f2d0();            // set up a とことん run (scene 4 = f_40f420)
void f_40f420();            // とことん in play, records replay
void f_40f850();            // とことん replay playback

// --- replay files: save/replay%d.rep -----------------------------------------
void f_40fad0(int32_t n);   // write the current replay to save/replay<n>.rep
bool f_40fb90(int32_t n);   // read save/replay<n>.rep into the replay buffer

// --- score tables (10 entries of {int32 max_combo, int32 score}) -------------
int32_t f_40fe10();                 // insert result into save/score.sav (第一幕), returns rank 1..10 or 11
void    f_40ff30(int32_t* table);   // read save/score.sav (zeros if missing)
int32_t f_40ff80();                 // same for save/daini_score.sav (第二幕)
void    f_4100b0(int32_t* table);   // read save/daini_score.sav
int32_t f_410100();                 // same for save/endless_score.sav (とことん)
void    f_410230(int32_t* table);   // read save/endless_score.sav

// --- save/game.sav ---------------------------------------------------------
int32_t f_410280();                 // read unlock level (dword 0)
void    f_4102d0(int32_t level);    // write unlock level (truncates the file to 4 bytes)
int32_t f_410310();                 // read play time (dword 1)
void    f_410360(int32_t seconds);  // "write" play time (bug: file opened "rb", nothing is written)

// --- std::vector<RepEvent> helpers (vector object at 0x41f7b4) --------------
struct RepEvent {               // one recorded input: 8 bytes in memory and on disk
    int32_t frame;              // value of the frame counter 0x41f6ac
    uint8_t button;             // virtual button number (2,4,5,6,7,8,9)
    uint8_t pad[3];             // uninitialised stack bytes in the original
};
int32_t   f_4103a0();                                            // size()
void      f_4103c0(RepEvent* first, RepEvent* last);            // _Destroy (no-op)
void      f_4103d0(RepEvent* pos, uint32_t n, const RepEvent* v); // insert(pos, n, v)
RepEvent* f_410620(RepEvent* first, RepEvent* last, RepEvent* dst); // uninitialized_copy
void      f_410660(RepEvent* dst, uint32_t n, const RepEvent* v); // uninitialized_fill_n
