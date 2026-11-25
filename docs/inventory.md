# Parts inventory

**The authority is Dallas's own inventory file, "Krech Protolab Component Inventory"** (updated 2026-09-15). It's in the claude.ai Project knowledge as `inventory/component-inventory.md` and stays out of this public repo. Before suggesting any part, footprint or purchase, check it, then the list below (`CLAUDE.md`, rule 1).

The file names some kits and cabinets without their contents: the drawer 3 IDC kit, the terminal-block assortment, the 0805 bin, the capacitor and diode cabinets. For parts that would be in one of those, ask him.

## Stated by Dallas, not in the inventory file

He has these, so they're not buy items:

- Boards and modules: Arduino Uno x2 (one confirmed genuine), Arduino Mega, Arduino Nano x5, an ESP-01 ("esp32-01"), PIC16F84A, PIC12F1572, ATtiny85, ATmega328P, BTT TMC2209 V1.2
- Displays: SSD1306 I2C OLED (4-pin, "0x78" printed), 1602A LCDs with and without PCF8574T backpacks
- Power: ALT-1205T 12 V brick, "a couple" of buck / power converters, a fuse kit, holders for blade fuses
- Resistors: loose 1.8k (not in the drawers)
- Capacitors: Wurth electrolytics ("a boatload" of 100 uF, 2.5 mm leads, 6.4 mm body); 470 uF (5 mm leads, 10.2 mm can)
- Mounting: M3 socket-head screws up to 20 mm, M3 nuts and nylocks, M3 male-female brass standoffs (per the 2026-11-24 project review)

## Buy list (checked against the inventory file 2026-11-25)

| Item | For | Status |
|---|---|---|
| 2x3 and 2x5 shrouded box headers | J2, J10, J11 (E2) | not in the file; the drawer 3 IDC kit isn't itemized, so check it first. Straight headers work meanwhile; verified listings in `docs/errata-rev-a.md` |
| FC-10 IDC sockets | ribbon ends | not in the file (the only IDC socket listed is 16-pin); check the drawer 3 kit first |
| 0805 10 uH inductor | L1 (E4) | no 0805 inductor in the file; a photo suggested an 0805 inductor book, so check the 0805 bin first. What was fitted isn't recorded |
| 5.08 mm 4-position terminal | J13 (E3) | the drawer 2 terminal-block assortment doesn't list pitches; check it first |
| Inline mini-blade fuse holder | J1 harness | not in the file; he has blade-fuse holders, but whether they're inline is unconfirmed |
| 30 mm 12 V fan | J6, optional | not in the file |
| IDC crimp tool | ribbon | not in the file (a vise works) |
