# Translation conventions (binary → C++)

The port translates every game function of `시작.exe` one to one. These rules
keep the translation faithful enough that the original replay files
(`save/replay*.rep`) play back identically.

## Sources

Generated with the scripts in `tools/decompile/` (angr + objdump):

* `game.c`   – full angr decompilation (with struct typedefs / locals)
* `game_c.c` – compacted version (declarations and thunks removed)
* `game.asm` – `objdump -d -M intel` of the whole exe (ground truth)

angr often drops arguments pushed before indirect (vtable) calls, mislabels
fields of stack descriptors and mangles x87 code. **Whenever the decompiled
code is unclear, read the assembly.**

The per-part notes in `docs/notes/` were written during translation; their
"external functions" sections list the signatures assumed at that time, the
part headers `src/game/part_*.h` are authoritative.

## Naming

* Function at address X → `f_X` (lowercase hex, e.g. `f_401040`). Keep the
  address in the name; add a one line comment saying what it does.
* angr creates fake 1–15 byte "functions" from inter-function NOP padding
  (e.g. `sub_401035` that "returns sub_401040()"); ignore them.
* Callbacks stored in tasks: `void f_X(Task* t)`.
* Scene functions stored in `0x41f79c`: `void f_X()` (type `SceneFn`).

## Globals

All game globals are accessed by original address through `src/game/mem.h`:
`MI32(a)`, `MU32`, `MF32`, `MF64`, `MI16`, `MU16`, `MU8`, `MI8`,
`MPTR(a)` (void*&), `MTASK(a)` (Task*&), `MFN(a)` (SceneFn&).
`sub_40b5e0()` returns the game state struct at `GS` = `0x41f688`, so
`*(int*)(sub_40b5e0() + 8)` is `MI32(GS + 8)`.
Arrays: `MI32(0x41f468 + 4*i)`. Byte fields: `MU8`. Floats: `MF32`.
Do not invent new named globals for game state.
Strings from `.data` are in `src/game/strings.h` as `STR_<addr>`.

## Tasks

See `src/engine/task.h`. Header fields: `prio`(+0) `f04`(+4) `wait`(+8)
`parent`(+0xc) `prev`(+0x10) `next`(+0x14) `dead`(+0x18) `fn`(+0x1c).
User data by original offset: `TI32(t,0x24)`, `TF32`, `TU8`, `TI8`, `TU16`,
`TI16`, `TPTR`, `TTASK`, `TFN`.
`a0->field_c->field_20` in angr = parent task data at +0x20 →
`TI32(t->parent, 0x20)`.
Creating a task (original builds a 0x220-byte descriptor on the stack and
calls `0x4128c0`):

```cpp
Task d; task_desc(&d);          // zeroed descriptor
d.prio = 16; d.wait = 30; d.parent = t; d.fn = f_401530;
TI32(&d, 0x20) = x;             // user data
Task* n = task_create(&d);      // may return nullptr (pool full)
```

Scheduler helpers: `task_kill_all()` 0x412950, `task_set_wait_mask(mask,v)`
0x412970, `task_pause_mask(mask)` 0x4129b0, `task_resume_mask(mask)`
0x4129f0, `task_run_all()` 0x412870, `task_reap()` 0x412a30,
`task_system_init()` 0x412770. Walking the list: `for (Task* p =
task_head()->next; p != task_tail(); p = p->next)`.

## Engine calls (`src/engine/engine.h`)

Graphics object `sub_411780()` (0x41fa98), vtable 0x41c28c:

