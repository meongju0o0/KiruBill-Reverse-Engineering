# Part A — 0x401000 .. 0x403a30

Translated in `src/game/part_a.cpp`, prototypes in `src/game/part_a.h`.

## Functions

| addr | kind | purpose |
|---|---|---|
| 401000 | create | stage 1 background task (prio 1, fn 401040) |
| 401040 | task | stage 1 background: surface 11 (data/002.dat) scrolled by camera y (GS+8); sky above filled with pixel GS+0x50; when camy < -1440, 1/10 chance per frame to spawn `f_4085f0(rand%640, camy-120, 0)` |
| 401100 | create | stage 2/endless background task (prio 2, fn 401140) |
| 401140 | task | background surface 10 (bg.bmp 640x1440, sy = camy+960); sky pixel GS+0x48; same random particle when camy <= -1440 |
| 401210 | create | HUD task (prio 0x20), parent = player |
| 401250 | task | HUD: technique gauge `GS+0x1c` (0..300 → 0..50 px, blinks wazabar/wazabar2 when full, blink counter 0x41f460 / toggle byte 0x41e050), defense gauge = player float +0x38 (0..100 px, bougyobar), label bougyo.bmp |
| 401340 | create | building: `(gravity, floors, kind, w1, w2, w3)`; roof + `floors` random floors + ground; floor column type 1/2/3 chosen with weights w1/w2/w3 (rand()%100 per column); returns building task |
| 401530 | task | building fall: fixed point y (+0x28 >> 6), lands when bottom reaches y 428 (floor height 118). Landing with vy > gravity → `f_40aae0()` + screen shake `f_401bc0()` |
| 4015c0 | create | floor task (prio 0x10), parent = building, data = 10 dwords |
| 401620 | task | floor: when building hit flag set and this is the bottom floor, subtract damage from the hit column; destroyed column → floor dead, building floor count--, score += combo*10*type*gravity*1.5, `f_4040b0(320, y)`, sound 7; otherwise score += combo*gravity*1.5. Score clamped to GS+0x3c. Draws 3 columns of 160x120 from surface 4+kind |
| 4017e0 | create | jump camera (prio 0x40, fn 401920), parent = player |
| 401850 | task | camera follows the falling player (y = player.f40 - 304 once locked; accelerating 0.05); dies when player state (+0x50) == 0; camera y clamped ≤ 0; writes GS+8, GS+0xc = GS+8+480 |
| 401920 | task | camera follows the rising player (until player vy +0x44 ≥ -2, then switches to 401850) |
| 4019d0 | create | camera scroll up to y-150 (prio 0x20) and back; pauses mask 0x7f; returns nullptr if camera already above |
| 401a70 | task | the scroll: small kick, scroll up 20/frame, hold 20 frames, back down 40/frame, resume mask 0x7f |
| 401bc0 | create | screen shake (prio 0x10): table {-20,2,-10,20,-4,13,2,-10,0,8,-2,4,-2,1} |
| 401c80 | task | shake step: GS+0 = table[i], GS+4 = table[i]+640, 14 steps |
| 401cc0 | create | player task (prio 8) + animation records at 0x41f468; also `f_4086e0(player)` and HUD `f_401210(player)`; returns the player (callers store it at MTASK(0x41f6cc)) |
| 401fb0 | task | player standing: Z attack, guard (held 7, gauge > 10), jump (held 6), lane move left/right (pressed 2/4), technique (held 8, GS+0x1c ≥ 300) |
| 4021d0 | task | standing variant used by other parts (snaps x/y to lane, gauge +2.5/frame, no draw) |
| 4022d0 / 402390 | task | lane move left / right (x eased with +0x48, +5/frame) |
| 402450 | task | guard on ground (gauge -1/frame) |
| 4024f0 / 402590 | task | guard recoil ground / air (gauge -4/frame, 8 frames; set by other parts) |
| 402670 | task | guard in the air |
| 402770 | task | ground slash (anim 0) |
| 402820 | task | jump crouch (anim 1), takes off: dust, jump camera, sound 9 |
| 4028f0 | task | airborne: gravity 0.1, land at y > 295 (sound 10), air slash, air technique, air guard |
| 402a80 | task | air slash (anim 2) |
| 402b60 / 402c30 | task | ground / air technique (anims 3 / 4); at the end, if `MU8(GS+0x29)`, `f_40b0c0(x+10, y-50)` |
| 402d40 / 402e00 | task | stunned on ground / in air, 30 frames (set by other parts) |
| 402ee0 | task | uncontrolled fall to the ground (set by other parts at stage end) |
| 402f90 | helper | animation tick: `f_402f90(anim_address)` |
| 402fd0 | helper | animation rewind |
| 402fe0 | create | stage banner (surface 56 = data/019.dat 440x100) |
| 403040 / 4030c0 | task | banner eases in to x=100, holds 180 frames, slides out |
| 403110 | create | `(x, y)` flying 斬 + combo number popup; uses 2 rand() calls (vx = rand%256-128, vy = (-rand)%64) |
| 4031c0 | task | popup physics (gravity 4 in 1/16 px), draws zan.bmp and up to 3 digits of combo.bmp |
| 4032a0 / 403320 | create/task | title screen left arrow (surface 55 lower half) — bounces on button 2, dies after decide (9) |
| 4033f0 / 403470 | create/task | title screen right arrow (surface 55 upper half) — bounces on button 4 |
| 403540 | create | 6-block tower (surface 41 = data/004.dat), each block 3 columns × 5 HP, wait 150, f04 = 1; returns tower |
| 403610 | task | tower fall / land (ground 429, block height 120) |
| 403690 / 4036f0 | create/task | tower block: hits, score (combo*10*gravity*1.5*100 when destroyed), draws 480x120 |
| 403870 / 4038e0 | create/task | final prompt (surface 40 = data/003.dat 600x120) eases to x=20; on decide spawns `f_40b350()` and when that task dies sets `MU8(GS+0x32)=1`; pauses mask 0x7f every frame |
| 403980 / 4039d0 | create/task | full screen flash with a COLORREF, toggles every 3 frames, 9 frames |

