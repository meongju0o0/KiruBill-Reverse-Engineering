#!/usr/bin/env python3
"""Replay regression test.

Plays every replay shipped with the original game (game/save/replay1..20.rep)
with the headless build of the port and checks that the final score and max
combo are exactly the ones stored in the original ranking files
(save/score.sav for 第一幕 = replays 1-10, save/daini_score.sav for
第二幕 = replays 11-20).  Any difference in game logic, random number usage
or floating point behaviour makes a replay diverge, so this test proves the
translation is bit exact.

usage: tests/run_replays.py path/to/KiruBill [game_dir]
"""
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

VK_RIGHT, VK_DOWN, VK_Z = 0x27, 0x28, 0x5A


def tap(lines, poll, vk):
    lines.append(f"{poll} {vk:#x} 1")
    lines.append(f"{poll + 3} {vk:#x} 0")


def input_script(k):
    """Title: Down -> ranking, Z.  List: (Right for 第二幕), Down x n, Z."""
    lines = []
    tap(lines, 30, VK_DOWN)
    tap(lines, 40, VK_Z)
    poll = 70
    n = k
    if k > 10:
        tap(lines, poll, VK_RIGHT)
        poll += 20
        n = k - 10
    for _ in range(n):
        tap(lines, poll, VK_DOWN)
        poll += 8
    tap(lines, poll + 10, VK_Z)
    return "\n".join(lines) + "\n"


def score_table(path):
    data = open(path, "rb").read()
    return [struct.unpack_from("<ii", data, 8 * i) for i in range(10)]


def replay_frames(path):
    data = open(path, "rb").read()
    _seed, count = struct.unpack_from("<Ii", data, 0)
    return struct.unpack_from("<i", data, 8 + (count - 1) * 8)[0]


def run_one(exe, game_dir, k):
    work = tempfile.mkdtemp(prefix=f"kb_replay{k}_")
    try:
        shutil.copytree(game_dir, os.path.join(work, "game"))
        gdir = os.path.join(work, "game")
        script = os.path.join(work, "input.txt")
        with open(script, "w") as f:
            f.write(input_script(k))
        frames = replay_frames(os.path.join(gdir, "save", f"replay{k}.rep")) + 1000
        out = subprocess.run([os.path.abspath(exe), "--headless", "--trace", "--frames", str(frames),
                              "--input", script], cwd=gdir, capture_output=True, text=True, timeout=1800)
        # playback scenes are 5 (第一幕) and 6 (第二幕); the end returns to a list
        m = None
        for line in out.stdout.splitlines():
            mm = re.search(r"scene ([567]) -> \d+\s+score=(-?\d+) combo=(-?\d+)", line)
            if mm:
                m = mm
        if not m:
            return k, None, None, out.stdout + out.stderr
        return k, int(m.group(2)), int(m.group(3)), ""
    finally:
        shutil.rmtree(work, ignore_errors=True)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    exe = sys.argv[1]
    game_dir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), "..", "game")
    expected = {}
    for i, (combo, score) in enumerate(score_table(os.path.join(game_dir, "save", "score.sav"))):
        expected[i + 1] = (score, combo)
    for i, (combo, score) in enumerate(score_table(os.path.join(game_dir, "save", "daini_score.sav"))):
        expected[i + 11] = (score, combo)

    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as pool:
        results = list(pool.map(lambda k: run_one(exe, game_dir, k), range(1, 21)))

    failed = 0
    for k, score, combo, log in results:
        exp = expected[k]
        ok = (score, combo) == exp
        failed += not ok
        print(f"replay{k:2d}: score={score} combo={combo}  expected score={exp[0]} combo={exp[1]}  "
              f"{'OK' if ok else 'MISMATCH'}")
        if log:
            print(log[-2000:])
    print(f"{20 - failed}/20 replays reproduce the original results")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
