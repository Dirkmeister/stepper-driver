# Hardware design files

**Not added yet.** The KiCad project and production files only exist on the owner's PC. Until they're here, `docs/hardware.md` (compiled from the design chats) is the only reference, and it can't be checked against the real files.

## What goes where

| Path | Put here | Notes |
|---|---|---|
| `kicad/` | the whole KiCad project folder: `Universal motor driver v01.kicad_pro`, `.kicad_sch`, `.kicad_pcb`, plus `sym-lib-table`, `fp-lib-table` and any project library (`tmc2209 module` / `stepper_tester.kicad_sym`, custom footprints) | The `.kicad_pcb` must be the ordered one (Claude's `v09_tabs`, installed under the root name). Check it matches before committing. |
| `production/rev-A/` | exactly what was uploaded to JLCPCB: `Universal_motor_driver_v01.zip` (gerbers + drills), plus `bom.csv`, `designators.csv`, `positions.csv` from the same export | This freezes Rev A as fabricated. |
| `../docs/component-inventory.md` | the owner's inventory file | Also add it to the claude.ai Project knowledge. |
| `bom/` (optional) | `stepper_tester_bom.xlsx`, the generic BOM from the design chat | Its designators are older than the final schematic; `docs/hardware.md` section 9 is newer. |

`.gitignore` already skips KiCad backups (`*-backups/`), `fp-info-cache`, lock files and autosaves, so you can copy the whole project folder in.

## Adding them

From a clone of this repo on the PC (GitHub Desktop or git):

1. Copy the files into the paths above.
2. Commit: `hw: add Rev A KiCad project as ordered` and `hw: add Rev A production files`.
3. Tag the commit that holds the ordered design: `hw-rev-a`.
4. Push.

Or attach the files in a chat in the Project and ask Claude to commit them.

## Revisions

| Rev | Date | Status | Notes |
|---|---|---|---|
| A | ordered by 2026-10-06, delivered 2026-10-12 | built, under test | errata in `docs/errata-rev-a.md` |
| B | | not started | candidate changes at the end of the errata doc |

From Rev B on: work on a branch (`hw/rev-b`), keep the KiCad project name, bump the silkscreen revision text, and tag `hw-rev-b` at the commit that gets ordered.
