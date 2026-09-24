#!/usr/bin/env python3
"""Hardware-in-the-loop test runner for the CEMS firmware.

The same scenario file runs against either backend:

  --backend sim        drives the simulated device (tests/sim_device.c) over a
                       line protocol on stdin/stdout. Runs in CI, no hardware.

  --backend hardware   drives a real STM32 board over SWD/JTAG using pyocd:
                       flash, reset/halt, poke the input variables, read back
                       the commanded duty. Implemented but not yet exercised -
                       no board available at the time of writing.

Results are written as JUnit XML so CI can display them.

Usage:
    python tools/hil_runner.py --scenarios tests/scenarios.json \
                               --device build/sim_device \
                               --report build/hil-report.xml
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
from pathlib import Path

TOLERANCE = 0.001


class ScenarioFailure(Exception):
    """Raised when a scenario assertion does not hold."""


# --------------------------------------------------------------------------
# Backends
# --------------------------------------------------------------------------
class SimulatorBackend:
    """Talks to the simulated device over stdin/stdout."""

    name = "simulator"

    def __init__(self, device_path: str):
        self.proc = subprocess.Popen(
            [device_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

    def _command(self, line: str) -> str:
        assert self.proc.stdin and self.proc.stdout
        self.proc.stdin.write(line + "\n")
        self.proc.stdin.flush()
        reply = self.proc.stdout.readline().strip()
        if reply.startswith("ERR"):
            raise ScenarioFailure(f"device rejected {line!r}: {reply}")
        return reply

    # --- operations used by scenarios ---
    def reset(self) -> None:
        self._command("RESET")

    def set_latch(self, closed: int) -> None:
        self._command(f"SET LATCH {int(closed)}")

    def set_pull(self, counts: int) -> None:
        self._command(f"SET PULL {int(counts)}")

    def set_current(self, amps: float) -> None:
        self._command(f"SET CURRENT {float(amps)}")

    def set_haptic(self, pressed: int) -> None:
        self._command(f"SET HAPTIC {int(pressed)}")

    def set_mode(self, mode: str) -> None:
        self._command(f"SET MODE {mode.upper()}")

    def step(self, ms: int) -> None:
        self._command(f"STEP {int(ms)}")

    def state(self) -> str:
        return self._command("GET STATE").split(" ", 1)[1]

    def duty(self) -> float:
        return float(self._command("GET DUTY").split(" ", 1)[1])

    def close(self) -> None:
        try:
            self._command("QUIT")
        except Exception:
            pass
        self.proc.terminate()


class HardwareBackend:
    """Drives a real board over SWD/JTAG with pyocd.

    NOT YET EXERCISED: written against the pyocd API but never run, because no
    board was available. The addresses below come from the linker map file.
    """

    name = "hardware"

    def __init__(self, firmware: str, target: str = "stm32f446retx"):
        try:
            from pyocd.core.helpers import ConnectHelper  # noqa: F401
        except ImportError as exc:  # pragma: no cover - depends on environment
            raise RuntimeError(
                "pyocd is not installed; install it to use the hardware backend"
            ) from exc

        from pyocd.core.helpers import ConnectHelper
        from pyocd.flash.file_programmer import FileProgrammer

        self.session = ConnectHelper.session_with_chosen_probe(
            target_override=target, options={"frequency": 4_000_000}
        )
        self.session.open()
        self.target = self.session.target

        FileProgrammer(self.session).program(firmware)   # flash over SWD
        self.target.reset_and_halt()

        # Symbol addresses, read from build/firmware.map
        self.sym = self._load_symbols()
        self.target.resume()

    @staticmethod
    def _load_symbols() -> dict:  # pragma: no cover - requires a build
        symbols = {}
        mapfile = Path("build/firmware.map")
        for line in mapfile.read_text(errors="ignore").splitlines():
            parts = line.split()
            if len(parts) == 2 and parts[0].startswith("0x"):
                symbols[parts[1]] = int(parts[0], 16)
        return symbols

    # Each operation writes to the variable the firmware polls, then lets the
    # target run for the requested time.
    def reset(self) -> None:                      # pragma: no cover
        self.target.reset_and_halt()
        self.target.resume()

    def set_latch(self, closed: int) -> None:     # pragma: no cover
        self.target.write32(self.sym["sim_latch"], int(closed))

    def set_pull(self, counts: int) -> None:      # pragma: no cover
        self.target.write32(self.sym["sim_pull"], int(counts))

    def set_current(self, amps: float) -> None:   # pragma: no cover
        self.target.write32(self.sym["sim_current_ma"], int(amps * 1000))

    def set_haptic(self, pressed: int) -> None:   # pragma: no cover
        self.target.write32(self.sym["sim_haptic"], int(pressed))

    def set_mode(self, mode: str) -> None:        # pragma: no cover
        order = {"OFF": 0, "SOFT": 1, "MED": 2, "HARD": 3}
        self.target.write32(self.sym["comfort_mode"], order[mode.upper()])

    def step(self, ms: int) -> None:              # pragma: no cover
        time.sleep(ms / 1000.0)                   # the target runs in real time

    def state(self) -> str:                       # pragma: no cover
        names = ["UNLATCHED", "BLA", "BSR", "COMFORT", "HAPTIC", "PARKING", "FAULT"]
        return names[self.target.read32(self.sym["state"])]

    def duty(self) -> float:                      # pragma: no cover
        return self.target.read32(self.sym["last_duty_milli"]) / 1000.0

    def close(self) -> None:                      # pragma: no cover
        self.session.close()


# --------------------------------------------------------------------------
# Scenario execution
# --------------------------------------------------------------------------
def apply_set(backend, values: dict) -> None:
    for key, value in values.items():
        if key == "latch":
            backend.set_latch(value)
        elif key == "pull":
            backend.set_pull(value)
        elif key == "current":
            backend.set_current(value)
        elif key == "haptic":
            backend.set_haptic(value)
        elif key == "mode":
            backend.set_mode(value)
        else:
            raise ScenarioFailure(f"unknown input {key!r}")


def check_expect(backend, expect: dict) -> None:
    if "state" in expect:
        actual = backend.state()
        if actual != expect["state"]:
            raise ScenarioFailure(
                f"expected state {expect['state']}, device is in {actual}"
            )

    if any(k in expect for k in ("duty", "duty_above", "duty_below")):
        duty = backend.duty()
        if "duty" in expect and abs(duty - expect["duty"]) > TOLERANCE:
            raise ScenarioFailure(f"expected duty {expect['duty']}, got {duty}")
        if "duty_above" in expect and duty <= expect["duty_above"]:
            raise ScenarioFailure(
                f"expected duty above {expect['duty_above']}, got {duty}"
            )
        if "duty_below" in expect and duty >= expect["duty_below"]:
            raise ScenarioFailure(
                f"expected duty below {expect['duty_below']}, got {duty}"
            )


def run_scenario(backend, scenario: dict) -> None:
    backend.reset()
    for step in scenario["steps"]:
        if "set" in step:
            apply_set(backend, step["set"])
        if "step_ms" in step:
            backend.step(step["step_ms"])
        if "expect" in step:
            check_expect(backend, step["expect"])


def write_report(path: Path, results: list) -> None:
    suite = ET.Element(
        "testsuite",
        name="cems-hil",
        tests=str(len(results)),
        failures=str(sum(1 for r in results if r["failure"])),
    )
    for result in results:
        case = ET.SubElement(
            suite,
            "testcase",
            classname="hil",
            name=result["name"],
            time=f"{result['seconds']:.3f}",
        )
        if result["failure"]:
            failure = ET.SubElement(case, "failure", message=result["failure"])
            failure.text = result["failure"]
    path.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(suite).write(path, encoding="utf-8", xml_declaration=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenarios", default="tests/scenarios.json")
    parser.add_argument("--backend", choices=["sim", "hardware"], default="sim")
    parser.add_argument("--device", default="build/sim_device")
    parser.add_argument("--firmware", default="build/firmware.bin")
    parser.add_argument("--report", default="build/hil-report.xml")
    args = parser.parse_args()

    scenarios = json.loads(Path(args.scenarios).read_text())["scenarios"]

    if args.backend == "sim":
        backend = SimulatorBackend(args.device)
    else:
        backend = HardwareBackend(args.firmware)

    print(f"HIL runner: {len(scenarios)} scenarios on the {backend.name} backend\n")

    results = []
    for scenario in scenarios:
        started = time.time()
        failure = None
        try:
            run_scenario(backend, scenario)
        except ScenarioFailure as exc:
            failure = str(exc)
        elapsed = time.time() - started

        results.append(
            {"name": scenario["name"], "failure": failure, "seconds": elapsed}
        )
        mark = "FAIL" if failure else "PASS"
        print(f"  [{mark}] {scenario['name']}")
        if failure:
            print(f"         {failure}")

    backend.close()
    write_report(Path(args.report), results)

    failed = sum(1 for r in results if r["failure"])
    print(f"\n{len(results) - failed}/{len(results)} scenarios passed")
    print(f"report: {args.report}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
