// Prototypes for part_e: original functions in [0x40aae0, 0x40d560).
#pragma once

#include <cstdint>
#include "../engine/task.h"

// --- effects / objects -----------------------------------------------------
void  f_40aae0();                                   // spawn landing dust cloud (surface 3, 15 frames)
void  f_40ac20(Task* t);                            // dust cloud animation callback
void  f_40acb0(int32_t x, int32_t type, Task* parent); // spawn small "guard" enemy (surfaces 47/48) on a building
void  f_40ad50(Task* t);                            // guard enemy callback
void  f_40afa0(int32_t x, int32_t y, int32_t type); // spawn falling guard (knocked off)
void  f_40b030(Task* t);                            // falling guard callback
Task* f_40b0c0(int32_t x, int32_t y);               // spawn special move (waza) beam: two tasks + hit dispatch
void  f_40b2c0(Task* t);                            // waza beam half callback
Task* f_40b350();                                   // spawn palette fade-out task (32 frames)
void  f_40b390(Task* t);                            // palette fade-out callback
Task* f_40b450();                                   // spawn palette fade-in task (32 frames)
void  f_40b490(Task* t);                            // palette fade-in callback

// --- static object at 0x41f7b0 (replay header: seed + std::vector of 8-byte input records)
void  f_40b550();                                   // static constructor (+ atexit registration)
void  f_40b580();                                   // static destructor (frees the vector storage)

// --- startup / shutdown / main loop -----------------------------------------
void  f_40b5f0();                                   // load all resources, bind keys, create font, set first scene
void  f_40bf30();                                   // shutdown: save flags, kill all tasks
void  f_40bf50();                                   // one main loop tick (60 fps timing, frame skip, flip)

// --- scenes (stored in MFN(0x41f79c)) ---------------------------------------
void  f_40c050();                                   // scene 0: pre-title (one frame)
void  f_40c080();                                   // scene 1: title / mode select
void  f_40c420();                                   // spawn the page arrows for the replay list screens
void  f_40c4a0();                                   // scene 9: replay list, 第一幕 (replays 1..10)
void  f_40ca00();                                   // scene 10: replay list, 第二幕 (replays 11..20)
void  f_40cf10();                                   // scene 11: replay list, とことん (replays 21..30)
void  f_40d410();                                   // reset game state and start 第一幕 (scene 2 = f_40d560)
