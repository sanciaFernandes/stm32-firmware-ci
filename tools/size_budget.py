#!/usr/bin/env python3
"""Enforce flash and RAM budgets for the firmware image.

Real projects fail the build when an image outgrows its budget, rather than
discovering it when the device stops fitting or the stack collides with the
heap. This parses arm-none-eabi-size and compares against limits.

    text = code + constants          -> flash
    data = initialised variables     -> flash (copy) and RAM
    bss  = zero-initialised variables-> RAM

    flash used = text + data
    RAM used   = data + bss

Usage:
    python tools/size_budget.py build/firmware.elf --flash-max 65536 --ram-max 20480
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys


def read_sizes(elf: str, size_tool: str) -> tuple[int, int, int]:
    if shutil.which(size_tool) is None:
        raise SystemExit(f"error: {size_tool} not found on PATH")

    output = subprocess.run(
        [size_tool, elf], capture_output=True, text=True, check=True
    ).stdout

    lines = [line for line in output.splitlines() if line.strip()]
    if len(lines) < 2:
        raise SystemExit(f"error: unexpected output from {size_tool}:\n{output}")

    text, data, bss = (int(value) for value in lines[1].split()[:3])
    return text, data, bss


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", help="path to the firmware .elf")
    parser.add_argument("--flash-max", type=int, required=True, help="bytes")
    parser.add_argument("--ram-max", type=int, required=True, help="bytes")
    parser.add_argument("--size-tool", default="arm-none-eabi-size")
    args = parser.parse_args()

    text, data, bss = read_sizes(args.elf, args.size_tool)
    flash_used = text + data
    ram_used = data + bss

    def report(label: str, used: int, limit: int) -> bool:
        pct = (used / limit) * 100.0
        status = "OK" if used <= limit else "OVER BUDGET"
        print(f"  {label:<6} {used:>7} / {limit:>7} bytes  ({pct:5.1f}%)  {status}")
        return used <= limit

    print(f"Size budget for {args.elf}")
    flash_ok = report("flash", flash_used, args.flash_max)
    ram_ok = report("RAM", ram_used, args.ram_max)

    if not (flash_ok and ram_ok):
        print("\nerror: firmware exceeds its size budget", file=sys.stderr)
        return 1

    print("\nwithin budget")
    return 0


if __name__ == "__main__":
    sys.exit(main())
