# Part b — 0x403a30 .. 0x405f70

All 30 real functions in the range are translated in `src/game/part_b.cpp`
(prototypes in `part_b.h`). angr's 1–15 byte padding "functions" were skipped.

## Functions

| addr | purpose |
|---|---|
| 403a30 | Spawn the 110-floor tower (falls with `f_401530`, prio 16, f04 2, wait 150; +0x20 y = camY-12982, +0x28 y<<6, +0x2c 5, +0x30 110 floors) and its 110 floor tasks. **Returns the tower Task\*** (callers in 0x40dd.. / 0x40e5.. use it). |
| 403b70 | Spawn one floor (`f_403bd0`, prio 16, parent = tower) from a 6-int descriptor. |
| 403bd0 | Tower floor: when the tower's hit flag (P+0x34 byte) is set and P+0x30-1 == this floor index, subtract damage P+0x3c from column P+0x38 hp; destroyed → P+0x30--, score `GS+0x10 += ftol(GS14*200*(P2c*1.5)*t34 + GS10)` capped at GS+0x3c, debris `f_4040b0(320, P20+idx*118)`, sound 7; otherwise score `+= GS14*(P2c*1.5)`. Draws surface 42 (480x120, row +0x24) at (80-camX, idx*118-camY+P20). Floor 0 hp 20 (roof row 0), floors 1-108 hp 1 (row 120), floor 109 hp 10 (row 240). |
| 403d60 | Spawn chimney smoke emitter (prio 2, f04 4, +0x20 = 1800 frames). |
| 403db0 | Emitter: 1/8 chance per frame (`crt_rand()%8==0`) to spawn a puff; draws surface 50 (70x22) at (366-camX, 184-camY). |
| 403e20 | Spawn smoke puff (prio 4): y 185, x 370+rand%40, vy -40-rand%60, vx rand%40-20 (rand order x, vy, vx), 4-bit fixed point. |
| 403ed0 | Puff motion; dies when y < camY-60; draws surface 51 (16x12). |
| 403f50 | Spawn the "終演" (game over) banner task (prio 32, wait 2; float y -100). |
| 403fc0 | Pauses all tasks with prio & 127 every frame, drops the banner with gravity 0.4, bounces at y=190 (vy *= -0.5, settles with vy=12 when |v|<3), counts frames in 0x41f670 once settled; at 60 frames calls `f_40b350()` (stores returned Task\* at +0x2c); when that task is dead sets `GS+0x28` byte = 1 (end of game). Draws surface 23 (220x100) at (210, y). |
| 4040b0 | Spawn debris burst emitter at (x,y) (prio 16). |
| 404100 | For 3 frames spawns 8 pieces at x-200..x+150 step 50: `f_404170(x+off-rand%20, y+rand%120, rand%3)` (rand order: type, y, x). |
| 404170 | Spawn debris piece: vx = (rand%128-64)*(type+3)/3, vy = (-32-rand%64)*(type+3)/3, size 50/30/16. |
| 404270 | Debris motion (gravity +4 per frame, 4-bit fixed), draws surface 13+type; dies when y>608 or x<-64 or x>704. |
| 404300 / 404350 | Player-vs-tower handler (tower from 403a30, floors 120 px). See below. |
| 404870 / 4048d0 | Only while `GS+0x29` (special mode) is set: auto hit each floor passing 150 px below the player (damage 5, column 1, combo +1, hit spark `f_403110(186, ...)`). |
| 4049a0 / 404a00 | Same as 404300/404350 for `f_401530` buildings (floors 118 px, bottom = y + floors*118 + 2, ground at 428). Descriptor +0x24/+0x2c are uninitialised stack in the original (unused). |
| 404f30 / 404fb0 | Special mode: 5 automatic hits (damage 1, column 1). When the 5 hits are used: kills itself, the player-side task (+0x20) and +0x24, `f_403980(0xffffff)` (white flash), then either clears byte +0x30 of `TTASK(A,0x34)` or `task_resume_mask(0x7f)`, and sets `TI32(MTASK(GS+0x44),0x30) = 0`. |
| 4050f0 / 405140 | Object carried by a parent task (B->parent): slash handling only (states 1,3,4,6,14). |
| 405440 / 405490 | Rock (iwa) vs. player: crush (ground y 429), slash (hp B+0x38, hit flag B+0x3c), guard/jump states like 404350. |
| 405a00 / 405a60 | Special mode auto hit on rocks. |
| 405b10 / 405b60 | Lightning bolt vs. player: touching it shocks the player (lives-1, invincible 180, state 13 → `f_402d40` or 14 → `f_402e00` when state was 3/10, sound 13); slashing it in states 1/4 (same column) gives combo, sound 1. |
| 405f20 | Spawn checker `f_405f70` (prio 16, +0x20 player, +0x24 obj). |

