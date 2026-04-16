#!/usr/bin/env python3
"""Build tools/catalog.jsonl from extract order + tools/translations_index.py."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXTRACT = ROOT / "tools" / "extract_datagate_tr_strings.py"
CATALOG_OUT = ROOT / "tools" / "catalog.jsonl"


def extracted_en_list() -> list[str]:
    r = subprocess.run(
        [sys.executable, str(EXTRACT)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=True,
    )
    out: list[str] = []
    for line in r.stdout.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        out.append(json.loads(line)["en"])
    return out


def main() -> int:
    sys.path.insert(0, str(ROOT / "tools"))
    from translations_index import TRANS  # type: ignore

    ens = extracted_en_list()
    if len(TRANS) != len(ens):
        print(
            f"translations_index.TRANS length {len(TRANS)} != extracted {len(ens)} — run extract and update TRANS.",
            file=sys.stderr,
        )
        return 1
    lines: list[str] = []
    for en, tr in zip(ens, TRANS, strict=True):
        row = {"en": en, "ru": tr["ru"], "fr": tr["fr"], "el": tr["el"]}
        lines.append(json.dumps(row, ensure_ascii=False))
    CATALOG_OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {CATALOG_OUT} ({len(lines)} rows)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