| original | port |
|---|---|
| vtbl+0x0c `(surf,x,y,w,h,sx,sy)` (0x411110) | `gfx_blit(surf,x,y,w,h,sx,sy)` |
| vtbl+0x10 `(colorref)` (0x411260) | `gfx_clear_rgb(c)` |
| vtbl+0x14 `(x,y,w,h,colorref)` (0x4112d0) | `gfx_fill_rgb(x,y,w,h,c)` |
| `sub_411370(gfx,x,y,w,h,pixel)` | `gfx_fill(x,y,w,h,pixel)` |
| `sub_411410(gfx,colorref)` | `gfx_rgb_to_pixel(c)` |
| vtbl+0x18 / +0x1c / `sub_411760` | `gfx_load_palette` / `gfx_set_palette` / `gfx_get_palette` |
| vtbl+0x28 | `gfx_flip()` |
| `sub_4114f0(gfx,surf,file,key,flags)` | `gfx_load_bmp(surf,file,key,flags)` |
| `*(char*)(sub_411780()+1564) = v` | `gfx_set_draw_enabled(v)` |
| GetDC/SetBkMode/SelectObject(gfx+0x620)/SetTextColor/TextOutA/ReleaseDC | `gfx_text(x,y,sjis,len,color)` |
| surface vtable (0x41c2bc) +0x0c/+0x14/+0x18/+0x08 | `gfx_surface_blit/fill_key/fill_rgb/load` |

Sound: `snd_load(file,idx)` 0x413400, `snd_play(idx)` 0x413610,
`snd_stop(idx)` 0x413690. Music: `mus_load(file,idx)` 0x412b90,
`mus_set_loop(idx,start,end)` 0x412d80, `mus_play(idx)` 0x412cb0,
`mus_stop(idx)` 0x412d20, `mus_unload(idx)` 0x412c30.
Input: `in_update()` 0x412090, `in_pressed(b)` 0x412180, `in_held(b)`
0x412150, `in_clear()` 0x412130, `in_force(b)` 0x4121b0, `in_bind(dev,b,code)`
0x4121d0. Buttons (keyboard / joystick): 1 up, 2 left, 3 down, 4 right,
5 Z/joy1 (attack), 6 up, 7 down/joy3, 8 X/joy2, 9 Z/joy1 (decide),
10 Esc, 11 Tab.
Quit: `plat_quit()` (0x4108e0). Error box: `plat_error(msg)` (0x410900).

CRT: `crt_rand()` 0x413f13, `crt_srand(s)` 0x413f09, `ftol(x)` 0x413f34,
`crt_time()` 0x414236. stdio: 0x414175 `fopen`, 0x414312/0x414376 `fclose`,
0x41458d `fread`, 0x414483 `fwrite`, 0x4146a3 `fseek`. 0x414368
`operator new`/malloc, 0x413efe `delete`/free, 0x414dd0 `memset`, 0x414e30
`memcpy`. Identify any other CRT routine from the asm (VC6 static CRT) and use
the standard C equivalent.

## Arithmetic (must be bit exact)

* Integers: `int32_t`/`uint32_t` with 32-bit wrap-around. Keep signedness of
  each compare/shift/divide as in the asm (`jl/jg` signed, `jb/ja` unsigned,
  `sar` vs `shr`, `idiv` vs `div`). Compiler idioms such as
  `(x*0x92492493>>32 ...)` are just divisions by constants – write the
  division.
* Floating point: the original runs on x87 with 53-bit precision. Evaluate
  every expression in `double` (promote float operands) and round to `float`
  only where the original stores to a 32-bit float (`fstp dword`). Float
  constants appear as e.g. `0.05000000074505806` = `(double)0.05f`; write
  them as `(double)0.05f` or the exact double. Comparisons of floats with
  doubles are done in double. Integer↔float conversions: `(double)(int32_t)x`
  and `ftol()` (truncation).
* `rand()` order matters (replays re-seed and replay inputs): never reorder or
  drop `crt_rand()` calls, including ones whose result is unused.

## Output

Each part writes `src/game/part_<x>.cpp` and `src/game/part_<x>.h`
(prototypes of every function it defines). The .cpp starts with
`#include "game.h"`. Functions defined in another part that you call must be
declared in a block at the top of your .cpp:
`// ---- defined in other parts ----`. Check your file with
`g++ -std=c++17 -fsyntax-only -Isrc src/game/part_<x>.cpp`.
Also write `docs/notes/part_<x>.md`: what each function does in game terms,
meaning of the globals / task fields you identified, file formats, and the
list of external functions you call with the signature you assumed.
