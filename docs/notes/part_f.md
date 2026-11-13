# Part f — 0x40d560 .. 0x410690 (scenes, replays, scores, game.sav)

Translated in `src/game/part_f.cpp` / `part_f.h`.

## Functions

| addr | purpose |
|---|---|
| 0x40d560 | Scene: 第一幕 in play, **recording**. Also keeps running after the switch to 第二幕 (`0x41f6ba`). Stage events from a 10-entry table indexed by `step+1`. End → result scene 8, rank via 0x40fe10, play-time/unlock update. |
| 0x40df30 | Scene 8: result screen (成果, 連斬 + max combo, 点 + score, rank "N位" if ranked, unlock message). Decide → title (scene 1 = 0x40c080) and saves the unlock level if it grew; Esc → quit. |
| 0x40e2d0 | Scene: 第一幕 replay **playback**. End (skip, `G_END` or player lives `+0x2c <= 0`) → scene 9 (0x40c4a0). |
| 0x40e750 | Set up 第二幕: background 0x401000, six clouds, player, score counter, camera, game state (`mode = 1`, `step = 0`, `event timer = 45`). |
| 0x40e840 | Scene 3: 第二幕 in play, recording. 9-entry event table indexed by `step`. End → scene 8, rank via 0x40ff80. |
| 0x40ef60 | Scene: 第二幕 replay playback. End → scene 10 (0x40ca00). |
| 0x40f2d0 | Set up とことん, sets scene 4 = 0x40f420; clears score/combo/frame/replay buffer, `step = -1`, `mode = 2`. |
| 0x40f420 | Scene 4: とことん in play, recording. A random building (0x401340) appears 45 frames after the previous one is gone. End → scene 8, rank via 0x410100 (no play-time update). |
| 0x40f850 | Scene: とことん replay playback. End → scene 11 (0x40cf10). |
| 0x40fad0 | Write `save/replay<n>.rep`. |
| 0x40fb90 | Read `save/replay<n>.rep` (returns false if missing). |
| 0x40fe10 / 0x40ff80 / 0x410100 | Insert the run into the score table of 第一幕 / 第二幕 / とことん, shift replays, save the run's replay, return rank 1..10 (11 = not ranked or file error). |
| 0x40ff30 / 0x4100b0 / 0x410230 | Read the score table (all zero if the file is missing). |
| 0x410280 / 0x4102d0 | Read / write the unlock level in `save/game.sav`. |
| 0x410310 / 0x410360 | Read / (fail to) write the play time in `save/game.sav`. |
| 0x4103a0, 0x4103c0, 0x4103d0, 0x410620, 0x410660 | `std::vector<RepEvent>` members (size, _Destroy, insert, uninitialized_copy, uninitialized_fill_n). |

## Frame structure of the play scenes

Recording (0x40d560, 0x40e840, 0x40f420):
1. `in_update()`.
2. For buttons **6, 7, 5, 8, 2, 4, 9** in this order: if held, `push_back({frame, button})`.
3. `task_run_all()`; `frame++` (`0x41f6ac`); if player `+0x30 == 0`: `stage_time++` (`0x41f6a8`).
4. End check (`0x41f6b0`), act-specific checks, then the event timer: when no event is busy (`0x41f6b1`), after 45 frames start the next event and store the returned stage object in `0x41f6c8`.
5. Esc (button 10) → quit.

Playback (0x40e2d0, 0x40ef60, 0x40f850):
1. `in_update()`; skip counter++ ; decide held after 120 frames arms the skip; releasing decide while armed ends the replay; Tab (11) held sets the fast-forward byte `0x41f6b8` (main loop runs the scene 4× per frame).
2. Esc → quit (return).
3. `in_clear()`, then for every event at the front of the buffer whose frame == `0x41f6ac`: `in_force(button)` and erase it from the front.
4. Same task run / counters as recording; continue only while `!end && player+0x2c > 0`, draw "DEMO" (surface 35) at (380,20).

The playback copies differ from the recording copies in small ways that must
be preserved: no `0x41f6a4 = 0` in the boss event, no unlock update in the
final event, the 第二幕/とことん playbacks have no 9,999,999 / act-switch checks.

## Stage event tables

第一幕 (`ev = step + 1`, unsigned, >9 → nothing):
0 random building (とことん style, `step` stays) · 1 0x403540 + 0x404300 + banner 0 ·
2 0x403a30 + 0x4049a0 + banner 1 · 3 0x408330 + banner 2 · 4 0x407c50 + banner 3 ·
5 fade-out 0x40b350 + text `data/023.dat` + player fn = 0x402ee0 ·
6 combo = 0, player fn = 0x401fb0, player+0x2c = 3, (rec only) `0x41f6a4 = 0`, 0x408df0, fade-in, boss 0x409540 + 0x406f00 + banner 4 ·
7 0x402fe0, player fn = 0x402ee0, 0x408880 ·
8 pause tasks mask 0x7f, fade-out, text `data/024.dat` ·
9 (rec only) unlock ≥ 2, end.

第二幕 (`ev = step`, >8 → nothing): the same list without entry 0, shifted down by one.

Random building: `lvl = stage_time/600; a = rand()%(lvl+1)+lvl+1 (max 100); b = rand()%3;
c = stage_time/1500+1; f_401340(c, a, b, 6, 3, 1)` (two `rand()` calls, in this order).

## Globals (game state at 0x41f688)

