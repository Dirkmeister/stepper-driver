# Hardware design files

Rev A as ordered from JLCPCB, copied byte-for-byte from the owner's PC on 2026-11-25. Tag `hw-rev-a` marks the commit that added them. Where these files and `docs/hardware.md` (compiled from the design chats) disagree, the files win.

## What's here

| Path | What | Notes |
|---|---|---|
| `kicad/Universal motor driver v01.kicad_pro` | KiCad project | the project name has spaces |
| `kicad/Universal motor driver v01.kicad_sch` | root schematic, the only sheet | title block: Universal Stepper Tester, rev A, 2026-10-05 |
| `kicad/Universal motor driver v01.kicad_pcb` | the board as ordered | same file as KiCad's 2026-10-06 02:15 backup, saved between the two gerber exports below, which are identical |
| `kicad/stepper_tester.kicad_sym` | project symbol library | holds `BTT_TMC2209_V1.2` |
| `kicad/sym-lib-table` | project symbol library table | see Known issues |
| `production/rev-A/` | fab outputs from 2026-10-06 | see below |

## Production files (Rev A)

| File | Exported (2026-10-06, CDT) | Notes |
|---|---|---|
| `Universal_motor_driver_v01.zip` | 02:19 | gerbers and drills, the file JLC needs |
| `bom.csv`, `positions.csv`, `designators.csv`, `netlist.ipc` | 02:19 | same export; LCSC part numbers blank |
| `production.zip` | 02:13 | an earlier export of the same set |

Which zip was uploaded isn't recorded, and it doesn't matter: the two exports' gerbers and drills match line for line apart from the creation time. Both carry the two known export problems: the encoder slots are routed slots, not the G85 mode JLC asks for (errata E5), and the silkscreen has lines down to 0.064 mm (E6). A G85 drill file was exported at 02:18 but never went into a zip.

`.gitattributes` stores everything under `production/` exactly as exported (the csv and ipc files are CRLF).

## Known issues in these files

- `sym-lib-table` maps the `tmc2209 module` library to the project folder by an absolute path on the owner's PC, not to `stepper_tester.kicad_sym`, so it won't resolve in a clone. The schematic keeps its own copy of every symbol, so it opens anyway. For Rev B, point it at `${KIPRJMOD}/stepper_tester.kicad_sym`.
- Not committed, still on the owner's PC: KiCad backups, `.kicad_prl` (local window state), 7 older or unused `.kicad_sch` files, the loose 02:18 drill files and the `.csv` / `.xlsx` spreadsheets in the project folder.

## Revisions

| Rev | Date | Status | Notes |
|---|---|---|---|
| A | ordered by 2026-10-06, delivered 2026-10-12 | built, under test | errata in `docs/errata-rev-a.md` |
| B | | not started | candidate changes at the end of the errata doc |

From Rev B on: work on a branch (`hw/rev-b`), keep the KiCad project name, bump the silkscreen revision text, and tag `hw-rev-b` at the commit that gets ordered. Dallas creates tags (GitHub Releases or git on the PC): tag pushes from Claude sessions are refused.
