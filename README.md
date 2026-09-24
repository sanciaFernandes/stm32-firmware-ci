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

## Layout
```
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
make cppcheck        # static analysis
make clean
```

Requires `arm-none-eabi-gcc` and `make` for the firmware build; the unit tests
build with the host `gcc`.

## Application logic
The firmware models a motorised seatbelt tensioner:

* `belt_ramp()` — linear pull ramp, clamped so the commanded duty can never
  exceed full pull even if the elapsed time overruns
* `belt_comfort_duty()` — three tension levels, with an out-of-range mode
  falling back to "no pull"
* `belt_next_mode()` — OFF → SOFT → MED → HARD → OFF button cycle

Those functions are deliberately free of hardware access so they can be tested
on the host, which is what the CI test job does.

## Traceability
```c
const char fw_version[] = FW_VERSION;   /* e.g. "1.0.0"   */
const char git_hash[]   = GIT_HASH;     /* e.g. "a1b2c3d" */
```
Both are injected by the Makefile at compile time, so a binary recovered from a
device can be matched to the exact commit that produced it.
