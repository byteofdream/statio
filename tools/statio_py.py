#!/usr/bin/env python3
"""Statio Python companion: lightweight system snapshot, watch mode, and plugins."""

from __future__ import annotations

import argparse
import importlib.util
import json
import os
import platform
import socket
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


def _read_first_line(path: str) -> str:
    try:
        return Path(path).read_text(encoding="utf-8").splitlines()[0].strip()
    except Exception:
        return ""


def _read_kv_file(path: str, delimiter: str = ":") -> Dict[str, str]:
    data: Dict[str, str] = {}
    try:
        for line in Path(path).read_text(encoding="utf-8").splitlines():
            if delimiter not in line:
                continue
            key, value = line.split(delimiter, 1)
            data[key.strip()] = value.strip()
    except Exception:
        pass
    return data


def collect_os() -> Dict[str, str]:
    release = _read_kv_file("/etc/os-release", "=")
    pretty = release.get("PRETTY_NAME", "").strip('"')
    version = release.get("VERSION_ID", "").strip('"')
    return {
        "distro": pretty,
        "version": version,
        "kernel": platform.release(),
        "architecture": platform.machine(),
        "hostname": socket.gethostname(),
    }


def collect_cpu() -> Dict[str, object]:
    model = ""
    physical_cores = 0
    mhz = 0.0

    try:
        for raw in Path("/proc/cpuinfo").read_text(encoding="utf-8").splitlines():
            if ":" not in raw:
                continue
            key, value = [part.strip() for part in raw.split(":", 1)]
            if key == "model name" and not model:
                model = value
            elif key == "cpu cores" and physical_cores == 0:
                try:
                    physical_cores = int(value)
                except ValueError:
                    pass
            elif key == "cpu MHz" and mhz == 0.0:
                try:
                    mhz = float(value)
                except ValueError:
                    pass
    except Exception:
        pass

    return {
        "model": model,
        "logical_threads": os.cpu_count() or 0,
        "physical_cores": physical_cores,
        "current_mhz": mhz,
    }


def collect_memory() -> Dict[str, int]:
    out = {
        "total_mb": 0,
        "free_mb": 0,
        "available_mb": 0,
        "swap_total_mb": 0,
        "swap_free_mb": 0,
    }

    data = _read_kv_file("/proc/meminfo", ":")

    def kb_to_mb(field: str) -> int:
        value = data.get(field, "0 kB").split()[0]
        try:
            return int(value) // 1024
        except ValueError:
            return 0

    out["total_mb"] = kb_to_mb("MemTotal")
    out["free_mb"] = kb_to_mb("MemFree")
    out["available_mb"] = kb_to_mb("MemAvailable")
    out["swap_total_mb"] = kb_to_mb("SwapTotal")
    out["swap_free_mb"] = kb_to_mb("SwapFree")
    return out


def _useful_mount(mount_point: str, source: str, fs_type: str, options: str) -> bool:
    allowed = {"/", "/home", "/boot", "/boot/efi", "/var", "/opt", "/mnt", "/media", "/srv"}
    pseudo = {
        "proc",
        "sysfs",
        "tmpfs",
        "devtmpfs",
        "cgroup",
        "cgroup2",
        "overlay",
        "squashfs",
        "devpts",
        "securityfs",
        "pstore",
        "mqueue",
        "tracefs",
        "fusectl",
    }
    if mount_point not in allowed:
        return False
    if fs_type in pseudo:
        return False
    if not source.startswith("/dev/"):
        return False
    if "bind" in options:
        return False
    return True


def collect_disks() -> List[Dict[str, object]]:
    disks: List[Dict[str, object]] = []
    seen = set()
    try:
        lines = Path("/proc/mounts").read_text(encoding="utf-8").splitlines()
    except Exception:
        return disks

    for line in lines:
        parts = line.split()
        if len(parts) < 4:
            continue
        source, mount_point, fs_type, options = parts[:4]
        if mount_point in seen:
            continue
        if not _useful_mount(mount_point, source, fs_type, options):
            continue

        try:
            usage = os.statvfs(mount_point)
            total = usage.f_blocks * usage.f_frsize
            free = usage.f_bavail * usage.f_frsize
        except Exception:
            continue

        seen.add(mount_point)
        disks.append(
            {
                "mount_point": mount_point,
                "filesystem": fs_type,
                "total_gb": total // (1024**3),
                "free_gb": free // (1024**3),
            }
        )

    disks.sort(key=lambda d: str(d["mount_point"]))
    return disks


def collect_network() -> List[Dict[str, object]]:
    net: List[Dict[str, object]] = []
    net_dir = Path("/sys/class/net")
    if not net_dir.exists():
        return net

    for iface in sorted(p.name for p in net_dir.iterdir() if p.is_dir()):
        rx = _read_first_line(f"/sys/class/net/{iface}/statistics/rx_bytes")
        tx = _read_first_line(f"/sys/class/net/{iface}/statistics/tx_bytes")
        mac = _read_first_line(f"/sys/class/net/{iface}/address")

        try:
            rx_bytes = int(rx) if rx else 0
        except ValueError:
            rx_bytes = 0
        try:
            tx_bytes = int(tx) if tx else 0
        except ValueError:
            tx_bytes = 0

        net.append(
            {
                "name": iface,
                "ipv4": "",
                "mac": mac,
                "rx_bytes": rx_bytes,
                "tx_bytes": tx_bytes,
            }
        )

    return net