## Player task (prio 8) data

| off | type | meaning |
|---|---|---|
| 0x20 | int | x (screen = x - GS+0) |
| 0x24 | int | y (ground = 304) |
| 0x28 | int | lane 0..2 (x = 120*lane+136) |
| 0x2c | int | life (3) |
| 0x30 | int | technique timer (60) |
| 0x34 | int | recoil counter |
| 0x38 | float | defense gauge 0..100 |
| 0x3c | float | x |
| 0x40 | float | y |
| 0x44 | float | vy (jump -14, gravity 0.1) |
| 0x48 | float | lane move speed |
| 0x4c | int | animation index (record 0x41f468 + idx*0x68) |
| 0x50 | int | state: 0 stand, 1 slash, 2 crouch, 3 air, 4 air slash, 5 technique, 6 air technique, 7 guard, 10 air guard, 11 lane move |
| 0x54 | int | stun counter |

Animation record (0x68 bytes, 5 at 0x41f468/4d0/538/5a0/608): up to 10
(sx,sy) frame pairs, +0x50 current frame, +0x54 tick, +0x58 ticks per frame,
+0x5c loop flag, +0x5d finished flag, +0x60 loop frame, +0x64 last frame.

## Building task (f_401340, prio 0x10)

+0x20 y, +0x24 vy, +0x28 y<<6, +0x2c gravity, +0x30 floors left,
+0x34 graphic set, +0x38 (byte) hit flag, +0x3c hit column, +0x40 damage.
Floor tasks: +0x20 index, +0x24..+0x2c sprite y per column,
+0x30..+0x38 hit points, +0x3c..+0x44 type.
Tower (f_403540): +0x34 (byte) hit flag, +0x38 column, +0x3c damage; blocks:
+0x20 index, +0x24 sprite y, +0x28..+0x30 hit points.

## Game state fields used (GS = 0x41f688)

+0x0 camera x, +0x4 camera x + 640, +0x8 camera y, +0xc camera y + 480,
+0x10 score, +0x14 combo, +0x1c technique gauge (0..300), +0x29 (byte)
technique beam enabled, +0x32 (byte) set when the final prompt finishes,
+0x3c score cap, +0x48 / +0x50 sky fill pixels.

## External functions called (assumed signatures)

* `void  f_4085f0(int32_t x, int32_t y, int32_t loop)` — part c
* `Task* f_408230(int32_t x, int32_t y)` — part c
* `Task* f_4086e0(Task* parent)` — part c
* `void  f_4040b0(int32_t x, int32_t y)` — part b
* `void  f_40aae0()` — part d/e (declared in part_e.h)
* `Task* f_40b0c0(int32_t x, int32_t y)` — declared in part_e.h
* `Task* f_40b350()` — declared in part_e.h

## Notes

* All descriptor fields the original left uninitialised (player +0x4c,
  final prompt +0x2c/+0x30, flash parent) are zero here.
* `f_402ee0` uses the unrounded x87 sum for `ftol` (kept as a double).
* x87 compares are written so NaN behaves like the original
  (`!(a >= b)` for `test ah,1`, `a > b` for `test ah,0x41`).
