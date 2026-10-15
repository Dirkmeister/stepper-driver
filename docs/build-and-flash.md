# Build and flash

How to get `firmware/stepper_tester` onto an ATmega328P with the Arduino IDE and an Uno as the programmer. Procedure as used on 2026-10-14 (the first chip's fuses were burned that day with MiniCore 3.1.3).

## You need

- Arduino IDE (2.x or 1.8) with the **MiniCore** board package
- A genuine Arduino Uno running **ArduinoISP**, plus a 10 uF capacitor
- Either a breadboard jig (option A) or a 2x3 header fitted at J2 (option B)

## 1. Install MiniCore

MiniCore is a **board package, not a library**, so Library Manager never lists it.

1. File > Preferences (on a Mac: Arduino IDE > Settings). In "Additional boards manager URLs" add:
   ```
   https://mcudude.github.io/MiniCore/package_MCUdude_MiniCore_index.json
   ```
   If the box already has a URL, use the window icon at the end of the field and put this one on its own line.
2. Open Boards Manager: the circuit-board icon in the IDE 2.x sidebar (the books icon under it is Library Manager), or Tools > Board > Boards Manager in 1.8.
3. Search "MiniCore", install "MiniCore by MCUdude". It downloads its own toolchain; let it finish.
4. If it doesn't appear, the package list didn't download: check the Output panel for an error about that URL, re-paste it with no spaces, restart the IDE.

## 2. Make the Uno a programmer

1. Board: Arduino Uno. Upload File > Examples > 11.ArduinoISP > ArduinoISP normally.
2. Then put a 10 uF cap from the Uno's RESET to GND (negative leg to GND). It stops the Uno resetting when the IDE opens the port. It also blocks normal uploads to the Uno, so pull it if you ever reprogram the Uno itself.

## 3. Tools settings

Keep the sketch in a folder named `stepper_tester` (the IDE requires the folder name to match). Then set:

| Menu | Setting |
|---|---|
| Board | MiniCore > ATmega328 |
| Clock | External 16 MHz |
| BOD | BOD 2.7V |
| EEPROM | EEPROM retained |
| Compiler LTO | LTO enabled |
| Variant | 328P / 328PA |
| Bootloader | No bootloader |
| Programmer | **Arduino as ISP** (not "Arduino as ISP fast": that one talks at 115,200 baud, the ArduinoISP sketch at 19,200, and it fails with "not in sync" or "programmer is not responding") |
| Port | the Uno's port |

The Clock, BOD, EEPROM, Variant and Bootloader menus only appear after MiniCore > ATmega328 is selected. The same settings are in the comment block at the top of the sketch.

## 4A. Program on a breadboard jig (no J2 needed)

Used for the first chip; J2 isn't fitted on the built boards. Wire with USB unplugged; pin 1 is at the notch end.

| From | To |
|---|---|
| Uno D10 | 328P pin 1 (RESET), plus 10k from pin 1 to 5V |
| Uno D11 | pin 17 (MOSI) |
| Uno D12 | pin 18 (MISO) |
| Uno D13 | pin 19 (SCK) |
| Uno 5V | pins 7 (VCC) and 20 (AVCC) |
| Uno GND | pins 8 and 22 |
| | 100 nF across pins 7 and 8 |
| | 16 MHz crystal across pins 9 and 10, 22 pF from each to GND (same parts as Y1, C6, C7) |
| | 10 uF from the Uno's RESET to GND |

**Have the crystal in place before Burn Bootloader.** That step switches the chip to the external crystal; after it, the chip needs the crystal to run at all, including to be reprogrammed. A chip that stops answering after Burn Bootloader is missing its clock, not dead.

## 4B. Program in circuit through J2

Fit a 2x3 header at J2 (straight is fine, mark pin 1; see E2). With U2 socketed you can also just use the jig. Wire, with the brick unplugged:

| J2 pin | Signal | Uno |
|---|---|---|
| 1 | MISO | D12 |
| 2 | VCC | 5V |
| 3 | SCK | D13 |
| 4 | MOSI | D11 |
| 5 | RESET | D10 |
| 6 | GND | GND |

Put the **J3 jumper on** so the Uno powers the board. Afterwards: unplug the ISP cable, **pull J3**, then plug in the brick. Never have J3 on with the brick connected.

## 5. Burn Bootloader (once per chip)

Tools > Burn Bootloader. With "No bootloader" selected this only writes the fuses: **L F7, H D7, E FD**. A fresh chip runs at 1 MHz from its internal oscillator, so skipping this step makes everything act broken (self-test 1 catches it).

Turn on verbose upload output (File > Preferences) and check for `Device signature = 1E 95 0F`:

- `1E 95 14` is a plain 328 (no P): set Variant to 328 / 328A and burn again.
- `00 00 00` or `FF FF FF`: wiring, the crystal, or the 10 uF cap.

**Normal, not an error:** with verbose output off, a good burn prints only `Warning: no flash data found in Intel Hex file ...bootloaders/empty/empty.hex`. With "No bootloader", MiniCore writes an empty file in step 2, and avrdude warns and exits cleanly. That second step only runs if the fuse write succeeded and the chip answered again on the crystal. Any line starting with `Error:` is a real failure.

## 6. Upload

Sketch > Upload Using Programmer (not the normal Upload; there's no bootloader).

- Compile line: about `Sketch uses 25208 bytes (76%) ... Maximum is 32768 bytes`. A maximum of 32384 means a bootloader is selected.
- Upload output: `Warning: no eeprom data found in Intel Hex file ...stepper_tester.ino.eep` is also normal. The firmware doesn't preload EEPROM; a blank EEPROM boots with defaults (400 mA, 1/16 step, 60 rpm).
- Any `Error:` line is a real failure; save the whole output.

For more chips: swap with USB unplugged and repeat 5 and 6 (Burn Bootloader is once per chip).

## 7. First run

On programmer power only, tests 1 to 6 pass, VM warns, 7 to 11 are skipped. On the brick, all 11 run. See `docs/assembly.md` for the staged first power-up and `docs/firmware.md` for what each test means.

## No MiniCore? Stock core fallback

If MiniCore can't be installed: Board "Arduino Uno" (Arduino AVR Boards), Programmer "Arduino as ISP" (not "ArduinoISP", a different programmer), then the same Burn Bootloader and Upload Using Programmer steps.

- Compiles to 25,264 bytes, no warnings.
- Only accepts 328P chips (signature 1E 95 0F).
- The Uno's fuses don't preserve EEPROM through the chip erase every upload does, so each reflash resets the saved settings to defaults. (MiniCore's "EEPROM retained" sets the fuse that keeps them.)

## Headless build and simulator (no hardware)

For checking a change before it goes near a chip, on Linux or in Claude's workspace:

```
tools/fetch_deps.sh sim      # pinned ArduinoCore-avr, MiniCore, simavr (once)
tools/build.sh               # MiniCore build: build/minicore/fw.elf, fw.hex, sizes
tools/build.sh arduino       # stock Uno core build
tools/sim/setup.sh           # patch + build simavr and the board simulator (once)
tools/sim/run_all.sh         # build + all 20 scenarios, exits 1 on any difference
```

Needs `gcc-avr avr-libc binutils-avr` (avr-gcc 7.3), `build-essential libelf-dev pkg-config`, and Python 3 with Pillow for the screen renders. Details in `tools/sim/README.md`.
