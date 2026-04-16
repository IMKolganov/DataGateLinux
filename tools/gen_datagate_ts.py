#!/usr/bin/env python3
"""Generate i18n/datagate_{en,ru,fr,el}.ts from tools/catalog.jsonl (UTF-8, one JSON object per line)."""

from __future__ import annotations

import json
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "tools" / "catalog.jsonl"
OUT_DIR = ROOT / "i18n"

LANGS = (
    ("en", "en_US"),
    ("ru", "ru_RU"),
    ("fr", "fr_FR"),
    ("el", "el_GR"),
)


def ts_escape(s: str) -> str:
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def write_ts(lang_code: str, locale: str, rows: list[dict]) -> None:
    lines = [
        '<?xml version="1.0" encoding="utf-8"?>',
        "<!DOCTYPE TS>",
        f'<TS version="2.1" language="{locale}">',
        "  <context>",
        "    <name>Datagate</name>",
    ]
    key = lang_code
    for row in rows:
        src = row["en"]
        if key == "en":
            trans = row.get("en", src)
        else:
            trans = row.get(key, "")
        lines.append("    <message>")
        lines.append(f"      <source>{ts_escape(src)}</source>")
        if not trans.strip():
            lines.append("      <translation type=\"unfinished\"></translation>")
        else:
            lines.append(f"      <translation>{ts_escape(trans)}</translation>")
        lines.append("    </message>")
    lines.extend(["  </context>", "</TS>", ""])
    out = OUT_DIR / f"datagate_{lang_code}.ts"
    out.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {out}", file=sys.stderr)


def main() -> int:
    if not CATALOG.is_file():
        print(f"Missing {CATALOG}", file=sys.stderr)
        return 1
    rows: list[dict] = []
    for line in CATALOG.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        rows.append(json.loads(line))
    if not rows:
        print("Empty catalog", file=sys.stderr)
        return 1
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for code, loc in LANGS:
        write_ts(code, loc, rows)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