| addr | meaning |
|---|---|
| 0x41f688/68c/690/694 | scroll x, view width (640), scroll y, scroll y + 480 |
| 0x41f698 | score |
| 0x41f69c | current combo |
| 0x41f6a0 | max combo (stored in score tables) |
| 0x41f6a4 | (cleared at the boss) |
| 0x41f6a8 | stage time (frames, only while player `+0x30 == 0`) |
| 0x41f6ac | frame counter = replay time base |
| 0x41f6b0 (byte) | run over |
| 0x41f6b1 (byte) | stage event busy |
| 0x41f6b4 | frames since the last event finished |
| 0x41f6b8 (byte) | fast forward |
| 0x41f6b9 (byte) | 9,999,999 event already done |
| 0x41f6ba (byte) | request 第一幕 → 第二幕 |
| 0x41f6bc | stage event index |
| 0x41f6c0 | mode (1 = 第二幕 run, 2 = とことん; 第一幕 set in part e) |
| 0x41f6c4 | score cap 99,999,999 |
| 0x41f6c8 | **Task\*** current stage object (`MTASK`) |
| 0x41f6cc | **Task\*** player (`MTASK`); `+0x2c` lives, `+0x30` frozen flag |
| 0x41f6d4 | pixel of RGB(0xa0,0x20,0x20), result background |
| 0x41f6dc | unlock level: 0 第一幕, 1 +第二幕, 2 +とことん |
| 0x41f6e0 | play seconds (session only, see game.sav) |
| 0x41f6e4 | rank of the last run |
| 0x41f798 / 0x41f79c | scene id / scene function |
| 0x41f7b0 | replay seed (`srand` value) |
| 0x41f7b4..0x41f7c0 | `std::vector<RepEvent>`: allocator, `MPTR(0x41f7b8)` first, `MPTR(0x41f7bc)` last, `MPTR(0x41f7c0)` end (buffer from `::operator new` of 8-byte `RepEvent`, freed with `::operator delete` by part e f_40b550) |
| 0x41f85c/860, 0x41f864/868, 0x41f86c/870 | playback skip counter / armed byte for 第一幕, 第二幕, とことん |

The original stores an unrelated stack value in `0x41f6c8` after events that
create no object; the only reader (0x40b0c0) runs while an object exists, so
the port stores `nullptr`.

## File formats (all little endian)

### `save/replay<N>.rep`
N = 1..10 第一幕, 11..20 第二幕, 21..30 とことん (rank = N, N-10, N-20).

```
u32 seed                       // value given to srand() before the run (0x41f7b0)
i32 count
count × { i32 frame; u8 button; u8 pad[3] }   // pad = garbage in originals, 0 in the port
```
Events are in recording order: per frame, held buttons 6,7,5,8,2,4,9.
Playback injects them with `in_force` (state 0x80) at the matching frame.

### `save/score.sav`, `save/daini_score.sav`, `save/endless_score.sav`
Exactly 80 bytes: 10 × `{ i32 max_combo; i32 score; }`, best first. A run is
inserted before the first entry with `score_entry <= score` (signed). On
insertion `replay<base+10>.rep` is deleted and `replay<base+k>` is renamed to
`replay<base+k+1>` for k = 9 .. rank, then the run is saved as
`replay<base+rank>.rep`. The table is rewritten even when not ranked.
Original open mode is `"wbc"` (MSVC commit flag); the port uses `"wb"`.

### `save/game.sav`
`i32 unlock_level` (+ an intended `i32 play_seconds` that is never written).
0x4102d0 rewrites the file with only the 4-byte level. 0x410360 opens the
file with `"rb"` before its `fwrite`, so the play time is never stored.
0x410310 reads offset 4; on a 4-byte file the read fails and it returns its
uninitialised local, which in the original holds the level that 0x410280 just
read (its `pop ecx`). So at startup `play_seconds = unlock_level` (0..2), and
the 第一幕 unlock of 第二幕 by play time needs > 14400 s within one session.
The port reproduces this with `s_ecx_after_410280`.

## External functions called (assumed signatures)

`void f_401000()`, `void f_401100()`,
`Task* f_401340(int32_t,int32_t,int32_t,int32_t,int32_t,int32_t)`,
`Task* f_401cc0()`, `void f_401fb0(Task*)`, `void f_402ee0(Task*)`,
`void f_402fe0()`, `Task* f_403540()`, `void f_403870()`, `Task* f_403a30()`,
`void f_404300(Task*,Task*)`, `void f_4049a0(Task*,Task*)`,
`void f_406840(Task*,Task*)`, `void f_406f00(Task*,Task*)`, `Task* f_407c50()`,
`Task* f_408330()`, `void f_4085f0(int32_t,int32_t,int32_t)`,
`void f_408780(int32_t)`, `Task* f_408880()`, `void f_408ae0()`,
`void f_408df0()`, `void f_408f50(const char*)`, **`Task* f_409540()`**,
`Task* f_40b350()`, `Task* f_40b450()`, `void f_40c080()`, `void f_40c420()`,
`void f_40c4a0()`, `void f_40ca00()`, `void f_40cf10()`.

`f_409540` returns the boss task in EAX (`call 0x4128c0` is its last call) and
the event code passes it to `f_406f00` and stores it in `0x41f6c8`; part d
currently declares it `void`.

## Note on the conventions document

Its CRT table swaps two entries: **0x414483 is `fwrite`** (calls `_flsbuf`
0x416331 / `_write`) and **0x41458d is `fread`** (calls `_filbuf` 0x41407c).
0x414376 is `fflush`, 0x414312 `fclose`, 0x414675 `rename` (MoveFileA),
0x4146a3 `fseek`.
