# Changelog

Newest first. One entry per working session or release. Firmware releases are tagged `fw-vX.Y`, ordered board revisions `hw-rev-X`. Everything before 2026-10-14 was reconstructed from the two design chats (`docs/history/sources.md`).

## 2026-11-25

**Docs: Restructure** (plan: the 2026-11-24 project review)
- Step 1: the backup is the 2026-11-24 project export zip.
- Step 2: pushed chat 1's repo bundle to GitHub, a fast-forward from the README commit. The bundle matched the Project knowledge copies (firmware byte-identical). The `fw-v1.0` tag push was refused (HTTP 403), so the tag still has to be created on GitHub. Touched: STATUS.md.
- Step 3: added the Rev A KiCad project (5 files) and production files (6 files) to `hardware/`, byte-for-byte from the owner's PC; `.gitattributes` now keeps `hardware/production/` as exported. The 02:13 and 02:19 exports carry identical gerbers and drills. `hw-rev-a` belongs on `64a4da5`. Touched: hardware/README.md, STATUS.md.

## 2026-10-14

**Repo**
- Created the repo from both chats: firmware v1.0 and the breadboard sketches recovered from the transcripts, headless build and board simulator (all 20 scenarios reproduce), hardware reference, Rev A errata, assembly and flashing guides, decision log, status, and the working agreement for Claude sessions (`CLAUDE.md`).

**Bring-up (board 1)**
- First ATmega328P programmed off-board on a breadboard jig with the Uno as ISP (J2 isn't fitted on the built boards). MiniCore 3.1.3; Burn Bootloader wrote the fuses (L F7, H D7, E FD) fine. The upload step itself wasn't recorded.
- First power-up: board alive, motor only vibrates. Suspected crossed coil pairs or an open coil connection; not resolved.

**Hardware**
- Power harness spec for J1 / J4 / J6: panel jack, inline 2 A blade fuse on the + wire, 22 AWG, XH plugs; switch rated at least 2 A DC.
- IDC ribbon crimp procedure.
- Display upgrade research (1.3 in, 1.54 in, 2.42 in); nothing chosen.

## 2026-10-13

**Hardware**
- Assembly of the Rev A boards (started the evening of 2026-10-12).
- Inventory audit during assembly found three footprints for parts the owner doesn't have: J2/J10/J11 IDC box headers, J13 5.08 mm terminal, L1 0805 inductor (errata E2 to E4). Workarounds defined.
- Verified Amazon listings for 2x3 / 2x5 shrouded headers and FC-10 sockets.

## 2026-10-12

**Hardware**
- 5 Rev A boards delivered.

**Firmware v1.0**
- One firmware for the real board instead of the step-by-step ladder: 11-step power-up self test that names the failing part, then the tester UI (speed knob, run/stop, menus for current, microsteps, chopper, UART or STEP drive, acceleration, stall stop, coil check, live driver registers).
- No external libraries: own SSD1306, I2C and TMC2209 UART code (U8g2 had pushed the build to 98.7% of flash). 25,208 B flash, 490 B RAM, zero warnings.
- Driver link at 115,200 baud nominal with an echo check on the single-wire bus.
- Verified with a headless build on MiniCore and the stock Uno core, and a simavr board simulator: 20 injected-fault scenarios, STEP timing, ramp timing, worst-case stack 504 B.
- Review of the 2026-10-06 progress doc: its TMCStepper error codes were inverted, and the "any 9 to 24 V adapter" fallback would burn D1 (stay at 9 to 14 V).

## 2026-10-06

**Firmware (breadboard, Uno)**
- Bring-up ladder step 1 (OLED, found at 0x3C) and step 2 (encoder; clockwise counted down, fixed by swapping wires) done. Step 3 (driver UART) prepared, blocked by the brick's missing AC cord.
- Progress doc "Stepper Tester: Progress and Next Steps" written (snapshot in `docs/history/`).

## Up to 2026-10-06: Rev A design and order

**Layout and order**
- Owner placed and routed the main board section by section, then the front board, with Claude's routing plans and checks (a mid-routing check found 0 clearance violations at 0.2 mm across 537 tracks).
- Full top-layer GND pour and 122 stitching/grid vias; GND vias for C18 to C21.
- 42 silkscreen labels, signed "-DK"; reference designators moved off pads.
- Main and front boards merged into one outline with 2 mouse-bite tabs and a 2.0 mm slot.
- Production zip checked (outline, NPTH, vias, pours); encoder slot drill mode and thin silkscreen flagged (E5, E6).
- Ordered from JLCPCB as a customer panel, 2 designs.

**Schematic (Rev A), finished 2026-10-05**
- Split into a main board and a front UI board joined by a 10-pin IDC ribbon (J10/J11).
- Resistors and small ceramics moved to 0805; electrolytics stay THT.
- Extra motor connectors J12 (header) and J13 (screw terminal) in parallel with J8; coil-current LED pairs, DIR and EN LEDs.
- Net names, title block, build notes; Power net class; ERC fixes (schematic v06).

**Schematic, earlier**
- Concept: OLED diagnostic stepper tester for NEMA 17 and smaller. Bare ATmega328P, BTT TMC2209 V1.2 in UART mode, SSD1306 OLED, EC11 encoder, ALT-1205T 12 V brick.
- Power input: panel jack, inline 2 A blade fuse, panel switch, low-side IRFZ44N reverse protection, 470 uF bulk, 1N4744A zener; L7805 for 5 V.
- Options for "production-style" flexibility: test points, spare GPIO, fan header, STEP/DIR/EN header; reset button, I2C pull-ups, TVS and buzzer as DNP footprints.
- Generic BOM spreadsheet with inventory locations.
