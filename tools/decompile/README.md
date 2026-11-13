# Decompilation tools

The C++ port was produced from the original `시작.exe` (PE32, MSVC 6, 2004
build of 斬るビル) with [angr](https://angr.io) 10.x as decompiler and
`objdump` disassembly as ground truth.

```bash
python3 -m venv venv && venv/bin/pip install angr
venv/bin/python list_functions.py 시작.exe functions.txt          # CFG recovery, 1003 functions
venv/bin/python decompile.py 시작.exe game.c 401000 413f30       # pseudo C of the game code
python3 compact.py game.c game_c.c                               # drop declarations / thunks
objdump -d -M intel --no-show-raw-insn 시작.exe > game.asm       # ground truth
```

* `0x401000-0x410690`  game logic (translated in `src/game/part_*.cpp`)
* `0x410690-0x413f30`  WinMain, DirectDraw/DirectSound/DirectMusic/input
  wrappers, task scheduler (re-implemented in `src/engine/`)
* `0x413f34-`          statically linked MSVC CRT

See `docs/TRANSLATION_CONVENTIONS.md` for how the translation maps the
binary onto C++.
