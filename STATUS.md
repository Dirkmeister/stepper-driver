# Status

**Updated 2026-10-14.** Where the project stands right now. Update it at the end of every working session (see `CLAUDE.md`); history goes in `CHANGELOG.md`.

## At a glance

| Area | State |
|---|---|
| Hardware | Rev A: 5 boards delivered 2026-10-12. At least one built (2026-10-13 to 14). Known issues: `docs/errata-rev-a.md` |
| Bring-up | First power-up 2026-10-14: board alive, **motor only vibrates** (open) |
| Firmware | v1.0 (2026-10-12). First chip programmed 2026-10-14 on a breadboard ISP jig (fuse burn confirmed; the upload step wasn't recorded). The real-board self-test result hasn't been recorded |
| Design files | KiCad project and Rev A production files in `hardware/` since 2026-11-25. The inventory is in the Project knowledge (`inventory/component-inventory.md`), not the repo |
| Repo and docs | created 2026-10-14 from both chats; pushed to GitHub 2026-11-25. Tags `fw-v1.0` and `hw-rev-a` aren't on GitHub yet (see open issues) |

## Open issues

- [ ] **Motor only vibrates.** Power off, meter the motor wires: J8/J12/J13 pins 1-2 must be one coil, pins 3-4 the other (`docs/assembly.md`, Motor wiring). Then run Menu > Coil check and note what it says, and run at 0.25 rev/s watching both coil-LED pairs. If the wiring is right, try a lower speed or more current than the default 400 mA.
- [ ] **Record the real self-test result:** a photo of all 11 lines (turn the knob to scroll) or the debug log from J9. The first board was most likely running v1.0 at first power-up (fuses burned 03:38, motor running at 04:56), but that wasn't stated, and the PCB chat's diagnosis assumed the older bring-up sketch's screen.
- [ ] **Knob direction and screen orientation** on the real board: if wrong, set Knob dir / Screen in the menu (no reflash).
- [ ] **E1, DIAG:** write down what was done on each board (clipped, jumpered to J7, or left), and measure the module's DIAG pin position.
- [ ] **E5, encoder slots:** check whether the delivered boards have oblong slots or round holes.
- [ ] **E6, J1 polarity marks:** check whether they printed; mark by hand if not (J1 pin 1 is negative).
- [ ] **C1 / C14 voltage rating:** read it on the cans. The 9 to 14 V input limit and the firmware's 25 V alarm both assume 25 V.
- [ ] **Brick polarity:** meter the brick's plug (center-positive assumed, never checked) before crimping the J1 harness.
- [ ] **Per-board build record:** what went into J2, J10/J11, J13, L1, R17/R18.
- [ ] **Compare the repo firmware with the copy that was flashed** (it was rebuilt from the chat; MD5 `9813bde1ffaefffa24b478bb6aa437b4`).
- [ ] **Create the tags on GitHub:** `fw-v1.0` at commit `b416c16`, `hw-rev-a` at `64a4da5`. Tag pushes from Claude sessions are refused (HTTP 403, 2026-11-25), so Dallas creates them (GitHub Releases, or git on the PC).
- [ ] **Paste the new Project instructions** from `docs/claude-project-instructions.md` into the Project's instructions field on claude.ai (Claude can't edit that field).
- [ ] **Power harness questions never answered:** does the drawer 4 jack fit the brick's plug (5.5 x 2.1 mm assumed), is an inline fuse holder on hand, what DC rating are the panel switches, is there a fan for J6.
- [ ] **IDC parts:** check the drawer 3 IDC kit for FC-10 sockets before buying; verified listings are in `docs/errata-rev-a.md` (E2).

## Decisions waiting on the owner

- **Display:** keep the 0.96 in, or upgrade (1.3 in SH1106, 1.54 in Waveshare SSD1309, 2.42 in SSD1309). Comparison in `docs/hardware.md` section 12; Claude's pick was the 1.54 in, panel-mounted and wired to U6's pads.
- **DIAG on the built boards:** clip and jumper to J7, or leave it off (the firmware doesn't need it; test 10 just warns).
- **R17/R18:** fit only if the OLED is flaky over the ribbon.

## Next steps

1. Get a clean self test (11/11, or 10 + the DIAG warning) and a motor spinning on board 1. Record it here.
2. Create the `fw-v1.0` and `hw-rev-a` tags on GitHub.
3. Firmware v1.1: fixes from real-hardware testing first, then the auto test (ramp until stall, report max rpm at the set current), see `docs/firmware.md` Backlog.
4. Build and test boards 2 to 5 with a per-board record.
5. Front panel for the UI board; M3 standoffs.
6. Keep collecting Rev B changes in `docs/errata-rev-a.md`.

## Recently resolved

- 2026-11-25: inventory file in the Project knowledge (`inventory/component-inventory.md`). Hard rule 1 points at it; `docs/inventory.md` keeps the parts Dallas has stated that the file doesn't list, plus the buy list.
- 2026-10-14: MiniCore install (it's a board package in Boards Manager, not a library).
- 2026-10-14: `no flash data found ... empty.hex` during Burn Bootloader and `no eeprom data found` during upload are normal (`docs/build-and-flash.md`).
- 2026-10-14: first ATmega328P's fuses burned off-board on a breadboard jig (J2 isn't fitted on the built boards).
