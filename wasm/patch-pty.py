#!/usr/bin/env python3
"""Make xterm-pty 0.12.0's Emscripten glue work with Emscripten 7+.

Emscripten 7 no longer defines the $TTY__deps metadata that the glue edits
(twice).  Wrap those two updates in existence checks.  The patch runs on the
checksum-verified pristine download and is idempotent.
"""
import sys

src, dst = sys.argv[1], sys.argv[2]
text = open(src, encoding="utf-8").read()

replacements = [
    (
        "Lib.$TTY__deps = Lib.$TTY__deps.filter(d => d !== '$FS_stdin_getChar');",
        "if (Lib.$TTY__deps) Lib.$TTY__deps = "
        "Lib.$TTY__deps.filter(d => d !== '$FS_stdin_getChar');",
    ),
    (
        "Lib.$TTY__deps.push('$PTY', '$PTY_askToWaitAgain');",
        "if (Lib.$TTY__deps) Lib.$TTY__deps.push('$PTY', '$PTY_askToWaitAgain');",
    ),
]

for old, new in replacements:
    if old in text:
        text = text.replace(old, new, 1)
    elif new not in text:
        sys.exit("patch-pty: expected text not found: " + old)

open(dst, "w", encoding="utf-8").write(text)
