# DataGateLinux

Desktop client for **DataGate** on Linux: Google sign-in, dashboard API, and OpenVPN (embedded OpenVPN 3 core or system `openvpn`, depending on build). Companion to [DataGateWin](https://github.com/IMKolganov/DataGateWin) / mobile apps where applicable.

**License:** [Mozilla Public License 2.0](LICENSE) (MPL-2.0).

## Requirements

- **Linux** (x86_64 tested)
- **CMake** ≥ 3.16, **C++20** compiler
- **Qt 6** (`Core`, `Gui`, `Widgets`, `Network`, `WebSockets`)
- Optional: **qt6-l10n-tools** (`lrelease`) if you regenerate `.qm` from `.ts`
- Embedded OpenVPN 3 build: **git submodules** (see below) plus usual build deps (e.g. OpenSSL, LZ4 — resolved by CMake on typical distros)

## Clone

```bash
git clone --recurse-submodules https://github.com/IMKolganov/DataGateLinux.git
cd DataGateLinux
```

If you already cloned without submodules:

```bash
git submodule update --init --recursive
```

Submodule layout and pinning policy: **[third_party/README.md](third_party/README.md)**.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Artifacts (default paths):

- `build/DataGateLinux` — main UI
- `build/DataGateOvpn3Helper` — helper process when embedded OpenVPN 3 uses the external-helper model on Linux

If `third_party/openvpn3` or `third_party/asio` is missing, CMake turns **off** the embedded OpenVPN 3 option; install submodules or see `third_party/README.md`.

## Configuration (`appsettings.json`)

The app reads **`appsettings.json`** from the **first path that exists**, in order:

1. **Current working directory** (`./appsettings.json`) — IDEs often set cwd to `build/` or `cmake-build-debug/`, not the repo root.
2. **Directory of the executable** — after a local build, this is usually `build/`.
3. **`QStandardPaths::AppConfigLocation`/appsettings.json** — per-user config location.

On success, the log line **`AppConfig: loaded <path>`** shows which file was used (enable logging as in your build, e.g. `DATAGATE_LOG` / Debug where applicable).

**Template:** start from **`appsettings.example.json`** — it ships **production** defaults (`Api:BaseUrl`, Google Desktop **`ClientId`**, OpenVPN and Update options). For a **self-hosted** dashboard or your own OAuth client, override `Api` / `GoogleAuth` (and any OpenVPN flags you need).

**After editing `appsettings.json` in the repo root:** CMake **POST_BUILD** can copy/sync it next to the binary (`cmake/SyncAppsettingsFromRepo.cmake`). **Rebuild** (or copy the file beside the binary by hand) so the running app picks up changes; otherwise you may still be loading an old file from `build/`.

**Local overrides:** copy the example to **`appsettings.json`** if you need a different config; that file is **gitignored**. The OAuth **client ID** in the example is a public identifier (not the client secret).

### Optional: update check (GitHub Releases)

Same idea as DataGateWin: optional startup check against  
`GET https://api.github.com/repos/{owner}/{repo}/releases/latest`.

Configure under **`Update`** in `appsettings.json` (see **`appsettings.example.json`**):

- `GitHubOwner`, `GitHubRepo` — repository for releases  
- `CheckOnStartup` — `false` to disable  

The running app version comes from CMake **`project(... VERSION ...)`** (see top of `CMakeLists.txt`). Tag names like `v1.0.0` are compared after stripping a leading `v`.

## Translations

UI strings are driven by **`tools/catalog.jsonl`** and embedded `.ts` resources. If you change user-visible text, update the catalog / translation workflow described under `tools/` (e.g. `build_catalog_jsonl.py`, `gen_datagate_ts.py`) so all locales stay in sync.

## Releases / local packaging

Release tarballs or unpacked trees are often placed under **`dist/`** — that directory is **gitignored** so local publish artifacts are not committed.

## Contributing

Pull requests are welcome. Match existing code style; keep `appsettings.json` out of commits; for i18n, follow the catalog-based workflow above.
