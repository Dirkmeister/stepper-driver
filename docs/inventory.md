# Parts inventory

**The authority is the owner's own inventory file, `component-inventory.md`.** It belongs at `docs/component-inventory.md` (and in the claude.ai Project knowledge) but hasn't been added yet. Until it is, this page lists only what the design chats recorded, so nobody mistakes it for the real thing. Before suggesting any part, footprint or purchase, check the owner's file (see `CLAUDE.md`, rule 1).

## Stated by the owner in the chats

- MCUs and boards: PIC16F84A, PIC12F1572, "esp32-01" (read as an ESP-01), Arduino Uno x2 (one confirmed genuine), Arduino Mega, Arduino Nano x5, ATtiny85, ATmega328P
- Displays: SSD1306 I2C OLED (4-pin, "0x78" printed), 1602A LCDs with and without PCF8574T backpacks
- Power: ALT-1205T 12 V brick, "a couple" of buck / power converters
- Driver: BigTreeTech TMC2209 V1.2
- Fuses: a fuse kit, and holders for blade fuses
- Resistors: loose 1.8k (not in the drawers)
- Capacitors: Wurth electrolytics ("a boatload" of 100 uF, 2.5 mm leads, 6.4 mm body); 470 uF (5 mm leads, 10.2 mm can); ceramics are small orange discs
- Crystal: 3.3 mm tall; EC11 encoder: 5 pins plus 2 side tabs
- "essentially a full suite of electronic prototyping supply"

## Confirmed in the inventory file by Claude's audit (2026-10-13)

From grepping `component-inventory.md` during assembly. These are Claude's readings, not the file itself:

- IRFZ44N, L7805, S8050, 16 MHz crystal, 14-pin IC sockets (no 28-pin), EC11 encoder, 5 mm LEDs, JST XH kit and crimpers, test points, header pins
- 0805 assortment (the audit said the passives, 0805 LEDs and electrolytics were matched to photos during design; values below)
- Drawer 2: straight male headers; PCB terminal-block assortment (pitches not listed)
- Drawer 3: IDC connector kit (contents not itemized), ribbon cable
- Drawer 4: external DC jacks, locking push buttons (ratings not checked)
- Inductors: Bourns SRN kit, DIP inductors including a 10 uH (Cofufu kit); **no 0805 inductor listed**
- AMS1117-3.3 regulators

Earlier readings of inventory photos (before the audit) mentioned: 0805 values (resistors 1k, 1.8k, 4.7k, 10k; capacitors 100 nF, 10 nF, 10 uF, 470 nF; LEDs red, green, yellow, blue), an 0805 inductor book with 10 uH (the inventory file lists no 0805 inductor), Zebulon kit (0805 22 pF, S8050 in SOT-23, zeners without a 15 V value, LL4148), THT 1N4148, 1N4744A, 1N5822, IRF540N, IRFP250N, 78L05, CrocSee mini-blade fuse kit, KCD1-101 rocker, Loreso and Jameco capacitor kits. No P-channel MOSFETs.

## Not in the inventory (buy list as of 2026-10-14)

| Item | For | Status |
|---|---|---|
| 2x3 and 2x5 shrouded box headers | J2, J10, J11 (E2) | verified listings in `docs/errata-rev-a.md`; not bought as far as recorded |
| FC-10 IDC sockets | ribbon ends | same; the drawer 3 kit may already have them (unchecked) |
| 0805 10 uH inductor | L1 (E4) | inventory file lists none, but a photo suggested an 0805 inductor book: check it first; what was fitted isn't recorded |
| 5.08 mm 4-position terminal | J13 (E3) | pitch of owned blocks unconfirmed |
| Inline mini-blade fuse holder | J1 harness | unconfirmed whether the owned holders are inline |
| M3 standoffs and screws | mounting | unconfirmed |
| 30 mm 12 V fan | J6, optional | unconfirmed |
| IDC crimp tool | ribbon | unconfirmed (a vise works) |
