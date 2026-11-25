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
| 2x3 and 2x5 shrouded box headers | J2, J10, J11 (E2) | not in the file; the drawer 3 IDC kit isn't itemized, so check it first (STATUS item 11). Straight headers work meanwhile; listings below |
| FC-10 IDC sockets | ribbon ends | not in the file (the only IDC socket listed is 16-pin); check the drawer 3 kit first (STATUS item 11); listing below |
| 0805 10 uH inductor | L1 (E4) | no 0805 inductor in the file; a photo suggested an 0805 inductor book, so check the 0805 bin first (STATUS item 11). What was fitted: STATUS item 10 |
| 5.08 mm 4-position terminal | J13 (E3) | the drawer 2 terminal-block assortment doesn't list pitches; check it first (STATUS item 11) |
| Inline mini-blade fuse holder | J1 harness | not in the file; he has blade-fuse holders, but whether they're inline is unconfirmed (STATUS item 9) |
| 30 mm 12 V fan | J6, optional | not in the file (STATUS item 9) |
| IDC crimp tool | ribbon | not in the file (a vise works) |

### Verified listings

Checked on the product pages 2026-10-13, not bought as far as recorded. Recheck price and stock before buying.

| Use | Listing | Verified |
|---|---|---|
| J10, J11 | https://www.amazon.com/dp/B0F9NSTWCV, 10 pcs 2x5 straight male shrouded header, $5.99 | 2.54 mm, male, 2x5, straight |
| Ribbon ends | https://www.amazon.com/dp/B00O9Y6FL6, uxcell 25 pcs FC-10P, $8.19 | 2.54 mm, 10-pin female IDC |
| J2 | https://www.amazon.com/dp/B07VQ8VLWG, 10 pcs 2x3 straight male shrouded header, $7.99 | 2.54 mm, male, 2x3, straight |

Avoid B00W8U8CSC: its title suggests a header + socket set, but the page says 40 female sockets and no headers.
