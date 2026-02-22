"""Example Statio plugin: exposes system uptime metrics."""

from __future__ import annotations

from pathlib import Path

PLUGIN_NAME = "uptime"


def collect(snapshot: dict) -> dict:
    _ = snapshot
    try:
        uptime_line = Path('/proc/uptime').read_text(encoding='utf-8').split()[0]
        uptime_seconds = float(uptime_line)
    except Exception:
        uptime_seconds = 0.0

    uptime_minutes = int(uptime_seconds // 60)
    uptime_hours = round(uptime_seconds / 3600.0, 2)

    return {
        'uptime_seconds': int(uptime_seconds),
        'uptime_minutes': uptime_minutes,
        'uptime_hours': uptime_hours,
    }
