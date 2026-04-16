#!/usr/bin/env python3
"""Extract string literals passed to Datagate::tr(...) from C++ sources (concatenation supported)."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"


def read_c_string(s: str, i: int) -> tuple[str, int]:
    """Read one double-quoted C string starting at s[i]=='\"'. Returns (decoded, index_after_closing_quote)."""
    assert s[i] == '"'
    i += 1
    out: list[str] = []
    while i < len(s):
        c = s[i]
        if c == "\\":
            if i + 1 >= len(s):
                break
            n = s[i + 1]
            esc = {
                "n": "\n",
                "r": "\r",
                "t": "\t",
                "\\": "\\",
                '"': '"',
            }
            out.append(esc.get(n, n))
            i += 2
            continue
        if c == '"':
            return "".join(out), i + 1
        out.append(c)
        i += 1
    return "".join(out), i


def skip_ws(s: str, i: int) -> int:
    while i < len(s) and s[i] in " \t\n\r\v\f":
        i += 1
    return i


def parse_tr_call(s: str, start: int) -> tuple[str | None, int]:
    """After 'Datagate::tr(', parse until closing paren at depth 0. Returns concatenated string or None if not literal-only."""
    depth = 1
    i = start
    parts: list[str] = []
    while i < len(s) and depth > 0:
        i = skip_ws(s, i)
        if i >= len(s):
            break
        if s[i] == ")":
            depth -= 1
            i += 1
            break
        if s[i] == '"':
            chunk, i = read_c_string(s, i)
            parts.append(chunk)
            continue
        # Non-literal argument (e.g. .arg(...)) — skip this tr() occurrence
        return None, i
    if parts:
        return "".join(parts), i
    return None, i


def extract_all(text: str) -> list[str]:
    out: list[str] = []
    key = "Datagate::tr("
    pos = 0
    while True:
        j = text.find(key, pos)
        if j < 0:
            break
        arg_start = j + len(key)
        msg, end = parse_tr_call(text, arg_start)
        if msg is not None:
            out.append(msg)
        pos = j + len(key) if msg is None else end
    return out


def main() -> int:
    seen: set[str] = set()
    ordered: list[str] = []
    for path in sorted(SRC.rglob("*.cpp")):
        text = path.read_text(encoding="utf-8")
        for m in extract_all(text):
            if m not in seen:
                seen.add(m)
                ordered.append(m)
    for m in ordered:
        print(json.dumps({"en": m}, ensure_ascii=False))
    print(f"# {len(ordered)} unique strings", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
