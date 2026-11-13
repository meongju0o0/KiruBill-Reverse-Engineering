// prototypes for part_a (original functions 0x401000 .. 0x403a30)
#pragma once
#include <cstdint>
#include "../engine/task.h"

void  f_401000();                                   // create stage 1 background task
void  f_401040(Task* t);                            // stage 1 background (task)
void  f_401100();                                   // create stage 2 background task
void  f_401140(Task* t);                            // stage 2 background (task)
void  f_401210(Task* parent);                       // create HUD task
void  f_401250(Task* t);                            // HUD gauges (task)
Task* f_401340(int32_t gravity, int32_t floors, int32_t kind,
               int32_t w1, int32_t w2, int32_t w3); // spawn building
void  f_401530(Task* t);                            // building fall/land (task)
Task* f_4015c0(const int32_t* S, Task* parent);     // create building floor
void  f_401620(Task* t);                            // building floor (task)
void  f_4017e0(Task* parent);                       // create jump camera task
void  f_401850(Task* t);                            // camera: falling (task)
void  f_401920(Task* t);                            // camera: rising (task)
Task* f_4019d0(int32_t y);                          // camera scroll to y
void  f_401a70(Task* t);                            // camera scroll (task)
Task* f_401bc0();                                   // create screen shake
void  f_401c80(Task* t);                            // screen shake (task)
Task* f_401cc0();                                   // create player (+anim tables, HUD)
void  f_401fb0(Task* t);                            // player: standing
void  f_4021d0(Task* t);                            // player: standing (snap, no draw)
void  f_4022d0(Task* t);                            // player: move left
void  f_402390(Task* t);                            // player: move right
void  f_402450(Task* t);                            // player: guard (ground)
void  f_4024f0(Task* t);                            // player: guard recoil (ground)
void  f_402590(Task* t);                            // player: guard recoil (air)
void  f_402670(Task* t);                            // player: guard (air)
void  f_402770(Task* t);                            // player: ground slash
void  f_402820(Task* t);                            // player: jump crouch
void  f_4028f0(Task* t);                            // player: airborne
void  f_402a80(Task* t);                            // player: air slash
void  f_402b60(Task* t);                            // player: ground technique
void  f_402c30(Task* t);                            // player: air technique
void  f_402d40(Task* t);                            // player: stunned (ground)
void  f_402e00(Task* t);                            // player: stunned (air)
void  f_402ee0(Task* t);                            // player: uncontrolled fall
void  f_402f90(uint32_t anim);                      // animation tick (anim = record address)
void  f_402fd0(uint32_t anim);                      // animation rewind
void  f_402fe0();                                   // create stage banner
void  f_403040(Task* t);                            // banner slide in (task)
void  f_4030c0(Task* t);                            // banner slide out (task)
void  f_403110(int32_t x, int32_t y);               // spawn combo popup
void  f_4031c0(Task* t);                            // combo popup (task)
void  f_4032a0(int32_t x, int32_t y);               // create title left arrow
void  f_403320(Task* t);                            // title left arrow (task)
void  f_4033f0(int32_t x, int32_t y);               // create title right arrow
void  f_403470(Task* t);                            // title right arrow (task)
Task* f_403540();                                   // spawn 6-block tower
void  f_403610(Task* t);                            // tower fall/land (task)
Task* f_403690(const int32_t* S, Task* parent);     // create tower block
void  f_4036f0(Task* t);                            // tower block (task)
void  f_403870();                                   // create final prompt task
void  f_4038e0(Task* t);                            // final prompt (task)
void  f_403980(uint32_t colorref);                  // create screen flash
void  f_4039d0(Task* t);                            // screen flash (task)
