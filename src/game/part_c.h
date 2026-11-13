// Part C: original functions 0x405f70 .. 0x408920 (see part_c.cpp)
#pragma once
#include <cstdint>
#include "../engine/task.h"

void  f_405f70(Task* t);                      // player vs small object collision
void  f_406590(Task* pl, Task* obj);          // spawn f_4065e0
void  f_4065e0(Task* t);                      // player vs 48x48 object collision
void  f_406840(Task* pl, Task* bld);          // spawn f_406890
void  f_406890(Task* t);                      // player vs falling building
void  f_406dc0(Task* a, Task* b);             // spawn f_406e20 (if GS+0x29)
void  f_406e20(Task* t);                      // special attack vs building
void  f_406f00(Task* pl, Task* obj);          // spawn f_406f60
void  f_406f60(Task* t);                      // player vs big falling object
void  f_4074a0(Task* a, Task* b, Task* c);    // spawn f_407520 (if GS+0x29)
void  f_407520(Task* t);                      // special attack vs big object (5 hits)
void  f_407640(Task* pl, Task* obj);          // spawn f_407690
void  f_407690(Task* t);                      // player vs parent-relative object
void  f_407990(int32_t x, Task* parent);      // spawn destructible part f_407a20
void  f_407a20(Task* t);                      // destructible part
Task* f_407c50();                             // create rubble gimmick manager
void  f_407ce0(Task* t);                      // rubble manager
Task* f_407d40(Task* parent, int32_t col);    // rubble column spawner
void  f_407db0(Task* t);                      // rubble column spawner update
Task* f_407e30(Task* parent, int32_t col);    // create rubble piece
void  f_407ef0(Task* t);                      // rubble piece update
void  f_4080a0(int32_t x, int32_t y);         // spawn rubble break effect
void  f_4080f0(Task* t);                      // rubble break effect
void  f_408160(Task* a, Task* b);             // spawn stacking check
void  f_4081b0(Task* t);                      // rubble stacking check
Task* f_408230(int32_t x, int32_t y);         // spawn jump dust
void  f_4082c0(Task* t);                      // jump dust
Task* f_408330();                             // create thunderstorm gimmick
void  f_408390(Task* t);                      // thunderstorm
void  f_408440(int32_t lane);                 // spawn lightning bolt
void  f_4084f0(Task* t);                      // lightning bolt
void  f_4085f0(int32_t x, int32_t y, int32_t loop); // spawn cloud
void  f_408660(Task* t);                      // cloud
Task* f_4086e0(Task* parent);                 // spawn lives display
void  f_408720(Task* t);                      // lives display
void  f_408780(int32_t frame);                // spawn sliding banner
void  f_4087f0(Task* t);                      // sliding banner
Task* f_408880();                             // create meteor shower gimmick
void  f_4088d0(Task* t);                      // meteor shower
