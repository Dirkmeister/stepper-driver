# Sources

Where everything in this repo came from, so a claim can be traced back.

## Chats (claude.ai Project "Stepper Motor Tool")

| Chat | Turns | Covers |
|---|---|---|
| [Pocket stepper motor tester](https://claude.ai/chat/34498e81-f673-4d6f-816b-74c9fe60a03b) | 288 | Concept and part choices; schematic sections 1 to 7 drawn by the owner in KiCad; options; BOM; two-board split; SMD pass; layout guidance (owner routed); silkscreen, GND pour, panelization; production-zip check; JLCPCB order; breadboard bring-up steps 1 to 2 on an Uno; assembly (2026-10-13) and the inventory audit; IDC part sourcing; power harness; first power-up (2026-10-14, motor only vibrates); display upgrade research |
| [Stepper tester firmware development](https://claude.ai/chat/3bcf0dfc-30f7-4e41-b683-b5201fa95df0) | 13 | Review of the progress doc; firmware v1.0 with the built-in self test; headless build and the simavr board simulator (20 fault scenarios); soldering notes; breadboard ISP jig; MiniCore install; avrdude warnings explained (2026-10-12 to 2026-10-14) |

Both chats were read end to end on 2026-10-14 to build this repo.

## Documents

- [Stepper Tester: Progress and Next Steps](https://claude.ai/code/artifact/3d0a0d93-0c92-4010-bbde-49feed594a54) (Claude Docs, 2026-10-06). Snapshot in `2026-10-06-progress-and-next-steps.md`; checked line by line against the chat's tool inputs.
- `component-inventory.md`: the owner's parts inventory, uploaded to the PCB chat. Since 2026-11-25 it's in the claude.ai Project knowledge (`inventory/component-inventory.md`); it stays out of this repo.
- KiCad project and production zips: in `hardware/` since 2026-11-25 (see `hardware/README.md`). The generic BOM spreadsheet (`stepper_tester_bom.xlsx`) isn't in this repo.

## How the code was recovered

The chats delivered files from Claude's temporary workspaces, which no longer exist. The transcripts keep every tool call's input (not its output), so the files were rebuilt by replaying those inputs in order:

- `firmware/stepper_tester/stepper_tester.ino`: the full rewrite plus the 10 later edit scripts from the firmware chat. Result: MD5 `9813bde1ffaefffa24b478bb6aa437b4`, 1,779 lines. It compiles to exactly the sizes reported when it was delivered (25,208 B flash / 490 B RAM with MiniCore, 25,264 B with the stock Uno core) and the simulator reproduces every reported result, including the 504 B worst-case stack. Compare it with the copy you flashed if you have it.
- `tools/sim/sim.c`, `tools/sim/simavr-icr1-write.patch`, `tools/build.sh`: replayed the same way. Changes made for the repo: paths are relative to the repo, and the build script dropped the unused U8g2 references. Outputs are byte-identical (`fw.hex` MD5 `5d3b933bb9842a8ba4659fa62c3e8fce` for MiniCore).
- `firmware/bringup/*.ino`: copied verbatim from the PCB chat's file-creation calls.
- The pinned third-party commits in `tools/fetch_deps.sh` are the heads of those repositories on 2026-10-12, when v1.0 was built and simulated.
