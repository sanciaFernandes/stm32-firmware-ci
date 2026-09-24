# STM32 Firmware Build & Release Pipeline

Bare-metal firmware for the **STM32F446RE (ARM Cortex-M4)** with a complete
build, verification and release pipeline — no IDE required.

The point of this repo is not the blinking LED. It is everything around it:
reproducible command-line builds, automated verification, traceable binaries
and published releases.

## What it demonstrates

| Area | How |
|---|---|
| Command-line build | `Makefile` using `arm-none-eabi-gcc`, custom linker script and startup code (no HAL, no CubeIDE) |
| Continuous integration | GitHub Actions: build + unit tests + static analysis on every push and pull request |
| Verification | Unity unit tests for the control logic, run on the host; `cppcheck` static analysis; `-Werror` |
| Configuration management | Firmware version and **git commit hash compiled into the binary**, so any build traces back to its source |
| Release management | Semantic version tags (`v1.0.0`) trigger a release build with binaries and auto-generated notes |
| Artefacts | `.bin`, `.hex` and the linker `.map` uploaded from every CI run |
| Size budgets | `tools/size_budget.py` fails the build if the image outgrows its flash or RAM budget |
| Test automation | `tools/hil_runner.py` (Python) drives scenario files against a simulated device, or a real board over SWD/JTAG, and emits JUnit XML |

## Layout
```
tools/hil_runner.py      Python HIL runner: sim or SWD/JTAG backend, JUnit output
tools/size_budget.py     flash/RAM budget check, fails the build when exceeded
tests/scenarios.json     backend-agnostic test scenarios
tests/sim_device.c       the simulator as a device with a line protocol
src/startup.c            vector table, .data copy, .bss zeroing, calls main()
src/main.c               register-level GPIO, no HAL
src/belt.c, inc/belt.h   belt tensioning logic (hardware-free, unit tested)
linker/STM32F446RE.ld    512 KB flash @ 0x08000000, 128 KB RAM @ 0x20000000
tests/test_belt.c        Unity tests: ramp, clamping, mode cycle, safe defaults
.github/workflows/ci.yml build / test / static-analysis / release jobs
```

## Build
```bash
make                 # build .elf, .bin, .hex and print flash/RAM usage
make test            # host unit tests (no board needed)
make sim             # scenario tests (Unity, host)
make hil             # Python HIL runner against the simulated device
make size-check      # enforce the flash/RAM budget
make cppcheck        # static analysis
make clean
```

Requires `arm-none-eabi-gcc` and `make` for the firmware build; the unit tests
build with the host `gcc`.

## Application logic: CEMS, a motorised seatbelt controller

Five functions, with the interlocks between them enforced by the state machine
rather than by convention:

| # | Function | Type | When | Behaviour |
|---|---|---|---|---|
| 1 | **BLA** - Belt Latch Assist | comfort | unlatched | The occupant starts pulling the webbing; the tachometer shows extraction, so the motor feeds webbing out **in proportion to how fast they pull**, making it effortless. Stops the moment the pull stops. |
| 2 | **BSR** - Belt Slack Reduction | **safety** | on latching | Removes webbing slack over 3 s so the belt sits active against the body. Exclusive and one-shot: nothing may interrupt it, and it cannot re-run without an unlatch. |
| 3 | **Comfort modes** | comfort | latched | The same speed-following law as BLA with different gains: **Soft** feeds webbing out most readily (long journeys), **Hard** least (rough terrain, so the occupant does not pitch forward under braking), **Medium** between. With no movement the motor is off. |
| 4 | **Haptic warning** | **safety** | latched | A toggle button on the prototype (drowsiness signal from the vehicle ECU in the car) makes the motor vibrate for 5 s, then it returns to the comfort mode that was active before. |
| 5 | **Belt Parking** | comfort | on unlatching | Retracts the webbing smoothly instead of letting the spring snap it back. |

