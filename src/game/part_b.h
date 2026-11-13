// prototypes for part_b (original 0x403a30 .. 0x405f70)
#pragma once

#include <cstdint>

struct Task;

// Only f_403a30 returns a value that callers use; the spawn helpers below
// return nothing (their callers ignore eax).
Task* f_403a30();                              // spawn the 110 floor tower (120 px floors, surface 42)
void  f_403b70(const int32_t* floor, Task* building); // spawn one floor (3 wall columns) of that tower
void  f_403bd0(Task* t);                       // tower floor: take player hits, score, draw
void  f_403d60();                              // spawn the chimney smoke emitter
void  f_403db0(Task* t);                       // smoke emitter (1800 frames) + chimney sprite (surface 50)
void  f_403e20();                              // spawn one smoke puff
void  f_403ed0(Task* t);                       // smoke puff movement / draw (surface 51)
void  f_403f50();                              // spawn the falling "終演" (game over) banner
void  f_403fc0(Task* t);                       // game over banner: drop, bounce, then end the game
void  f_4040b0(int32_t x, int32_t y);          // spawn a debris burst emitter
void  f_404100(Task* t);                       // debris burst emitter (3 frames x 8 pieces)
void  f_404170(int32_t x, int32_t y, int32_t type); // spawn one debris piece (type 0..2 = hahen_1..3)
void  f_404270(Task* t);                       // debris piece movement / draw
void  f_404300(Task* player, Task* building);  // spawn player-vs-falling-tower checker (120 px floors)
void  f_404350(Task* t);                       // player attack / crush handling vs. tower
void  f_404870(Task* player, Task* building);  // spawn auto-hit checker (only while GS+0x29 set)
void  f_4048d0(Task* t);                       // auto-hit checker vs. tower (120 px floors)
void  f_4049a0(Task* player, Task* building);  // spawn player-vs-falling-building checker (118 px floors)
void  f_404a00(Task* t);                       // player attack / crush handling vs. building
void  f_404f30(Task* player, Task* b2, Task* building); // spawn 5-hit auto checker (only while GS+0x29 set)
void  f_404fb0(Task* t);                       // 5-hit auto checker vs. building (118 px floors)
void  f_4050f0(Task* player, Task* obj);       // spawn player-vs-carried-object checker
void  f_405140(Task* t);                       // carried object vs. player attacks
void  f_405440(Task* player, Task* rock);      // spawn player-vs-rock checker
void  f_405490(Task* t);                       // rock vs. player (attack / crush)
void  f_405a00(Task* player, Task* rock);      // spawn rock auto-hit checker (only while GS+0x29 set)
void  f_405a60(Task* t);                       // rock auto-hit checker
void  f_405b10(Task* player, Task* bolt);      // spawn player-vs-lightning checker
void  f_405b60(Task* t);                       // lightning vs. player (shock / slash)
void  f_405f20(Task* player, Task* obj);       // spawn checker f_405f70 (part c)
