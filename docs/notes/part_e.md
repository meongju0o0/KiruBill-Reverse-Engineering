# part_e — original functions 0x40aae0 .. 0x40d560

## Functions

| addr | port | purpose |
|---|---|---|
| 0x40aae0 | `f_40aae0()` | spawn the landing dust cloud (prio 0x10) at (0x40, 0xac) |
| 0x40ac20 | `f_40ac20(t)` | dust animation: surface 3 (tuti.bmp) 15 frames 512x256, one frame per 2 ticks |
| 0x40acb0 | `f_40acb0(x, type, parent)` | spawn a guard enemy on a building floor (prio 8) + `f_407640(player, guard)` |
| 0x40ad50 | `f_40ad50(t)` | guard: walks in/out, shield/hp, shoots `f_4092d0` between age 200..240, falls off when killed or floor gone |
| 0x40afa0 | `f_40afa0(x, y, type)` | spawn a falling defeated guard (random vx = rand()%80-40) |
| 0x40b030 | `f_40b030(t)` | falling guard, fixed point (>>4) velocity, killed off screen |
| 0x40b0c0 | `f_40b0c0(x, y)` → Task* | waza (special move) beam: two half tasks (prio 0x20 / 8), stage specific hit dispatch, camera task stored at beam +0x34 |
| 0x40b2c0 | `f_40b2c0(t)` | beam half: rises 20 px/frame; when off screen clears player +0x30 and resumes prio mask 0x7f |
| 0x40b350 / 0x40b390 | `f_40b350()` → Task*, `f_40b390(t)` | palette fade out over 32 frames (prio 0x10). Callers store the returned task. |
| 0x40b450 / 0x40b490 | `f_40b450()` → Task*, `f_40b490(t)` | palette fade in over 32 frames (prio 0x40) |
| 0x40b550 / 0x40b580 | `f_40b550()`, `f_40b580()` | static ctor / atexit dtor of the replay object at 0x41f7b0 |
| 0x40b5f0 | `f_40b5f0()` | startup: load screen, key bindings, all surfaces / sounds, palette, UI colours, first scene, `mkdir save` |
| 0x40bf30 | `f_40bf30()` | shutdown: write game.sav values, kill + reap all tasks |
| 0x40bf50 | `f_40bf50()` | main loop tick (timing / frame skip / flip) |
| 0x40c050 | `f_40c050()` | scene 0: poll input once, go to title |
| 0x40c080 | `f_40c080()` | scene 1: title |
| 0x40c420 | `f_40c420()` | spawn page arrows for the replay list if another list exists |
| 0x40c4a0 | `f_40c4a0()` | scene 9: replay list 第一幕 |
| 0x40ca00 | `f_40ca00()` | scene 10: replay list 第二幕 |
| 0x40cf10 | `f_40cf10()` | scene 11: replay list とことん |
| 0x40d410 | `f_40d410()` | reset game state, start 第一幕 (scene 2 = 0x40d560) |

0x40b5e0 (`mov eax, 0x41f688; ret`) is the GS getter and is not translated.

## Scenes (MI32(0x41f798) id / MFN(0x41f79c) fn)

0 `f_40c050` pre-title, 1 `f_40c080` title, 2 `f_40d560` 第一幕, 3 `f_40e840`
第二幕, 4 とことん (set by `f_40f2d0`), 5 `f_40e2d0` / 6 `f_40ef60` / 7
`f_40f850` replay playback of the three modes, 9/10/11 replay lists.

Title: Up/Down move the cursor `MI32(0x41f838)` (0 start, 1 replays);
Left/Right cycle the mode `MI32(0x41f83c)` within the unlock level
`MI32(0x41f6dc)` (0..2). Decide (button 9) starts the mode:

* 0 第一幕: `mus_load("sound/bgm.mid",0)`, loop 0..139000, play, `f_40d410`.
* 1 第二幕: `mus_load("data/000.dat",0)` (a MIDI file), loop 0x7b00..0x3a500,
  play, `f_40d410`, kill all tasks, run tasks once (reap), `f_40e750`, scene 3.
* 2 とことん: bgm.mid, loop 0..139000, `f_40f2d0`.

Every start sets `MU32(0x41f7b0) = time(NULL)` and `srand()` with it (the
replay seed). Replay playback sets the seed from the replay file instead
(`f_40fb90(n)` then `crt_srand(MU32(0x41f7b0))`).

Replay lists: 10 rows; row i shows i, combo, score and a "saiengi" (replay)
button if `save/replay<k>.rep` exists (k = i, i+10, i+20). Selection 0 is
the "modoru" (back) button. Left/Right switch between the lists that exist.

## Globals identified

