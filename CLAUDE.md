# Working agreement for Claude

Read this before doing anything in this project. It applies in every chat of the claude.ai Project "Stepper Motor Tool" and in Claude Code sessions on this repo.

The owner is Dallas (Dirkmeister on GitHub). Boards are signed "-DK".

## Source of truth

1. **This repo** (`Dirkmeister/stepper-driver`). `STATUS.md` is the current state and open issues; `docs/` is the reference; `CHANGELOG.md` is the history.
2. **The owner's files** once added: KiCad project in `hardware/kicad/`, inventory in `docs/component-inventory.md`. When the design files and `docs/hardware.md` disagree, the design files win; fix the doc.
3. **Chat history** is background only. If a chat and the repo disagree, the repo wins unless the owner says otherwise. Don't rebuild facts from memory of earlier chats.

## Hard rules

Each one comes from a real mistake in this project.

1. **Inventory first.** Before recommending any part, footprint or purchase, check it against the owner's inventory file. If it isn't there, say so plainly and mark it as a buy item. Never assume he has a part. Rev A shipped with three footprints for parts he didn't have (errata E2 to E4), and he found out mid-assembly.
2. **Verify purchase links on the actual product page.** Open each listing and read it (pitch, pin count, gender, mount type, quantity) before recommending it; give the URL and what you checked. Never pick from titles or search snippets. A title-only pick turned out to be 40 sockets and no headers.
3. **Answer the question that was asked.** A yes/no question gets yes/no. Don't edit files, rearrange layouts or "fix" things he didn't ask for; list proposed fixes and ask before each one. When he asks for hardware upgrade options, give hardware options, not a firmware workaround.
4. **Physical facts come from the design files, the datasheet, or the owner's measurement.** Label estimates as estimates and show the math. If a dimension or pin position matters and isn't known, ask him to measure it.
5. **Never guess KiCad library names.** Use symbols and footprints that exist in the stock KiCad libraries (check) or in the project's own libraries. A guessed inductor footprint broke ERC once.
6. **Every file you hand over gets a new, unique name or version, and you confirm it was actually delivered** before referring to it. Same-name files caused lost edits twice.
7. **Track every flagged issue to closure in `STATUS.md`.** Don't let one get buried in a list of questions (the deleted front LEDs nearly did).
8. **The silkscreen is signed "-DK"**, not "Krech Protolab" (his choice for this board; the schematic title block still says Krech Protolab).

## How he likes to work

- Short, casual messages (lowercase, abbreviations). Reply direct and efficient; no padding, no hand-holding, but don't skip the reasoning on technical choices; he asks "why" when it's missing.
- **Wiring, assembly and breadboard work:** one small, testable step at a time. When words aren't landing, draw it: a schematic or wiring diagram, not a longer pin list.
- **Firmware for the real board:** one complete build with a built-in self test, verified before delivery (compile + simulator), then test, debug, revise on the hardware. Don't fall back to module-by-module ladders.
- **PCB layout:** he works section by section, bulk first and final touches at the end. Keep two lists, "wrong now" and a final-touch punch list; don't nag about polish mid-bulk. He usually makes his own KiCad edits from a clear list plus drawings; edit the files only when he asks.
- When editing KiCad files: preserve CRLF and tab formatting, check connectivity before and after, keep a backup, deliver under a new filename, and say exactly how to install it (close KiCad, replace the root-named file, reopen).

## Session protocol

**Start**
1. Read `STATUS.md`, then the docs that touch today's task.
2. If the repo is attached to the session, clone it and look at `git log --oneline -15`. Work from the clone.
3. Mention any open issue in `STATUS.md` that bears on today's task.

**During**
- Small commits, one logical change each (see Git below).
- Firmware: after every change run `tools/build.sh` and `tools/sim/run_all.sh`; report flash, RAM and the scenario result. Say what the simulator can't prove.
- Hardware: run anything that becomes a part or footprint choice through rule 1.

**End** (or when he says "wrap up")
1. Update `STATUS.md` (state, open issues, next steps) and add a `CHANGELOG.md` entry.
2. Commit and push. Tag releases.
3. If `STATUS.md`, `CHANGELOG.md`, `CLAUDE.md`, any of `docs/*.md` or the firmware changed, re-upload those files to the claude.ai Project knowledge as `stepper-driver/<repo path>` (for example `stepper-driver/docs/hardware.md`), so other chats see the change.
4. Tell him in a few lines what changed and what's next.

## Git

- One branch, `main`, for now. Commit prefixes: `fw:`, `hw:`, `docs:`, `tools:`, `repo:`.
- Firmware release: bump `FW_VER` and the header's version/date line, add a changelog entry, tag `fw-vX.Y` (minor for features and fixes, major for anything that changes how the board is used or flashed).
- Hardware: commit KiCad sources and project libraries; never backups or caches (`.gitignore` handles it). Production files of every ordered revision go in `hardware/production/rev-X/`, exactly as uploaded to the fab; tag `hw-rev-X` on that commit. Rev B work goes on a `hw/rev-b` branch.
- Commit author is the owner (`Dirkmeister`); add the attribution trailer the session asks for.
- No force-pushes, no history rewrites, no deleting branches or tags without asking.

## Firmware checks (required for every firmware change)

| Check | v1.0 baseline |
|---|---|
| `tools/build.sh` | 25,208 B flash, 490 B RAM, 0 warnings (MiniCore); 25,264 B on the stock Uno core |
| `tools/sim/run_all.sh` | 20 / 20 scenarios as expected |
| worst-case stack (simulated) | 504 B used, 1,558 B free above static RAM |

Flash ceiling is 32,768 B (no bootloader).

## Safety notes to repeat in any instructions you give him

- Never plug or unplug a motor with 12 V on; the TMC2209 dies.
- J1 pin 1 is NEGATIVE (JACK_N). The board's +/- marks may not have printed.
- J3 (ISP PWR) jumper off whenever the brick is connected.
- Input 9 to 14 V with D1 (15 V zener) fitted; C1 and C14 are assumed 25 V (unverified).
- Q1's tab is live (JACK_N): keep standoffs, screws and grounded metal off it.
- The StepStick goes in with EN/VM at the marked corner; backwards kills it.
- Measure against board GND (TP3/TP4), not the jack's minus.
