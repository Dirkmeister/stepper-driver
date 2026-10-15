# Decision log

Why the design is the way it is. "Owner" means Dallas decided or confirmed it; "accepted" means Claude proposed it and he went ahead without objecting. Dates are approximate for the early design work (the design chat started before 2026-10-05). Add new decisions at the bottom.

| # | Decision | Why | Alternatives considered | Who |
|---|---|---|---|---|
| 1 | Bench tester with an OLED diagnostic display, NEMA 17 and smaller, bipolar motors only | The TMC2209's sweet spot; diagnostics are the point, not just spinning | "universal controller" on an 8-pin ATtiny with pots | owner |
| 2 | Bare ATmega328P (DIP-28, socketed), 16 MHz crystal, ISP-programmed, no bootloader | With no USB chip the hardware UART is free for the driver; 2 KB RAM fits an OLED; same code runs on an Uno or Nano | ATtiny85 (512 B RAM, no UART), ATtiny1616, Arduino Nano (5 on hand) | owner chose the bare chip ("I'd rather use a chip"); socket, crystal, ISP and no bootloader accepted |
| 3 | BTT TMC2209 V1.2 module in single-wire UART mode, address 0 | Owner's module; UART gives current, microsteps, VACTUAL speed and all diagnostics | step/dir only | owner (module), accepted (UART mode, address 0) |
| 4 | Tie both PDN header pins to the UART net, 1k on TX, MS1/MS2/CLK to GND | BTT bridges UART to one pin, other brands to the other; tying both covers either. MS pins set address 0; CLK to GND selects the internal clock | jumpering pin 5 to 6 (rejected: pin 6 is CLK) | accepted |
| 5 | SSD1306 0.96 in I2C OLED | On hand; can draw bar graphs and result screens | 1602 LCD with PCF8574 (on hand, text only) | owner |
| 6 | ALT-1205T 12 V brick as the supply | On hand; no bench power hub | 24 V supply | owner |
| 7 | L7805 linear regulator for 5 V | Silent next to the VM ADC; only 0.28 to 0.47 W at 12 V | buck module (on hand) | accepted |
| 8 | Reverse polarity by a low-side IRFZ44N | No voltage drop; no P-channel FETs in stock | series 1N5822 Schottky | accepted (owner asked why, then placed it) |
| 9 | 2 A mini blade fuse, inline in the J1 harness, not on the board | Owner has the fuse kit and holders; a series fuse can't be an optional footprint without breaking the net | PTC fuse; board fuse footprint | owner (parts), accepted (placement) |
| 10 | Panel DC jack + XH harnesses for jack, switch and fan | Easier with an enclosure | board-mount jack with its own fuse (dropped) | owner |
| 11 | "Production-style" options: test points, spare GPIO header, fan header, STEP/DIR/EN header; reset button, I2C pull-ups, TVS and buzzer as DNP footprints | "I want to make sure I have room on the pcb for options you know, like they do in production" | none | owner |
| 12 | Encoder lines get external 10k + 10 nF RC | Alps' recommended EC11 circuit; clean edges into the interrupts | internal pull-ups only | accepted |
| 13 | VM sense 10k / 1.8k | 29 V maps to 4.42 V; owner had loose 1.8k | 15k / 2.7k (same ratio) | owner (parts) |
| 14 | AVCC through L1 10 uH + C8; 1N4148 from RESET to +5V | L1/C8 give the ADC a cleaner rail; RESET has no internal clamp diode to VCC, so D3 clamps spikes (Microchip AVR042) | AVCC straight to 5 V, no clamp | accepted |
| 15 | Split into a main board and a front UI board on one panel, 10-pin IDC ribbon (J10/J11) | Main board lies flat with air at the heatsink; UI board bolts to the panel | one board behind the panel | owner's idea |
| 16 | OLED soldered directly, not socketed | By Claude's height estimates a socket makes the stack too tall for the encoder bushing to reach the panel | socket | accepted |
| 17 | 0805 SMD for resistors and small ceramics; electrolytics stay THT | Fewer holes, better decoupling, owner has an 0805 assortment | all THT | owner |
| 18 | Extra motor connectors (J12 header, J13 terminal) in parallel with J8; coil, DIR and EN LEDs | Test odd motor pinouts; see each signal fire | J8 only | owner |
| 19 | Main board mounts open on a base; only the UI gets an enclosure | Owner builds enclosures from the PCB | full enclosure | owner |
| 20 | Fab at JLCPCB; both boards in one outline with 2 mouse-bite tabs | "i use jlc pcb"; the owner added the tabs and mouse bites himself. "Panel by customer, 2 designs" was the recommended order setting | KiKit panel | owner |
| 21 | Silkscreen signed "-DK" | "besides the 'krech protolab' instead do '-DK'" | "Krech Protolab" (still in the schematic title block) | owner |
| 22 | Firmware for the real board is one complete build with a power-up self test, not a module-by-module ladder | "i prefer a one shot, test, debug, revise." | 5-step breadboard ladder (steps 1 and 2 were done on an Uno) | owner |
| 23 | Firmware uses no libraries (own SSD1306, I2C, TMC2209 UART) | U8g2 build was 98.7% of flash; no hangs; the self test can isolate faults | U8g2 + TMCStepper | accepted |
| 24 | Driver UART at 115,200 nominal | Driver measures baud per message, so the 328P's +2% error doesn't matter; twice as fast as 57,600 | 57,600 | accepted |
| 25 | Program chips on a breadboard jig before they go on the board | J2 not fitted: "i dont have J2, id like to program the atmega before placing on the pcb" | in-circuit via J2 | owner |
| 26 | Git repo (`Dirkmeister/stepper-driver`, 2026-10-14) as the source of truth, docs in the repo, mirrored into the claude.ai Project | Proper documentation and revision control | chat history and Docs only | owner |
