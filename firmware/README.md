# Firmware

| Folder | What | Status |
|---|---|---|
| `stepper_tester/` | The board firmware: power-up self test, tester UI, menus. ATmega328P via MiniCore, no libraries. | **current**, v1.0 (2026-10-12) |
| `bringup/` | Three Arduino Uno breadboard sketches from before the boards arrived (OLED, encoder, driver UART). | superseded, kept for breadboard debugging |

- What the firmware does: `docs/firmware.md`
- How to flash it: `docs/build-and-flash.md`
- Headless build and the simulator: `tools/build.sh`, `tools/sim/`

## Changing the firmware

1. Edit `stepper_tester/stepper_tester.ino` (keep it a single file; the folder name must match the sketch name for the Arduino IDE).
2. `tools/build.sh`: zero warnings; note flash and RAM.
3. `tools/sim/run_all.sh`: all scenarios must match. Add a scenario when you add a self-test case or a fault path.
4. For a release: bump `FW_VER` and the version/date line in the header comment, add a `CHANGELOG.md` entry, commit, tag `fw-vX.Y`.
5. Flash a real board and record the self-test result in `STATUS.md`. The simulator can't prove real driver behaviour.
