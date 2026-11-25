# Seeds

Opening messages for the next threads, newest first. The thread that wraps up writes one here; Dallas pastes the block into a new chat in the "Stepper Motor Tool" project. Each seed opens with its Thread line and is at most 10 lines.

## Bring-up: Motor only vibrates

Written 2026-11-25 by Docs: Restructure, from STATUS items 1 to 9.

```
Thread: Bring-up: Motor only vibrates
Goal: board 1 passes its self test and spins a motor. Work STATUS items 1 to 9 one testable step at a time; record results in STATUS, the errata and CHANGELOG.
1. Power off. Meter the motor's coil pairs at J8/J12/J13: pins 1-2 one coil, 3-4 the other (docs/assembly.md, Motor wiring).
2. Power on: photo of all 11 self-test lines or the J9 log (item 2); knob direction and screen orientation (item 3).
3. Menu > Coil check, then run at 0.25 rev/s watching both coil-LED pairs (item 1).
4. While at the bench: DIAG pin (4), encoder slots (5), J1 marks (6), C1/C14 can ratings (7), brick polarity (8), harness parts (9).
Safety (all 7 in CLAUDE.md): never plug or unplug a motor with 12 V on; J1 pin 1 is negative; J3 off with the brick connected; measure against TP3/TP4.
```