Overcurrent above 12 A stops the motor from any state and latches a **fault**,
which clears only when the belt is released.

```
                    +--------------------+
              +---->|     UNLATCHED      |<------------+
              |     +---+------------+---+             |
              |  pulling|            |latched          |park complete
              |         v            v                 |
              |     +-------+    +-------+        +----------+
   pull stops +-----|  BLA  |--->|  BSR  |        | PARKING  |
                    +-------+    | 3 s   |        +----------+
                                 |exclusive|            ^
                                 +---+-----+            | unlatched
                                     | after 3 s        | (from any state)
                                     v                  |
                                 +---------+            |
                                 | COMFORT |------------+
                                 | S/M/H   |
                                 +----+----+
                          haptic button|  ^ after 5 s
                                      v  |
                                 +---------+
                                 | HAPTIC  |
                                 +---------+
```

### The control law
BLA and all three comfort modes share one rule:

```
tacho    = tachometer reading this sample, clamped to [1, 30]
duty_pct = initial % + (factor(mode) % per count * tacho)
duty     = duty_pct / 100     /* the motor controller takes 0..1 */

/* BLA/Soft  +5% .. +20%   assists extraction
   Medium    +5% .. +14%
   Hard      -5% ..  -8%   NEGATIVE: the motor pulls back, so extraction is
                           deliberately hard and the occupant cannot pitch
                           forward under braking */
```

The initial duty gives a base amount of help at the slowest pull, and the
factor adds more as the pull gets faster. Clamping the reading at both ends
means a very slow pull still gets help, while an unusually fast pull is treated
as the fastest expected pull, so the motor never feeds out excess webbing. With
no movement the motor is off. Each mode is then a single tunable number.

### Why it is built this way
The state machine is **non-blocking**: `cems_step()` performs one step and
returns, so the buckle switch, motor current and timers are re-checked every
10 ms throughout a function rather than being ignored inside a blocking loop.

All hardware access goes through `platform.h`, so the identical logic runs
against real registers on the target and against a scripted fake on the host.
That is what makes the awkward cases testable without a vehicle: unlatching at
peak pull, a haptic request arriving mid-BSR, or the belt jamming.

### Hardware-in-the-loop runner
`tools/hil_runner.py` reads `tests/scenarios.json` and drives one of two
backends through the same steps:

* **simulator** - talks to `build/sim_device` over a line protocol
  (`SET LATCH 1`, `SET PULL 20`, `STEP 100`, `GET STATE`). This runs in CI.
* **hardware** - uses **pyocd** over **SWD/JTAG** to flash the board, halt the
  core, write the input variables, read back the commanded duty and resume.
  Implemented but **not yet exercised**, as no board was available; the
  addresses come from the linker map file.

Both produce JUnit XML, so the same scenarios and the same report format cover
a simulated device today and a bench setup later.

### Tested scenarios
13 scenario tests cover each function plus the interlocks, including:
* BLA assists while pulling, stops when the occupant lets go, never runs latched
* A faster pull produces more assist, and the tacho reading is clamped so an extreme pull gives no more than the limit
* Soft assists more than Medium, and Hard is negative - it resists extraction rather than assisting
* The motor is off when the occupant is not moving
* BSR lasts 3 s, runs once per latch, and runs again after unlatch/relatch
* A haptic request during BSR is ignored, then serviced once BSR completes
* Haptic runs 5 s and restores the previous comfort mode
* Parking runs on release and returns to idle
* Overcurrent latches a fault that does not clear when the current recovers
* The commanded duty never leaves the range [-1.0, +1.0]

## Traceability
```c
const char fw_version[] = FW_VERSION;   /* e.g. "1.0.0"   */
const char git_hash[]   = GIT_HASH;     /* e.g. "a1b2c3d" */
```
Both are injected by the Makefile at compile time, so a binary recovered from a
device can be matched to the exact commit that produced it.
