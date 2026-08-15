#!/usr/bin/env bash
# Smoke checks for XrayConfigBuilder — delegates to tools/run_tests.sh (subset via env).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec bash "$ROOT/tools/run_tests.sh"
