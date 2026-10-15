# Board simulator

Runs the real compiled firmware (`build/minicore/fw.elf`) on a simulated ATmega328P at 16 MHz ([simavr](https://github.com/buserror/simavr)) wired to software models of the rest of the board, so a firmware change can be regression-tested before it touches a chip. The whole suite takes under a minute.

## What's modelled (`sim.c`)

- **TMC2209** on the single-wire UART: echo of every byte sent, datagrams with CRC, register file (GCONF, CHOPCONF, IHOLD_IRUN, VACTUAL, IOIN, DRV_STATUS, SG_RESULT ...), address set by MS1/MS2, version 0x21.
- **SSD1306** on I2C: command and data decoding into a frame buffer, dumped as 128x64 `.pgm` screen images.
- **VM** on ADC0 through the 10k / 1.8k divider (AVCC 5.000 V), changeable mid-run.
- **Encoder** A/B and switch on PD2/PD3/PD4 (detent-accurate quadrature), **DIAG** on PB0.
- **Debug log** decoder on PB2 (57600 baud, bit timing checked), written to `<scenario>.log`.
- **STEP** pulse counter on OC1A (PB1) and status-LED edge capture on PD5; worst-case stack depth.

## Scenarios

| Scenario | Injected condition | Must report |
|---|---|---|
| happy | good board, 12 V, motor | 0 fail; then UI run: start/stop, +2 clicks, push+turn, reverse, menu, coil check, register view |
| step | same, Drive = STEP pin | 0 fail; steady 3,200.00 Hz at 60 rpm, 6,400.00 Hz at 120 rpm |
| steplow | full step, STEP pin, 1 rpm | 0 fail; 3.333 Hz, then 6.667 Hz after one click |
| novm | no 12 V at boot, brick plugged in later | VM warning; self test re-runs once VM appears |
| addr2 | MS2 not grounded (driver at address 2) | LINK fail, "addr 2" |
| noecho | TX path broken (no echo) | UART fail |
| linelow | PDN line held low | UART fail |
| notmc | no driver module | LINK fail |
| nodiag | DIAG not wired | DIAG warning |
| enbroken / dirbroken / stepbroken | EN / DIR / STEP trace open | PINS fail naming the pin |
| nooled | no display | OLED fail; status LED blinks the failed test number |
| short | output short to GND (coil A) | OUT fail |
| tmc2208 | version 0x20 | DRV fail |
| nomotor | no motor | coil check: both coils open |
| coilA | coil A open | coil check: coil A open |
| vmdrop | VM sags to 7 V while running | VM LOW fault, driver disabled, speed zero |
| holdboot | knob held at power-up | defaults restored |
| clk8 | chip running at 8 MHz (fuses not burned) | CLK fail |

The expected results are encoded in `run_all.sh`; it exits 1 if any scenario differs.

## Running it

```
tools/fetch_deps.sh sim      # once: pinned ArduinoCore-avr, MiniCore, simavr
tools/sim/setup.sh           # once: patch + build simavr, build build/sim/sim2
tools/sim/run_all.sh         # build firmware, run all 20, compare
tools/sim/run_all.sh happy step    # just some
python3 tools/sim/sheet.py   # refresh docs/images/sim-screens.png
python3 tools/sim/render.py /tmp/x.png build/sim/out/happy_*.pgm   # any screens
```

Outputs land in `build/sim/out/` (gitignored): `<scenario>.stdout` (harness summary), `<scenario>.log` (the firmware's own debug log, decoded), and `.pgm` screen dumps. Environment knobs: `SIM_OUT` (output directory), `UARTV=1` (print UART traffic), `PCT=<ms>` (print the program counter every 50 ms from that time, for hangs).

Needs gcc, make, `libelf-dev`, `pkg-config`, Python 3 with Pillow, plus the AVR toolchain for the firmware build.

## The simavr patch

`simavr-icr1-write.patch` makes a write to ICR1 take effect immediately when Timer1 runs with TOP = ICR1, as on real hardware. Stock simavr only re-reads TOP when the timer mode changes, which made the STEP-mode frequency changes look broken in simulation. It applies to simavr commit `557b074` (pinned in `tools/fetch_deps.sh`).

## Adding a scenario

1. Add a knob to the `sc` struct or a step sequence in `main()` keyed on the scenario name (see `vmdrop` or `steplow`).
2. Add a line to `EXPECT` in `run_all.sh`: `name|result: N fail, M warn.|TEST=detail`.
3. Run it, read `<name>.log`, render the screens if the UI matters.

## Limits

The models follow the datasheets, not real silicon. Not covered: real open-load and StallGuard behaviour, analog effects on the ribbon (the OLED at 400 kHz), mechanical knob direction, and anything about the motor itself. A pass here means the firmware does what it was designed to do; only the real board can say the design was right.

## History

Written in the firmware chat on 2026-10-12 and recovered from its transcript (`docs/history/sources.md`). Only change for the repo: output and ELF paths are relative (`SIM_OUT`, `build/minicore/fw.elf`) instead of absolute workspace paths.
