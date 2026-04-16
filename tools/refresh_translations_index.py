#!/usr/bin/env python3
"""Rebuild tools/translations_index.py from extract order + catalog.jsonl + fixed extras."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# New or changed strings not yet in old catalog (ru, fr, el)
EXTRA: dict[str, tuple[str, str, str]] = {
    "You're signed in": (
        "Вы вошли",
        "Vous êtes connecté(e).",
        "Έχετε συνδεθεί.",
    ),
    "You can close this tab and return to the DataGate app.": (
        "Можно закрыть эту вкладку и вернуться в DataGate.",
        "Vous pouvez fermer cet onglet et revenir à DataGate.",
        "Μπορείτε να κλείσετε αυτή την καρτέλα και να επιστρέψετε στο DataGate.",
    ),
    "Sign-in did not complete": (
        "Вход не завершён",
        "La connexion n’a pas abouti.",
        "Η σύνδεση δεν ολοκληρώθηκε.",
    ),
    "You can close this tab and try again from DataGate.": (
        "Закройте вкладку и попробуйте снова из DataGate.",
        "Fermez cet onglet et réessayez depuis DataGate.",
        "Κλείστε αυτή την καρτέλα και δοκιμάστε ξανά από το DataGate.",
    ),
    "Invalid sign-in request": (
        "Некорректный запрос входа",
        "Requête de connexion invalide.",
        "Μη έγκυρο αίτημα σύνδεσης.",
    ),
    "Close this tab and start sign-in again from DataGate.": (
        "Закройте вкладку и начните вход снова из DataGate.",
        "Fermez cet onglet et recommencez la connexion depuis DataGate.",
        "Κλείστε αυτή την καρτέλα και ξεκινήστε ξανά τη σύνδεση από το DataGate.",
    ),
    "More apps": (
        "Другие приложения",
        "Autres applications",
        "Περισσότερες εφαρμογές",
    ),
    "You can download other DataGate apps for your devices from the website:": (
        "Другие приложения DataGate для ваших устройств можно скачать на сайте:",
        "Vous pouvez télécharger les autres applications DataGate pour vos appareils sur le site :",
        "Μπορείτε να κατεβάσετε άλλες εφαρμογές DataGate για τις συσκευές σας από τον ιστότοπο:",
    ),
    "Windows": (
        "Windows",
        "Windows",
        "Windows",
    ),
    "Linux": (
        "Linux",
        "Linux",
        "Linux",
    ),
    "Android": (
        "Android",
        "Android",
        "Android",
    ),
    "iOS": (
        "iOS",
        "iOS",
        "iOS",
    ),
}


def extracted_en_list() -> list[str]:
    r = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "extract_datagate_tr_strings.py")],
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
    catalog_path = ROOT / "tools" / "catalog.jsonl"
    old: dict[str, dict[str, str]] = {}
    if catalog_path.is_file():
        for line in catalog_path.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            o = json.loads(line)
            old[o["en"]] = {"ru": o["ru"], "fr": o["fr"], "el": o["el"]}

    ens = extracted_en_list()
    rows: list[str] = [
        "# -*- coding: utf-8 -*-",
        "# Aligned with tools/extract_datagate_tr_strings.py (same length as extracted list).",
        "# Regenerate: python3 tools/refresh_translations_index.py",
        "TRANS = [",
    ]
    for i, en in enumerate(ens):
        if en in old:
            t = old[en]
        elif en in EXTRA:
            ru, fr, el = EXTRA[en]
            t = {"ru": ru, "fr": fr, "el": el}
        else:
            t = {"ru": en, "fr": en, "el": en}
        comment = en.replace("\n", " ")[:56]
        rows.append(
            f'    {{"ru": {repr(t["ru"])}, "fr": {repr(t["fr"])}, "el": {repr(t["el"])}}},  # {i} {comment}'
        )
    rows.append("]")
    out = ROOT / "tools" / "translations_index.py"
    out.write_text("\n".join(rows) + "\n", encoding="utf-8")
    print(f"Wrote {len(ens)} entries to {out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
