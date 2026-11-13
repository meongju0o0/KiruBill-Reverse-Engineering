# Part C — 0x405f70 .. 0x408920

Translated in `src/game/part_c.cpp` / `part_c.h`. All functions in the range
were translated (angr's padding "functions" skipped).

## Shared data

Player task (`pl`, created elsewhere; also `MTASK(GS+0x44)`):

| off | meaning |
|---|---|
| 0x20 / 0x24 | x / y (int) |
| 0x28 | lane / column (0..2) of the player |
| 0x2c | lives |
| 0x30 | invincibility timer (set to 180 when hurt, 0 when attacking) |
| 0x38 | float damage gauge 0..100 (+30 per hit taken) |
| 0x40 / 0x44 | float y / vy used while riding/standing on an object |
| 0x50 | state (0,2,11 idle-ish, 1 attack, 3/4 jump attack, 6 jump, 7/8 guard, 9/10 guard bounce, 12 crushed, 13/14 hurt) |
| 0x54 | flag cleared while touching an object in state 13/14 |
| fn  | per-state callback; collisions switch it to f_401fb0 (normal), f_4021d0 (crushed), f_4024f0 (guard), f_402590 (guard bounce), f_402d40 / f_402e00 (hurt) |

Game state (`GS` = 0x41f688): +0x00 camera x, +0x08 camera y, +0x10 score,
+0x14 combo, +0x18 best combo, +0x1c special gauge (0..300), +0x29 (byte)
stage gimmick active, +0x34 banner counter, +0x3c score cap, +0x40 gimmick
value cleared with +0x29, **+0x44 is a Task\* (player) → `MTASK(GS+0x44)`**.

Common sequences (helpers in part_c.cpp, order preserved):
* hurt: best combo = max(best, combo), combo = 0, gauge +30 (cap 100.0f),
  special +90 (cap 300), sound 13 (6 when crushed), `f_403f50()` if lives <= 0.
* hit: combo +1 (cap 999), special += min(30,(combo+1)/2) (cap 300),
  `f_408c10(px,py-30)`, `f_403110(px-70, py-camy-30)`,
  `task_set_wait_mask(0x7f,3)` (hit stop), `snd_play(rand()%3)`.

## Functions

| addr | purpose |
|---|---|
| 405f70 | collision task player vs small object (`TTASK(t,0x24)`: 0x20 x, 0x24 y, 0x30 byte hit, 0x34) |
| 406590 | spawn 4065e0 (prio 0x10, data 0x20 pl, 0x24 obj) |
| 4065e0 | player vs 48x48 object, only hurts the player |
| 406840 | spawn 406890 (prio 0x20) |
| 406890 | player vs falling building (building: 0x20 y, 0x24 vy, 0x28 y*64, 0x2c weight, 0x30 floors, 0x38 byte hit, 0x3c lane of hit, 0x40 hit power 1/5); bottom = y + 118*floors + 2, ground = 428. Pauses/resumes prio-mask 8 tasks (the building) when it rests on the player |
| 406dc0 | if `GS+0x29`: spawn 406e20 (prio 0x20) |
| 406e20 | special attack object vs building (marks hit, combo +1, popup) |
| 406f00 | spawn 406f60 (prio 0x20, data 0x20 pl, 0x28 obj) |
| 406f60 | player vs big falling object (bottom = y + 468, ground 429; obj 0x34 byte hit, 0x38 power) |
| 4074a0 | if `GS+0x29`: spawn 407520 (prio 0x20, a,b,c, 5 hits) |
| 407520 | special attack vs big object; after 5 hits kills a, b, itself, white flash, releases hit stop |
| 407640 | spawn 407690 (prio 0x10) |
| 407690 | player vs object positioned relative to its parent (obj 0x3c byte hit) |
| 407990 | spawn destructible part 407a20 (prio 8, parent, 6 hp) + `f_4050f0(player, part)` |
| 407a20 | destructible part (surface 57); emits `f_4093e0` at ages 210/215/220, explodes into 6 debris (`f_404170`) |
| 407c50 | create rubble gimmick manager 407ce0 (1800 frames) with 3 column spawners; calls `f_403d60()`; returns the manager |
| 407ce0 | rubble manager; ends the gimmick (`GS+0x29=0`, `GS+0x40=0`) when no column has pieces |
| 407d40 | create column spawner 407db0 (parent, col) |
| 407db0 | drops a piece every 30..129 frames (`rand()%100+30`), links pieces (0x44 next) |
| 407e30 | create rubble piece 407ef0 + `f_405440(player, piece)` |
| 407ef0 | rubble piece physics (y in 1/16 px), score combo*3*45 per hit and combo*3*405 when destroyed, surface 44 |
| 4080a0 / 4080f0 | rubble break effect (4 frames x 8 debris particles) |
| 408160 / 4081b0 | stacking: piece a lands on piece b (85 px above, velocity exchange) |
| 408230 / 4082c0 | jump dust effect (surface 9, 3 frames) |
| 408330 / 408390 | thunderstorm gimmick (1800 frames, sound 14 at start) |
| 408440 / 4084f0 | lightning bolt on a lane (surface 43); cut bonus combo*3000; sound 12 at end |
| 4085f0 / 408660 | cloud (surface 18), x in 1/16 px, optional wrap |
| 4086e0 / 408720 | lives display (surface 1) |
| 408780 / 4087f0 | sliding banner (surface 52, frame = arg) |
| 408880 / 4088d0 | meteor shower gimmick (1200 frames, `f_40aa30()` at 200, `f_408920()` 1/3 per frame) |

## External functions called (assumed signatures)

```
void f_401fb0(Task*); void f_4021d0(Task*); void f_4024f0(Task*);
void f_402590(Task*); void f_402d40(Task*); void f_402e00(Task*);   // part a
void f_403110(int32_t x, int32_t y);                                 // part a
void f_403980(int32_t colorref);                                     // part a
void f_403d60();  void f_403f50();                                   // part b
void f_404170(int32_t x, int32_t y, int32_t kind);                   // part b
void f_4050f0(Task*, Task*); void f_405440(Task*, Task*);
void f_405b10(Task*, Task*);                                         // part b
void f_408920(); void f_408c10(int32_t x, int32_t y);
void f_4093e0(int32_t x, int32_t y, int32_t kind); void f_40aa30();  // part d
```

## Notes / caveats

* Task `dead` is written/tested as a byte in the original; translated as
  `t->dead = 1` / `(uint8_t)t->dead`.
* Float code follows the x87 sequence (evaluate in double, round on store).
  Build with `-ffp-contract=off` so the compiler never fuses `a*b+c`.
* Several descriptors leave fields uninitialised in the original (e.g.
  406f00 +0x24/+0x2c, 407c50 +0x24.., byte fields with garbage upper bytes);
  they are zero here and are never read before being written.
* `-0x200 / weight` (406890/406f60 state 8) would divide by zero if a weight
  were 0, exactly like the original.
