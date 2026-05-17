#!/usr/bin/env python3
"""
Strip all C/C++ comments from .c/.h files while respecting string and char
literals, and preserving preprocessor directives. In-place rewrite.

Usage:
    python3 .strip_comments.py path1 [path2 ...]

Each path is a file or a directory (walked recursively for *.c, *.h).
A backup of the original is written to <file>.bak (skipped if already exists).
"""
import os
import sys


def strip_comments(src: str) -> str:
    out = []
    i = 0
    n = len(src)
    in_line_comment = False
    in_block_comment = False
    in_string = False
    in_char = False
    while i < n:
        c = src[i]
        nxt = src[i + 1] if i + 1 < n else ""

        if in_line_comment:
            if c == "\n":
                in_line_comment = False
                out.append(c)
            i += 1
            continue

        if in_block_comment:
            if c == "*" and nxt == "/":
                in_block_comment = False
                i += 2
                continue
            if c == "\n":
                out.append(c)
            i += 1
            continue

        if in_string:
            out.append(c)
            if c == "\\" and i + 1 < n:
                out.append(src[i + 1])
                i += 2
                continue
            if c == '"':
                in_string = False
            i += 1
            continue

        if in_char:
            out.append(c)
            if c == "\\" and i + 1 < n:
                out.append(src[i + 1])
                i += 2
                continue
            if c == "'":
                in_char = False
            i += 1
            continue

        if c == "/" and nxt == "/":
            in_line_comment = True
            i += 2
            continue
        if c == "/" and nxt == "*":
            in_block_comment = True
            i += 2
            continue
        if c == '"':
            in_string = True
            out.append(c)
            i += 1
            continue
        if c == "'":
            in_char = True
            out.append(c)
            i += 1
            continue

        out.append(c)
        i += 1

    result = "".join(out)

    # Collapse runs of blank lines (3+ → 2)
    lines = result.split("\n")
    cleaned = []
    blank_run = 0
    for line in lines:
        if line.strip() == "":
            blank_run += 1
            if blank_run <= 1:
                cleaned.append(line)
        else:
            blank_run = 0
            # Strip trailing whitespace introduced by removed trailing comments
            cleaned.append(line.rstrip())
    return "\n".join(cleaned)


def process_file(path: str) -> None:
    with open(path, "r", encoding="utf-8", errors="surrogateescape") as f:
        src = f.read()
    bak = path + ".bak"
    if not os.path.exists(bak):
        with open(bak, "w", encoding="utf-8", errors="surrogateescape") as f:
            f.write(src)
    stripped = strip_comments(src)
    with open(path, "w", encoding="utf-8", errors="surrogateescape") as f:
        f.write(stripped)


def main():
    targets = sys.argv[1:]
    if not targets:
        print("usage: strip_comments.py <path> [...]", file=sys.stderr)
        sys.exit(2)
    files = []
    for t in targets:
        if os.path.isdir(t):
            for root, _, names in os.walk(t):
                for n in names:
                    if n.endswith((".c", ".h")):
                        files.append(os.path.join(root, n))
        elif os.path.isfile(t):
            files.append(t)
    for fp in files:
        process_file(fp)
        print(f"stripped {fp}")


if __name__ == "__main__":
    main()