def collect_gpu() -> List[Dict[str, object]]:
    gpus: List[Dict[str, object]] = []
    for index in range(8):
        vendor = _read_first_line(f"/sys/class/drm/card{index}/device/vendor")
        if not vendor:
            continue
        gpus.append({"adapter": f"card{index} vendor={vendor}", "detected": True})

    if not gpus:
        gpus.append({"adapter": "No GPU details (platform-specific collector needed)", "detected": False})
    return gpus


def collect_snapshot() -> Dict[str, object]:
    return {
        "timestamp": int(time.time()),
        "os": collect_os(),
        "cpu": collect_cpu(),
        "memory": collect_memory(),
        "disks": collect_disks(),
        "network": collect_network(),
        "gpus": collect_gpu(),
    }


def _load_plugin_module(plugin_path: Path) -> Tuple[str, Optional[Any], Optional[str]]:
    module_name = f"statio_plugin_{plugin_path.stem}"
    try:
        spec = importlib.util.spec_from_file_location(module_name, plugin_path)
        if spec is None or spec.loader is None:
            return plugin_path.stem, None, "failed to create module spec"
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        plugin_name = getattr(module, "PLUGIN_NAME", plugin_path.stem)
        return str(plugin_name), module, None
    except Exception as exc:
        return plugin_path.stem, None, str(exc)


def run_plugins(snapshot: Dict[str, object], plugins_dir: Path) -> Tuple[Dict[str, object], Dict[str, str]]:
    plugin_results: Dict[str, object] = {}
    plugin_errors: Dict[str, str] = {}

    if not plugins_dir.exists() or not plugins_dir.is_dir():
        return plugin_results, plugin_errors

    for plugin_path in sorted(plugins_dir.glob("*.py")):
        if plugin_path.name.startswith("_"):
            continue

        plugin_name, module, load_error = _load_plugin_module(plugin_path)
        if load_error is not None:
            plugin_errors[plugin_name] = f"load error: {load_error}"
            continue

        collect_fn = getattr(module, "collect", None)
        if not callable(collect_fn):
            plugin_errors[plugin_name] = "plugin must expose callable collect(snapshot)"
            continue

        try:
            plugin_results[plugin_name] = collect_fn(snapshot)
        except Exception as exc:
            plugin_errors[plugin_name] = f"runtime error: {exc}"

    return plugin_results, plugin_errors


def print_human(snapshot: Dict[str, object]) -> None:
    os_info = snapshot["os"]
    cpu = snapshot["cpu"]
    mem = snapshot["memory"]

    print("Statio Python Snapshot")
    print("=====================")
    print(f"Host: {os_info['hostname']}")
    print(f"OS: {os_info['distro']}")
    print(f"Kernel: {os_info['kernel']}")
    print(f"CPU: {cpu['model']}")
    print(
        f"RAM: total={mem['total_mb']}MB free={mem['free_mb']}MB available={mem['available_mb']}MB "
        f"swap={mem['swap_free_mb']}/{mem['swap_total_mb']}MB"
    )
    print(f"Disks: {len(snapshot['disks'])} | Network IFs: {len(snapshot['network'])} | GPUs: {len(snapshot['gpus'])}")

    plugins = snapshot.get("plugins", {})
    plugin_errors = snapshot.get("plugin_errors", {})
    if plugins:
        print("\nPlugins:")
        for name, value in plugins.items():
            preview = str(value)
            if len(preview) > 120:
                preview = preview[:117] + "..."
            print(f"- {name}: {preview}")

    if plugin_errors:
        print("\nPlugin errors:")
        for name, message in plugin_errors.items():
            print(f"- {name}: {message}")


def build_snapshot_with_plugins(plugins_dir: Path, enable_plugins: bool) -> Dict[str, object]:
    snapshot = collect_snapshot()
    snapshot["plugins"] = {}
    snapshot["plugin_errors"] = {}

    if enable_plugins:
        plugin_results, plugin_errors = run_plugins(snapshot, plugins_dir)
        snapshot["plugins"] = plugin_results
        snapshot["plugin_errors"] = plugin_errors

    return snapshot


def main() -> int:
    parser = argparse.ArgumentParser(description="Statio Python companion")
    parser.add_argument("--json", action="store_true", help="Output JSON")
    parser.add_argument("--pretty", action="store_true", help="Pretty-print JSON")
    parser.add_argument("--watch", type=float, default=0.0, help="Refresh every N seconds")
    parser.add_argument("--no-plugins", action="store_true", help="Disable plugin loading")
    parser.add_argument(
        "--plugins-dir",
        default=str(Path(__file__).resolve().parent / "plugins"),
        help="Directory that contains Python plugins (*.py)",
    )
    args = parser.parse_args()

    plugins_dir = Path(args.plugins_dir)

    def emit() -> None:
        snapshot = build_snapshot_with_plugins(plugins_dir=plugins_dir, enable_plugins=not args.no_plugins)
        if args.json:
            if args.pretty:
                print(json.dumps(snapshot, indent=2, ensure_ascii=False))
            else:
                print(json.dumps(snapshot, separators=(",", ":"), ensure_ascii=False))
        else:
            print_human(snapshot)

    if args.watch > 0:
        try:
            while True:
                os.system("clear")
                emit()
                time.sleep(args.watch)
        except KeyboardInterrupt:
            return 0
    else:
        emit()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
