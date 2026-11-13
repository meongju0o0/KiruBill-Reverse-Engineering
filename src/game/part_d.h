// Prototypes for part_d (original functions 0x408920 .. 0x40aae0).
#pragma once

#include <cstdint>

struct Task;

void  f_408920();                                     // spawn a falling leaf particle
void  f_4089f0(Task* t);                              // leaf particle update
void  f_408ae0();                                     // create score counter task
void  f_408b20(Task* t);                              // score counter display
void  f_408c10(int32_t x, int32_t y);                 // spawn slash effect at world (x, y)
void  f_408d80(Task* t);                              // slash effect animation
void  f_408df0();                                     // start opening camera drop (pauses prio 8)
void  f_408e80(Task* t);                              // opening camera drop update
void  f_408f50(const char* file);                     // load encoded scrolling text file
void  f_4090c0(Task* t);                              // scrolling text manager
Task* f_409220(Task* parent, const char* str, int32_t delay); // create one text line task
void  f_4092c0(Task* t);                              // empty callback
void  f_4092d0(int32_t x, int32_t y, int32_t dir);    // spawn debris piece
void  f_409370(Task* t);                              // debris update
void  f_4093e0(int32_t x, int32_t y, int32_t dir);    // spawn shock wave
void  f_409470(Task* t);                              // shock wave update
Task* f_409540();                                     // create the moon boss
void  f_4095c0(Task* t);                              // moon form 1
void  f_409760(Task* t);                              // moon form 2: hover + attack objects
void  f_4099d0(Task* t);                              // moon form 2: bounce
void  f_409bc0(Task* t);                              // moon form 2: rise + lightning
void  f_409e70(Task* t);                              // moon form 3: bounce
void  f_40a0a0(Task* t);                              // moon form 3: high jump
void  f_40a2e0(Task* t);                              // moon form 3: lightning barrage
void  f_40a5f0(Task* t);                              // moon form 3: hover + rocks
void  f_40a850(Task* t);                              // moon death sequence
void  f_40aa30();                                     // create ending picture task
void  f_40aa80(Task* t);                              // ending picture slide
