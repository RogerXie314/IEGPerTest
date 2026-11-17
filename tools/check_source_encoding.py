# -*- coding: utf-8 -*-
"""Guard against the GBK/UTF-8 mojibake regression (EF BF BD / UTF-8 BOM).

Why this exists
---------------
WLServerTest source files are GBK (CP936) + CRLF, no BOM. In the past a tool
read a GBK file as UTF-8 with errors='replace' and wrote it back: every invalid
GBK byte became the UTF-8 replacement character (EF BF BD). MSVC then read
those bytes as GBK and the UI displayed "锟斤拷..." in task status / log output.
The same corruption was reintroduced in commit 903beac (Fix9, 2026-10-14).

Run:
    python tools/check_source_encoding.py

Returns non-zero if any tracked source file is corrupted. Called by both build
scripts so the bad bytes cannot be compiled or packaged unnoticed.
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "external", "IEG_Code", "code", "WLServerTest")
MARKER = b"\xef\xbf\xbd"
BOM = b"\xef\xbb\xbf"
EXTS = (".cpp", ".h", ".rc", ".bat")


def tracked_sources():
    try:
        out = subprocess.check_output(
            ["git", "ls-files", "external/IEG_Code/code/WLServerTest"],
            cwd=ROOT, stderr=subprocess.DEVNULL).decode("utf-8", "replace")
    except Exception:
        out = ""
    files = [os.path.join(ROOT, p) for p in out.splitlines()
             if p.lower().endswith(EXTS)]
    if not files:  # fallback when git is unavailable
        files = [os.path.join(SRC, n) for n in os.listdir(SRC)
                 if n.lower().endswith(EXTS) and os.path.isfile(os.path.join(SRC, n))]
    return sorted(f for f in files if os.path.isfile(f))


def check_file(path):
    data = open(path, "rb").read()
    problems = []
    if data.startswith(BOM):
        problems.append("UTF-8 BOM present (source must be bare GBK)")
    if MARKER in data:
        lines = data.split(b"\r\n")
        for i, line in enumerate(lines, 1):
            if MARKER in line:
                text = line.decode("gbk", "replace").strip()[:120]
                problems.append("line %d: UTF-8 replacement bytes EF BF BD -> %s" % (i, text))
    return problems


def main():
    files = tracked_sources()
    bad = []
    for path in files:
        problems = check_file(path)
        if problems:
            bad.append((os.path.relpath(path, ROOT), problems))
    print("encoding guard: %d source files scanned" % len(files))
    for rel, problems in bad:
        print("[FAIL] %s" % rel)
        for p in problems[:20]:
            print("       %s" % p)
        if len(problems) > 20:
            print("       ... %d more" % (len(problems) - 20))
    if bad:
        print("\n[FAIL] GBK source corrupted. Restore from git / rewrite with "
              "open(path,'wb') + encode('gbk'); never use default UTF-8 writes.")
        return 1
    print("[OK] no EF BF BD markers, no UTF-8 BOM in tracked sources")
    return 0


if __name__ == "__main__":
    sys.exit(main())
