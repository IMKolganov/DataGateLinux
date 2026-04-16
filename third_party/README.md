# Third-party sources for embedded OpenVPN 3

This repo includes **git submodules** (see `.gitmodules` in the repository root):

| Path | Upstream |
|------|----------|
| `third_party/openvpn3` | [OpenVPN/openvpn3](https://github.com/OpenVPN/openvpn3) — pinned to tag `release/3.11.6` (adjust intentionally when upgrading). |
| `third_party/asio` | [chriskohlhoff/asio](https://github.com/chriskohlhoff/asio) — pinned to tag `asio-1-38-0`. |

After clone:

```bash
git submodule update --init --recursive
```

Or clone once with: `git clone --recurse-submodules <url>`.

## Do we build OpenVPN 3 inside DataGateLinux?

**Yes.** CMake builds a static library `datagate_ovpn3_core` from the OpenVPN 3 **sources** and links it into `DataGateLinux`. This is not the system `/usr/sbin/openvpn` (OpenVPN 2) and not a prebuilt `.so` from a distro package — the C++ core is compiled into your binary (same idea as DataGateAndroid’s `native-openvpn3`).

## Do we have to add OpenVPN’s Git repo to *this* repo?

**Not strictly.** You only need the trees on disk when you run CMake (clone, submodule, symlink, or CI checkout). Nothing forces you to commit OpenVPN inside DataGateLinux.

**Recommended for reproducible builds and CI:**

- Add **git submodule** to a **fixed tag or commit** of [OpenVPN/openvpn3](https://github.com/OpenVPN/openvpn3), not floating `master`:

  ```bash
  git submodule add -b master https://github.com/OpenVPN/openvpn3.git third_party/openvpn3
  cd third_party/openvpn3 && git checkout v3.12.0   # example: pin exact tag
  cd ../.. && git add third_party/openvpn3 && git commit -m "Pin openvpn3 submodule"
  ```

  Or submodule at a **specific commit** after `git checkout <sha>` inside the submodule.

- Same for **Asio** (header-only), or vendor a known release under `third_party/asio/`.

Then every clone runs `git submodule update --init --recursive` and gets the same sources.

**Alternatives:**

- **Symlink** to your DataGateAndroid tree (fine for local dev; bad for others/CI unless documented).
- **Shallow clone** in CI before `cmake` (pin `git clone --branch v3.12.0 --depth 1`).
- **FetchContent** in CMake with `GIT_TAG` pinned (first configure may fetch from GitHub).

## What if upstream OpenVPN 3 changes something?

Without a pin, `master` can break your build or change wire behaviour.

**Policy:**

1. **Pin** a tag or commit (submodule or FetchContent `GIT_TAG`).
2. **Upgrade deliberately**: bump submodule / tag, rebuild, run VPN smoke tests, then commit the new pointer.
3. Keep **DataGateAndroid** and **DataGateLinux** on the **same** OpenVPN 3 revision if you want identical protocol/core behaviour.

Optional: document the pinned revision in the main README or `CHANGELOG` when you bump it.

## Layout

- **`openvpn3/`** — OpenVPN 3 core tree (`client/`, `openvpn/`, …).
- **`asio/asio/include`** — standalone Asio so `#include <asio.hpp>` works (same layout as DataGateAndroid `native-openvpn3/third_party/asio`).

Quick local setup (no submodule):

```bash
cd third_party
git clone --depth 1 --branch v3.12.0 https://github.com/OpenVPN/openvpn3.git openvpn3
# Asio: unpack release or symlink from DataGateAndroid native-openvpn3/third_party/asio
```

Symlink from Android (developer machine):

```bash
ln -sfn /path/to/DataGateAndroid/native-openvpn3/openvpn3 third_party/openvpn3
ln -sfn /path/to/DataGateAndroid/native-openvpn3/third_party/asio third_party/asio
```

CMake overrides: `-DDATAGATE_OPENVPN3_ROOT=` and `-DDATAGATE_ASIO_INCLUDE_DIR=` if sources live elsewhere.

## `.gitignore`

Local symlinks or unpacked trees are often listed in `.gitignore` so each developer/CI supplies their own `third_party/openvpn3` and `third_party/asio`, **unless** you use submodules (then you **commit** the submodule pointer and do not ignore `third_party/openvpn3`).