### Player-vs-falling-object handler (404350 / 404a00 / 405490)

`bottom` = lowest y of the falling object. Each frame:

* crush: when `bottom` == ground (429 / 428) and the player is not
  invincible (A+0x30 == 0), `A->wait > 0` and lives (A+0x2c) > 0:
  `task_resume_mask(8)`, lives--, invincible 180, state 12, `A->fn = f_4021d0`,
  max combo `GS+0x18 = max(GS+0x18, GS+0x14)`, combo `GS+0x14 = 0`,
  float gauge `A+0x38 += 30` (max 100), `GS+0x1c += 90` (max 300), sound 6;
  no lives left → `f_403f50()` (game over banner).
* by player state `A+0x50` (jump table):
  * 0/2: object above the player's head and falling (speed > 0), not
    invincible → `task_pause_mask(8)` (holds the object).
  * 1 (slash) / 4 (air slash, uses float `A+0x44*6 + A+0x24`): hit → object
    hit flag/damage (5 if invincible), column `= A+0x28`, push object up
    (`speed += -10*factor`), combo +1 (max 999), `GS+0x1c += min(30,(combo+1)/2)`
    (max 300), slash effect `f_408c10(Ax, Ay-30)`, spark
    `f_403110(Ax-70, Ay-camY-30)`, hit stop `task_set_wait_mask(0x7f,3)`,
    sound `crt_rand()%3`.
  * 3/6: landing on the object: `A+0x44 = speed/128`, `A+0x40 = bottom-48`.
  * 7 → 8 (guard): `A->fn = f_4024f0`, object speed `= -factor*32`, combo break, sound 5.
  * 8: `x += -512/factor`.
  * 10 → 9: `A->fn = f_402590`, `A+0x44 = 8.0`, `A+0x40 = bottom-50`, speed `= -factor*16`, sound 5.
  * 11: like 0/2 with +80.

## Globals / fields identified

* `GS+0x10` score, `GS+0x14` combo, `GS+0x18` max combo, `GS+0x1c` gauge
  (0..300), `GS+0x3c` score cap, `GS+0x28` (byte) game finished,
  `GS+0x29` (byte) special mode active.
* **`GS+0x40` is a `Task*`** (current falling building; read as
  `[GS+0x40]+0x20` by 0x40b1b7). The handlers clear it with a 32-bit store;
  this part sets both `MI32(GS+0x40)=0` and `MTASK(GS+0x40)=nullptr`.
  Other parts must use `MTASK(GS+0x40)`.
* **`GS+0x44` is a `Task*`** (player-related task; `MTASK(GS+0x44)`).
* `0x41f670` frame counter of the game over banner.
* Player task A: +0x20 x, +0x24 y, +0x28 column, +0x2c lives, +0x30
  invincibility, **+0x34 Task\*** (read by 404fb0 via `TTASK(A,0x34)`),
  +0x38 float gauge, +0x40 float y, +0x44 float vy, +0x50 state, +0x54 flag.
* Tasks are killed by writing the low byte of `dead` (`mov byte [t+0x18],1`).

## External functions called (assumed signatures)

```cpp
void  f_401530(Task* t);              // part a (task callback)
void  f_4021d0(Task* t);              // part a
void  f_4024f0(Task* t);              // part a
void  f_402590(Task* t);              // part a
void  f_402d40(Task* t);              // part a
void  f_402e00(Task* t);              // part a
void  f_403110(int32_t x, int32_t y); // part a
void  f_403980(int32_t colorref);     // part a
void  f_405f70(Task* t);              // part c
void  f_408c10(int32_t x, int32_t y); // part d
Task* f_40b350();                     // part d (return value is stored and its dead flag polled)
```

Functions of this part called from elsewhere take `Task*` for every
task-pointer argument (`f_404300`, `f_404870`, `f_4049a0`, `f_404f30`,
`f_4050f0`, `f_405440`, `f_405a00`, `f_405b10`, `f_405f20`) and return
`void`, except `Task* f_403a30()`.
