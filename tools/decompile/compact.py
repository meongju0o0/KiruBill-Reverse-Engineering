#!/usr/bin/env python3
"""Shrink angr's output: drop struct typedefs, local declarations, externs and
one-line thunks created from inter-function NOP padding.

usage: compact.py game.c game_c.c
"""
import re
import sys

src = open(sys.argv[1]).read()
out = []
in_struct = False
for line in src.split("\n"):
    if re.match(r"^typedef struct .*\{$", line):
        in_struct = True
        continue
    if in_struct:
        if re.match(r"^\} .*;$", line):
            in_struct = False
        continue
    if re.match(r"^    [\w\s\*\[\]]+ \w+;\s+// ", line) or line.startswith("extern "):
        continue
    out.append(line)
txt = "\n".join(out)
txt = re.sub(r"// ===== \w+ \(\w+\) =====\n[^\n]+\n\{\n\n?    (return )?sub_\w+\([^\n]*\);"
             r"( /\* do not return \*/)?\n(    return;\n)?\}\n\n?", "", txt)
txt = re.sub(r"\n\n+", "\n", txt)
open(sys.argv[2], "w").write(txt)
