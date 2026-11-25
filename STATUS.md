# Status

Updated 2026-11-25 (Docs: Restructure). State and open items only: history goes in `CHANGELOG.md`, reasons in `docs/decisions.md`.

## State

| Area | State | As of |
|---|---|---|
| Hardware | Rev A: 5 boards delivered, at least 1 built | 2026-10-14 |
| Bring-up | Board 1 powers up; motor only vibrates | 2026-10-14 |
| Firmware | v1.0: build and simulator match the `CLAUDE.md` baseline (Firmware checks); real-board self test not recorded | 2026-11-25 |
| Repo | On GitHub with its full history: firmware, tools, docs and design files. Tags not created yet (item 13) | 2026-11-25 |
| Design files | Rev A KiCad project and production files in `hardware/` | 2026-11-25 |
| Inventory | Dallas's file in the Project knowledge (`inventory/component-inventory.md`); stated parts and the buy list in `docs/inventory.md` | 2026-11-25 |

## Active thread

None. Next: Bring-up: Motor only vibrates (seed in `docs/seeds.md`).

## Queue

1. Bring-up: Motor only vibrates
2. FW: v1.1 (bring-up fixes first, then the auto test; `docs/firmware.md` Backlog)
3. HW: Boards 2 to 5, a build record each
4. HW: Front panel for the UI board

## Open items (ages on 2026-11-25)

| # | Item | Opened | Age | Thread |
|---|---|---|---|---|
| 1 | Motor only vibrates: meter the coil pairs at J8/J12/J13 with power off (`docs/assembly.md`, Motor wiring), run Coil check, try 0.25 rev/s | 2026-10-14 | 42 d | Bring-up |
| 2 | Record the self test: photo of all 11 lines or the J9 log (also confirms v1.0 is on the chip; the PCB chat's diagnosis assumed the older bring-up sketch) | 2026-10-14 | 42 d | Bring-up |
| 3 | Knob direction and screen orientation on the real board | 2026-10-14 | 42 d | Bring-up |
| 4 | E1 DIAG: what was done on each board; measure the pin | 2026-10-14 | 42 d | Bring-up |
| 5 | E5 encoder slots: oblong or round (both production zips have routed slots) | 2026-10-06 | 50 d | Bring-up |
| 6 | E6 J1 +/- marks: printed? (0.064 mm lines went to the fab) | 2026-10-06 | 50 d | Bring-up |
| 7 | C1/C14 voltage rating: read the cans | 2026-10-14 | 42 d | Bring-up |
| 8 | Brick polarity: meter the plug (center-positive assumed) before crimping J1 | 2026-10-14 | 42 d | Bring-up |
| 9 | Power harness: jack fit (5.5 x 2.1 mm?), inline fuse holder, switch DC rating, J6 fan | 2026-10-14 | 42 d | Bring-up |
| 10 | Per-board record: J2, J10/J11, J13, L1, R17/R18 | 2026-10-14 | 42 d | HW |
| 11 | Check before buying: the drawer 3 IDC kit (FC-10 sockets, 2x3 and 2x5 box headers), the 0805 bin (10 uH for L1), the drawer 2 terminal blocks (5.08 mm for J13) | 2026-10-13 | 43 d | HW |
| 12 | C6/C7 dielectric: C0G/NP0? | 2026-10-14 | 42 d | HW |
| 13 | Tags on GitHub: `fw-v1.0` at `b416c16`, `hw-rev-a` at `64a4da5` (Claude sessions can't push tags) | 2026-11-25 | 0 d | Dallas |
| 15 | Check `docs/hardware.md` against the KiCad files now in `hardware/kicad/` (the design files win) | 2026-11-25 | 0 d | HW |

Closed 2026-11-24 by the review: the flashed-firmware comparison (folded into item 2), J5's connector type (JST XH 4-pin, 2.50 mm) and the encoder RC pairing (R13/C18 on PD2, R14/C19 on PD3, R15/C20 on PD4).

Closed 2026-11-25 by Docs: Restructure: design files into `hardware/`, the inventory into the Project knowledge, the new Project instructions (pasted; they match `docs/claude-project-instructions.md`), the RETIRED banners on the 2026-10-06 progress doc and its repo snapshot (item 14), and the knowledge-mirror decision (all 11 repo files, `docs/decisions.md` #30).

## Decisions waiting on Dallas

| Decision | Options | Opened |
|---|---|---|
| Display | Keep the 0.96 in, or go 1.3, 1.54 or 2.42 in (`docs/hardware.md` section 12) | 2026-10-14 |
| DIAG on built boards | Jumper to J7, or leave it off (firmware test 10 only warns) | 2026-10-14 |
| R17/R18 | Fit only if the OLED is flaky over the ribbon | 2026-10-14 |

## Decision log

`docs/decisions.md`, 30 entries. New ones go at the bottom.

Health check 2026-11-25: 1 red (items 1 to 9), 3 yellow (2 fixed the same day), 3 green. Next one due 2026-12-09 or after 5 threads, whichever comes first.
