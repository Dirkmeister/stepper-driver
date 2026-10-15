<!--
Snapshot of the Claude Docs document "Stepper Tester: Progress and Next Steps"
(https://claude.ai/code/artifact/3d0a0d93-0c92-4010-bbde-49feed594a54), written 2026-10-06
in the PCB design chat, copied here on 2026-10-14. It was never updated after the boards
arrived, so its status and open issues are out of date: see STATUS.md for the current state.
Date and author chips from the doc are shown in [brackets]. One unanswered comment in the doc,
on the DIAG item: "Is clip-and-jumper to J7 the fix you want for DIAG, or measure first and decide?"
-->

# Stepper Tester: Progress and Next Steps

[date: 2026-10-06] · [@Dallas]

## Status at a glance

The boards are ordered from JLCPCB, and firmware bring-up is 2 of 5 steps done: the OLED and encoder both work on the Uno. Step 3 (talking to the driver) is planned and wired on paper, but blocked until the 12 V brick has an AC cord.

| Track | State |
| --- | --- |
| PCB | Routed, checked, panelized, ordered |
| Firmware step 1, OLED | Done |
| Firmware step 2, encoder | Done (one cleanup left) |
| Firmware step 3, driver UART | Ready to wire, needs 12 V |
| Firmware steps 4 and 5 | Not started |

## What we did: the PCB

The board went from placed parts to an ordered two-board panel, 3.96 x 5.08 in (100.63 x 129.11 mm), checked against JLC's limits before ordering.

| Area | Result |
| --- | --- |
| Layout fixes | J7 moved off U7 (later moved back, see open issues), H1 to its corner, TP refs restored, J11 flipped to the front board's back |
| Routing | You routed everything; my clearance check found 0 violations at 0.2 mm across 537 tracks |
| Ground | Full top GND pour, 122 new GND vias (76 stitching along the long bottom runs, 46 grid), plus vias for C18, C19, C20, C21 |
| Front board | Exact waypoints for the OLED, encoder, and LED lines, all checked for clearance |
| Silkscreen | 42 labels added (pinouts, EN/VM, NO HOT PLUG, board ID with -DK), all TP refs hidden |
| Tabs | Two outlines merged into one through the tabs, gap set to exactly 2.0 mm, mouse-bite footprints aligned |
| Production check | Outline closed, mouse bites in the non-plated drill file, pours filled, copper 0.5 mm from every edge |
| Order | Placed with JLC as panel by customer, 2 designs |

Two items were flagged before ordering: the encoder's mounting slots were exported as routed slots instead of the G85 drill mode JLC asks for, and some silkscreen (the J1 +/- symbol at 0.064 mm) is below JLC's minimum line width.

## What we did: firmware bring-up

We switched from wiring everything at once to a five-step ladder, one part and one small sketch per step, on a genuine Arduino Uno. Each step keeps the wiring from the step before.

1. **OLED (done).** Four wires: GND, VCC to 5V, SCL to A5, SDA to A4. The I2C scan found the display at 0x3C (press the Uno's reset button with the Serial Monitor open at 115200 to see it). Sketch: `step1_oled.ino`.
2. **Encoder (done).** A to D2, B to D3, C to GND, switch to D4 and GND, internal pullups. One click = 4 raw counts, so the sketch divides by 4. Clockwise counted down; you swapped A and B to fix it. Sketch: `step2_encoder.ino`.
3. **Driver UART (next).** Wiring and sketch are ready: `step3_driver_uart.ino`.

Libraries installed: U8g2. Still to install: TMCStepper (by teemuatlut).

The full stage 1 sketch, `stepper_tester_bringup.ino`, is also written. It combines all of this plus a spin test, and is the end goal of the ladder.

## Open issues

The AC cord is the only blocker; the rest can be handled any time before the boards arrive.

- [ ] **Find the brick's AC cord.** Match the socket shape: figure-8 (C7, like a PS4), three-lobed Mickey Mouse (C5, like a laptop charger), or three-pin trapezoid (C13, like a desktop PC). Fallback: any 9 to 24 V DC adapter, kept under 25 V for the cap's rating.
- [ ] **Encoder cleanup.** Put A back on D2 and B on D3, then change `encRaw += QEM[...]` to `encRaw -= QEM[...]` in `encISR()`. This matches the PCB, which routes A to D2.
- [ ] **Measure the DIAG pin.** Note which way it points (down like the other pins, or up) and how many 0.1 in (2.54 mm) steps it sits from EN.
- [ ] **Check DIAG against the PCB.** J7 sits at the pin 1/16 end of the module (y 66.67 mm), but your DIAG pin is at the EN/VM end (y 48.9 mm). If the pin points down, it won't land in J7 and may touch copper under the module. Likely fix: clip or bend the DIAG pin and run a jumper to J7.
- [ ] **Confirm the encoder slots in JLC's viewer.** Both mounting tabs should show as oblong slots, not circles.
- [ ] **Decide on driver mounting for the breadboard.** Loose module with female jumpers on its pins (planned), or a small perfboard driver carrier.

## What's next

Pick up at step 3: prove the Uno can talk to the TMC2209, with no motor attached.

### Step 3 wiring

Leave the driver off the breadboard and push female-to-male jumpers onto its pins, so DIAG can't short anything. Uno USB and brick both unplugged while wiring.

- VIO to Uno 5V, and the GND beside VIO to Uno GND
- VM to brick +12V, and the GND beside VM to brick minus
- Brick minus also to Uno GND (shared ground is required)
- 100 uF cap across VM and GND at the module, long leg to VM
- MS1, MS2, and CLK to GND (sets UART address 0)
- EN to D7
- Both PDN pins tied together; D10 straight to PDN, and D11 through a 1k resistor to PDN
- STEP, DIR, DIAG, and the four motor pins: nothing yet

Double-check that VM goes to + before plugging in the brick. Power up the Uno over USB first, then the brick.

### Step 3 success

- Screen shows **Link: OK (0)** and **Version: 0x21**
- Pressing the knob flips "EN out", and "seen" follows within half a second
- If it fails: (1) means no reply, so check the brick, the shared ground, and the PDN wire; (2) means D10 and D11 are probably swapped

### After step 3

1. **Step 4, motor.** Add STEP to D9 and DIR to D6, find the coil pairs with a meter (a few ohms within a pair), and spin slowly at 400 mA. Never plug or unplug the motor with 12 V on.
2. **Step 5, extras.** DIAG to D8 once its location is sorted, the VM voltage reading on A0, and stall detection.
3. **UI framework.** Menus and screens for speed, current, and diagnostics. Only needs the OLED and encoder, so it can be built any time.
4. **Move to the real board.** Flash the Uno with the ArduinoISP sketch, then program the bare ATmega328P through MiniCore (ATmega328, 16 MHz external).

## Reference

Uno pin numbers match the bare ATmega328P, so the same firmware runs on both; only the driver UART changes between breadboard and PCB.

| Signal | Uno pin | 328P pin | Notes |
| --- | --- | --- | --- |
| ENC_A | D2 | PD2 | Interrupt; count sign flipped in code |
| ENC_B | D3 | PD3 | Interrupt |
| ENC_SW | D4 | PD4 | Pressed = LOW |
| LED_STAT | D5 | PD5 | Front board status LED |
| DIR | D6 | PD6 | |
| EN | D7 | PD7 | Active LOW (LOW = driver on) |
| DIAG | D8 | PB0 | Stall / fault flag |
| STEP | D9 | PB1 | |
| Driver UART (breadboard) | D10, D11 | PB2, PB3 | SoftwareSerial; D11 through 1k |
| Driver UART (PCB) | D0, D1 | PD0, PD1 | Hardware UART, R16 on TX |
| Debug out (PCB) | D10 | PB2 | J9 pin 1 to a USB-serial adapter |
| VM sense | A0 | PC0 | 10k / 1.8k divider |
| Buzzer | A1 | PC1 | DNP on the board |
| SDA / SCL | A4 / A5 | PC4 / PC5 | OLED at 0x3C |

### Key numbers

- **UART at 57600 baud.** UBRR = 16,000,000 / (8 x 57,600) - 1 = 33.72, rounds to 34, actual 16,000,000 / (8 x 35) = 57,143, so -0.79% error. 115200 would be +2.12%.
- **Speed register (VACTUAL).** One count = 12,000,000 / 16,777,216 = 0.71526 microsteps/s. 1 rev/s at 1/16 microstepping = 200 x 16 = 3200 microsteps/s, so VACTUAL = 3200 / 0.71526 = 4474.
- **VM scale.** (10 + 1.8) / 1.8 = 6.556 x the ADC voltage.
- **Driver settings.** R_SENSE 0.11 ohm, address 0, test current 400 mA RMS, 1/16 microstepping, StealthChop.

### Files

- `step1_oled.ino`, `step2_encoder.ino`, `step3_driver_uart.ino`: the bring-up ladder
- `stepper_tester_bringup.ino`: the combined stage 1 sketch
- `Universal_motor_driver_v09_tabs.kicad_pcb`: the board as panelized for ordering
