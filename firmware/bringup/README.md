# Breadboard bring-up sketches (superseded)

Written 2026-10-06 for a genuine Arduino Uno on a breadboard, before the boards arrived, as a step-by-step ladder. Steps 1 and 2 were run and worked; step 3 was wired on paper but never run (the 12 V brick's AC cord was missing). The board firmware's power-up self test replaced the ladder.

Kept because they're still handy for checking a module, display or encoder on its own. Each sketch's header comment has its wiring. Board: Arduino Uno; Serial Monitor 115200.

| Sketch | Tests | Libraries |
|---|---|---|
| `step1_oled/` | I2C scan + text on the OLED (SDA A4, SCL A5) | U8g2 |
| `step2_encoder/` | encoder counts and button (A D2, B D3, switch D4, C to GND) | U8g2 |
| `step3_driver_uart/` | TMC2209 link, version, EN toggle, **no motor** (SoftwareSerial: D10 straight to PDN, D11 through 1k) | U8g2, TMCStepper (teemuatlut) |

Files are exactly as delivered, so two things in them are known to be off:

- **step2:** with A on D2 the count runs backwards (clockwise counts down; one detent = 4 raw counts). On the breadboard the wires were swapped to fix it. The PCB routes A to D2 as well; the board firmware handles direction with Menu > Knob dir.
- **step3:** the comment `// 0 OK, 1 no reply, 2 all zeros` on `test_connection()` is wrong. TMCStepper returns 0 when a read gets no answer, so "no reply" shows as 2, not 1. The driver also needs 12 V on VM before it answers at all.

On a breadboard, keep the module off the breadboard and use female jumpers on its pins: DIAG on the BTT V1.2 sits in line with EN/VM and would land in the VM or EN strip. Power the Uno over USB first, then the brick; tie the brick's GND to the Uno's GND.

Not included: the combined "stage 1" sketch from the same period. Its PCB build creates `SoftwareSerial(255, 10)`, and pin 255 makes it write to a random register. Don't use it.
