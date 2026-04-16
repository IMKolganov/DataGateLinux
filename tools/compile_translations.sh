#!/bin/sh
# Regenerate i18n/datagate_*.qm from .ts (commit the .qm so CI/IDE builds work without lrelease on PATH).
# Requires: sudo apt install qt6-l10n-tools   (or Qt SDK with lrelease in PATH)
set -e
cd "$(dirname "$0")/.."
LR=
for c in lrelease-qt6 lrelease; do
  if command -v "$c" >/dev/null 2>&1; then
    LR=$c
    break
  fi
done
if [ -z "$LR" ]; then
  for p in /usr/lib/qt6/bin/lrelease /usr/lib/x86_64-linux-gnu/qt6/bin/lrelease; do
    if [ -x "$p" ]; then LR=$p; break; fi
  done
fi
if [ -z "$LR" ]; then
  echo "lrelease not found. Install: sudo apt install qt6-l10n-tools" >&2
  exit 1
fi
for t in i18n/datagate_*.ts; do
  b=$(basename "$t" .ts)
  echo "$LR $t -> i18n/${b}.qm"
  "$LR" "$t" -qm "i18n/${b}.qm"
done
