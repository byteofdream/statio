# Statio

`Statio` is a C++ system diagnostics tool inspired by AIDA64-class utilities and built from scratch.

## Features

- Collects OS details (distribution, version, kernel, architecture, hostname)
- Collects CPU details (model, physical cores, logical threads, current MHz)
- Collects memory details (RAM and swap)
- Shows main mounted disk entries and capacity data
- Shows network interfaces and traffic counters (when available)
- Shows basic GPU adapter data from `/sys/class/drm`
- Provides both CLI and Qt GUI modes

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

- `statio` (CLI) is always built.
- `statio-qt` is built automatically when `Qt5/Qt6 Widgets` is available.

Disable GUI build if needed:

```bash
cmake -S . -B build -DBUILD_QT_GUI=OFF
cmake --build build -j
```

## Run

CLI:

```bash
./build/statio
```

GUI:

```bash
./build/statio-qt
```

Python companion:

```bash
python3 tools/statio_py.py
python3 tools/statio_py.py --json --pretty
python3 tools/statio_py.py --watch 2
```

Disable plugins:

```bash
python3 tools/statio_py.py --no-plugins
```

Use a custom plugins folder:

```bash
python3 tools/statio_py.py --plugins-dir /path/to/plugins
```

## Qt GUI

Current `statio-qt` interface includes:

- Tabs: `Overview`, `CPU`, `Memory`, `Disks`, `Network`, `GPU`
- Light theme (black text with clean black component outlines)
- Dark theme switch in `Settings -> Theme`
- Structured tables instead of a single text dump
- `Refresh Now` button
- Auto-refresh every 5 seconds
- `Help -> About Statio` dialog

## Project Structure

- `include/statio/system_info.hpp` - data models and public API
- `src/system_info.cpp` - system telemetry collectors and report rendering
- `src/main.cpp` - CLI entry point
- `include/statio/main_window.hpp` + `src/main_window.cpp` - Qt GUI
- `src/main_qt.cpp` - GUI entry point
- `tools/statio_py.py` - Python snapshot/watch utility (no third-party dependencies)
- `tools/plugins/*.py` - optional Python plugins for extra collectors

## Python Plugins

`tools/statio_py.py` auto-loads plugins from `tools/plugins` by default.

Plugin contract:

- A plugin is a `.py` file.
- Optional: `PLUGIN_NAME = \"my_plugin\"`
- Required: `def collect(snapshot: dict) -> object`
- `snapshot` is the already-collected base Statio data.
- Return any JSON-serializable object (dict/list/number/string/bool).

Example plugin is included:

- `tools/plugins/uptime_plugin.py`

## Roadmap

- Add Windows backend (WMI + WinAPI)
- Add richer sensors (temperatures, SMART, hardware monitoring)
- Add JSON/CSV export
- Add benchmark module
