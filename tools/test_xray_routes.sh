#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if ! command -v unshare >/dev/null || ! unshare --user --map-root-user --net true 2>/dev/null; then
  echo "SKIP  xray_routes (private user/network namespaces unavailable)"
  exit 0
fi
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT
mkdir "$WORKDIR/bin"
# Prevent DNS teardown from contacting host services or writing the host resolv.conf.
printf '#!/bin/sh\nexit 0\n' > "$WORKDIR/bin/resolvectl"
# The namespace owns its routes; no privilege escalation is needed or allowed in this test.
printf '#!/bin/sh\nexit 1\n' > "$WORKDIR/bin/pkexec"
chmod +x "$WORKDIR/bin/"*
g++ -std=c++20 -fPIC $(pkg-config --cflags Qt6Core Qt6Network) -I"$ROOT/src" \
  "$ROOT/tools/tests/xray_routes.cpp" "$ROOT/src/XrayTunLinux.cpp" \
  -o "$WORKDIR/test_xray_routes" $(pkg-config --libs Qt6Core Qt6Network)
DATAGATE_TEST_PARENT_NET_NS="$(readlink /proc/self/ns/net)" PATH="$WORKDIR/bin:$PATH" \
  unshare --user --map-root-user --net "$WORKDIR/test_xray_routes"