| addr | meaning |
|---|---|
| 0x41e058 (u8, init 1) | title: page arrows need (re)spawning |
| GS+0x00 0x41f688 | camera x |
| GS+0x04 0x41f68c | set to 640 at game start |
| GS+0x08 0x41f690 | camera y |
| GS+0x0c 0x41f694 | camera y + 480 |
| GS+0x10..0x3c | reset in `f_40d410` (0x41f698 = 0, 0x41f69c, 0x41f6a0, 0x41f6a4, 0x41f6a8, 0x41f6ac = 0, bytes 0x41f6b0/b1 = 0, 0x41f6b4 = 0, 0x41f6bc = -1, 0x41f6c0 = 0, 0x41f6c4 = 9999999) |
| 0x41f6b8 (u8) | replay fast forward: scene runs 4x per tick |
| 0x41f6b9 / 0x41f6ba (u8) | reset to 0 at game start |
| GS+0x40 0x41f6c8 | **Task\*** current stage/building task; its `f04` is the stage type 0..5 used by the waza dispatch |
| GS+0x44 0x41f6cc | **Task\*** player task (`f_401cc0()` result). Player +0x28 = current lane / object index, +0x30 = "waza in progress" flag |
| GS+0x48/0x4c/0x50 0x41f6d0/d4/d8 | pixel values of COLORREF 0xfe9656, 0x2020a0, 0x343434 |
| 0x41f6dc | unlock level (save/game.sav dword 0) |
| 0x41f6e0 | play time (save/game.sav dword 1) |
| 0x41f6e8 (u8) bit0 | main loop timer initialised |
| 0x41f6f0..0x41f740 | 第二幕 score table: 10 x {combo, score} |
| 0x41f740..0x41f790 | 第一幕 score table |
| 0x41f7c8..0x41f818 | とことん score table |
| 0x41f790 / 0x41f824 / 0x41f7ac | list selection (第一幕 / 第二幕 / とことん) |
| 0x41f818+i / 0x41f7a0+i / 0x41f678+i (u8, i=1..10) | replay file exists |
| 0x41f844 / 0x41f84c / 0x41f854 | list scene initialised |
| 0x41f848 / 0x41f849 (u8) | list 9: daini / endless score file exists |
| 0x41f850 (u8) | list 10: endless score file exists |
| 0x41f858 (u8) | list 11: daini score file exists |
| 0x41f798 / 0x41f79c | scene id / scene function |
| 0x41f7b0 | replay object: +0 seed (u32), +4 allocator byte, +8/+0xc/+0x10 std::vector begin/end/capacity (8-byte records) — pointers in `MPTR(0x41f7b8/bc/c0)` |
| 0x41f828 / 0x41f82c | main loop: next frame time / now (timeGetTime) |
| 0x41f830 | main loop frame counter (wraps at 99999999) |
| 0x41f834 (u8) | frame skip toggle |
| 0x41f838 / 0x41f83c / 0x41f840 | title cursor / mode / idle counter |

## Main loop tick (0x40bf50)

```
first call: next = timeGetTime()
now = timeGetTime(); if (now - next <= 16) return      (unsigned)
if (fast_forward) { draw off; scene(); scene(); scene(); draw on }
scene(); flip()
next += 16 + (counter % 3 != 0)                         -> 60 fps
draw = 1; if (now > next + 10) draw = !last_draw        -> skip every 2nd frame when late
last_draw = draw; gfx_set_draw_enabled(draw)
if (now > next + 500) next = now
if (++counter == 99999999) counter = 0
```
The caller (WinMain message loop 0x410bd0) calls this whenever no message is
pending and 0x41fa90 (quit requested) is clear.

## Task layouts

* dust (0x40aae0): +0x20 x=0x40, +0x24 y=0xac, +0x28+8i sx (0), +0x2c+8i sy
  (0x100*i, i<15), +0xa0 frame, +0xa4 tick.
* guard (0x40acb0): parent = building floor (parent +0x20 floor y, +0x30/+0x38
  compared, +0x34 byte "collapsed"); +0x20 x, +0x24 y offset, +0x28 hp 2,
  +0x2c age, +0x30 type, +0x34 recoil (-90), +0x38 shield, +0x3c u8 hit flag,
  +0x40 damage. Other code hits it by setting +0x3c and +0x40.
* falling guard (0x40afa0): +0x20 x, +0x24 y, +0x28 vx, +0x2c vy, +0x30 type.
* waza beam (0x40b0c0): +0x20 x, +0x24 y, +0x28 speed 20, +0x2c half,
  first half +0x34 = task returned by `f_4019d0` (stored with `TTASK`).
* fades: +0x20 frame counter.

## Notes for the engine / integration

* **CRT mapping correction**: 0x41458d is `fread` and 0x414483 is `fwrite`
  (0x410280 opens game.sav "rb" and calls 0x41458d; 0x4102d0 opens "wb" and
  calls 0x414483). TRANSLATION_CONVENTIONS.md lists them the other way round.
* `gfx_get_palette` (0x411760) must return the palette loaded by
  `gfx_load_palette` (stored at gfx+0x21c); `gfx_set_palette` must NOT update
  it. The fades compute `base * k / 32` from it every frame.
* `f_40b550` is a static initializer from the CRT init table (0x41e004); the
  port must call it once at startup (it registers `f_40b580` with atexit).
  `f_40b580` frees the vector storage with `::operator delete`; part_f's
  vector code must allocate with `::operator new` (VC6 0x414368).
* `f_40b5f0` creates the save directory itself (`_mkdir`/`mkdir`); no
  engine API exists for it.
* The font (CreateFontA(28, 14, ..., SHIFTJIS_CHARSET, "HGS行書体") fallback
  "ＭＳ 明朝") is left to the engine.
* `f_40bf30` calls 0x412860 which is `task_kill_all(); task_reap();`.
