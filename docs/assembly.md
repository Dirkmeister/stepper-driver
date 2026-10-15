# Assembly and bring-up (Rev A)

Bench procedure for building a Rev A board. Part values and pinouts are in `docs/hardware.md`; known board problems and their workarounds are in `docs/errata-rev-a.md`.

## Safety first

- **Never plug or unplug a motor with 12 V on.** The inductive kick kills TMC2209s. Switch off, then touch the motor connector.
- **J1 pin 1 is negative** (JACK_N). The board's `+`/`-` marks may not have printed (E6).
- **J3 (ISP PWR) off whenever the brick is plugged in.**
- **Input 9 to 14 V** with D1 (15 V zener) fitted. C1 and C14 are assumed to be 25 V parts; check the cans.
- **Q1's tab is live** (JACK_N). Keep standoffs, screws and the base plate off it.
- **StepStick orientation:** `EN` and `VM` are marked on the silkscreen at the module's corner. A module plugged in backwards dies on power-up.

## Build order

1. **0805 parts first** (resistors, ceramics, L1, 0805 LEDs), then low THT parts, then tall ones (C1, the TO-220s, sockets, headers, connectors).
2. **U2 goes in a socket** made from two 14-pin sockets end to end. Without a fitted J2, every firmware update means pulling the chip, so the socket matters.
3. **U7 goes on two 1x8 female headers.** Dry-fit the module before soldering anything near it and check where its DIAG pin lands (E1).
4. **Polarity checks:** C1 and C14 (+); D1, D3, D5, D6 cathode bands; LED orientation; U2 notch matches the silkscreen.
5. **Coil LED pairs (D9/D10, D11/D12) are back to back on purpose,** one LED each way. Don't "fix" one.
6. **D14, the blue EN LED,** points from R22 into the EN line. It only lights when the driver is enabled, so it's dark during most of the self test.
7. **Do not populate:** D5, R17, R18, SW2, BZ1, Q2, R6, R7, D6. R17/R18 are harmless to fit (4.7k or 10k); only fit them if the OLED is flaky over the ribbon.
8. **Footprints with workarounds** (E2 to E4): straight male headers in J2/J10/J11 with pin 1 marked; J13 only if your terminal blocks are 5.08 mm; L1 as 0 ohm, a bridge, or a bent-leg DIP 10 uH.
9. **Encoder slots:** if the board came back with round holes, file them out or snip the tabs (E5).

## First power-up (staged)

1. U2 and U7 out. Brick on through the harness and switch. TP1 should read about 12 V, TP2 5.0 V (against TP3/TP4, not the jack's minus).
2. Power off. Insert the programmed U2 (see `docs/build-and-flash.md`). Power on: the OLED shows the self test. With U7 still out, tests 1 to 6 should pass, test 7 (LINK) fails with "No reply", and 8 to 11 are skipped. That's expected.
3. Power off. Insert U7, heatsink up, EN/VM to the marked corner. Power on: all 11 tests should pass (10, DIAG, may warn if DIAG isn't jumpered; that's optional).
4. Power off. Connect a motor. Power on: the motor clicks once during test 11 (the output check). Then Menu > Coil check, then press to run.

Send back a photo of the self-test screen (turn the knob to scroll all 11 lines) or the debug log (`docs/firmware.md`).

## Power harness (J1, J4, J6)

```
brick -> panel DC jack -> inline fuse holder (2 A mini blade) on the + wire -> XH 2-pin -> J1
panel switch (2 wires, either way round) -> XH 2-pin -> J4
optional 12 V fan -> XH 2-pin -> J6 (pin 1 +12V red, pin 2 GND black)
```

- **J1:** XH pin 2 to the brick's + (through the fuse), XH pin 1 to its -. Most bricks are center-positive, but this one was never metered: check the plug first. J1's order is backwards from the usual convention; check against `docs/hardware.md` before crimping.
- **Fuse:** a standard (fast) 2 A mini blade is fine; the switch-on surge is well under its melt rating (estimate).
- **Switch:** rated at least 2 A DC. The KCD1-101 rocker was the planned part; the rating of the locking push buttons in drawer 4 hasn't been checked.
- **Wire:** 22 AWG for all three; that's the largest XH takes. At the 1.42 A worst case, 2 ft (0.6 m) of total run drops about 46 mV.
- **Panel jack:** identify the lugs with a meter. The center pin connects to the center hole. Of the other two, the switch lug opens when a plug is inserted; leave it empty and use the sleeve lug.
- **Building it:** heat shrink on first, strip 1/4 in (6 mm), tin, hook and solder at the panel end; XH crimp at the board end with a ratcheting crimper (IWISS SN-28B or Engineer PA-09). Pliers make crimps that look fine and pull out in a week. Cut each harness to the inside dimension plus 1 to 2 in (25 to 50 mm).
- **Still open:** whether the drawer 4 jack fits the brick's plug (5.5 x 2.1 mm assumed), whether an inline holder is on hand, the switch's DC rating.

## Ribbon (J10 to J11)

1. The red stripe is wire 1; the molded triangle on the IDC socket marks pin 1. Stripe on the triangle side at both ends.
2. Cut the ribbon square. Let it overhang about 1/32 in (1 mm).
3. Press evenly (vise or IDC tool) until both latches click. Trim, fold the cable back over the top and add the strain-relief clip if the socket has one.
4. Leave about 1 in (25 mm) of slack.
5. Continuity test: pin n to pin n beeps; neighbours (1-2, 2-3, ...) must not.

## Motor wiring

1. Power off before touching the motor connector.
2. Find the coil pairs with a meter: two wires of the same coil read a few ohms (typically 1 to 5 ohm for a NEMA 17, estimate); wires from different coils read open.
3. On J8 / J12 / J13, **pins 1-2 must be one coil and pins 3-4 the other.** If pin 1 pairs with pin 3, swap the wires in positions 2 and 3; if it pairs with pin 4, swap positions 2 and 4.
4. Run slowly (0.25 rev/s or lower) and watch the coil LEDs: both pairs should alternate red and green. They show the driver switching both outputs, not current reaching the motor; an open motor lead can still light its pair.
5. With v1.0, Menu > Coil check runs the motor in SpreadCycle at 30 rpm for 2 s and reports each coil (`ok` / `OPEN`) and shorts.

**Open as of 2026-10-14:** on first power-up the motor only vibrated. Likely causes, in order: crossed coil pairs, an open coil connection (bad crimp), or a startup stall (slow down, or raise the current from the default 400 mA). Not resolved yet; record the meter readings and the Coil check result. The two chats disagreed on whether crossed pairs set the driver's open-load flags, so don't rely on Coil check alone until that's been seen on real hardware.
