# Part D — original 0x408920 .. 0x40aae0

Translated in `src/game/part_d.cpp`, prototypes in `src/game/part_d.h`.

## Functions

| addr | purpose |
|---|---|
| 408920 | spawn falling leaf particle (3 rand calls: vy, x, vx) |
| 4089f0 | leaf update: x/y in 1/32 px (+0x2c/+0x24), random drift, 4-frame anim from surface 46 (data/009.dat) |
| 408ae0 | create score counter task (prio 0x20) |
| 408b20 | score counter: rolls displayed value (+0x20) toward GS+0x10 digit by digit; 7 digits at x=350 (8 digits at x=314 if GS+0x38) using surface 2 (score.bmp, 38x48 digits) at y=432 |
| 408c10(x,y) | slash effect: 18 frame table (sx=0, sy=128k) at +0x28+8k; counter +0xb8 |
| 408d80 | slash animation, frames 1..17 of surface 17 (slash.bmp), then dead |
| 408df0 | opening drop: GS+8 = -960, GS+4 = GS+8+480 (sic, GS+4 not GS+0xc), task with float y=-960 / v=10, `task_set_wait_mask(8,6000)` |
| 408e80 | drop update: y += v, v -= 0.05f; when y > 0: y = 0, dead, `task_resume_mask(8)`; GS+8 = ftol(y); draws youi.bmp (19) while y < -200, else hajime.bmp (20) at (220,190) |
| 408f50(file) | text reader (story data/023.dat, staff roll data/024.dat). Opens with `"r"` (MSVC text mode: CRLF→LF, ^Z = EOF). Decoding: byte c ≥ 0x20 → c^0x3a unless result < 0x20; decoded 0xff → 0xc5; raw 0xff or EOF ends the file. One f_409220 line task per non-empty line; first line delay = 40·line, later = 45·line (line counter includes blank lines). Manager task fn f_4090c0: +0x20 = first line (TTASK), +0x24 = speed |
| 4090c0 | text manager: speed 8 while button 9 held else 1; each line waits until its counter +0x58 reaches +0x54, then moves up; removed when y < -32 (prev pointer bug kept); draws `gfx_text((640-len*14)/2, y, s, len, 0xffffff)`. When no lines remain: GS+0x29 (byte) = 0 and task dies |
| 409220(parent,str,delay) | line task: text at +0x20 (≤ 44 bytes usable), y +0x4c = 480, delay +0x54, counter +0x58, next +0x5c (TTASK); fn f_4092c0; returns the task |
| 4092c0 | empty callback |
| 4092d0(x,y,dir) | debris piece (vx = 4-8·dir, vy = 4, hit byte +0x30), prio 8, then `f_405f20(MTASK(GS+0x44), task)` |
| 409370 | debris update (surface 49 = data/012.dat 16x16); dies when hit byte set or y+16 > 429 |
| 4093e0(x,y,dir) | shock wave (dir 0 falling, 1 left, 2 right; hit byte +0x30), prio 8, then `f_406590(MTASK(GS+0x44), task)` |
| 409470 | shock wave update (surface 58 = data/021.dat, 64x64, frame toggled via +0x28 ^= 0x40); falling wave hitting y+32 > 429 splits into left/right waves at y=397 |
| 409540 | create moon boss (prio 0x10, f04=5, HP 50, y=-960) |
| 4095c0 | moon form 1 (falls, bounces). Hit: HP -= dmg; non-fatal score += GS14·100; fatal score += (GS14+100)·15000, f_4040b0(320, y+380), snd 7, → form 2 (HP 70) |
| 409760 / 4099d0 / 409bc0 | moon form 2: hover + 5 attack objects (f_40acb0) / bounce / rise + lightning (f_408440, flash f_403980(0x93fdfd)). Hit: non-fatal GS14·150, fatal (GS14+100)·25000 → form 3 (f_409e70, HP 70) |
| 409e70 / 40a0a0 / 40a2e0 / 40a5f0 | moon form 3: bounce / high jump / scripted lightning barrage / hover + 3 rocks (f_407990). Hit: non-fatal GS14·300, fatal (GS14+200)·50000 → f_40a850 |
| 40a850 | moon death: rises, every 40 frames f_4040b0 + snd 7 + 4× f_40afa0(rand%480+100, y-rand%200+380, rand%2) (rand order: kind, y, x); dies after 540 frames |
| 40aa30 / 40aa80 | ending picture (surface 59 = data/022.dat) sliding from y=-100 to 100 at (200, y), 1400 frames |

All moon states draw surface 45 (data/008.dat, 640×480) at (-camX, y-camY).
Score adds are clamped to GS+0x3c. Ground contact: y + 468 ≥ 429.

## Globals / fields identified

* GS+0x00 camera x, GS+0x08 camera y (GS+0x04 / GS+0x0c the far edges).
* GS+0x10 score, GS+0x14 score multiplier (combo?), GS+0x3c score cap,
  GS+0x38 flag: 8-digit score display.
* GS+0x29 (byte): "text scroll running" flag cleared by f_4090c0.
* **GS+0x44 = 0x41f6cc is a `Task*`** (the player task, set at 0x40d48c from
  the return value of f_401cc0). Accessed as `MTASK(GS + 0x44)`; its data
  +0x28 is the player's lane (0..2) used for aimed lightning.
* Moon task fields: +0x20 y, +0x24 vy, +0x28 y<<6, +0x2c parameter,
  +0x30 HP, +0x34 hit flag (byte, set by attackers), +0x38 damage,
  +0x3c timer / hit counter.

## External functions called (assumed signatures)

```cpp
void f_40aae0();                                     // screen shake
void f_401bc0();                                     // impact effect
void f_4040b0(int32_t x, int32_t y);
void f_403980(uint32_t colorref);
void f_408440(int32_t lane);
void f_407990(int32_t x, Task* owner);
void f_40acb0(int32_t x, int32_t kind, Task* owner);
void f_40afa0(int32_t x, int32_t y, int32_t kind);
void f_405f20(Task* owner, Task* t);                 // owner = MTASK(GS+0x44)
void f_406590(Task* owner, Task* t);
```

## Notes

* Text drawing uses `gfx_text` only when `gfx_draw_enabled()` (the original
  passes a NULL DC to TextOutA otherwise).
* The text reader never fcloses in the original; the port reads the whole
  file up front with MSVC text-mode semantics.
