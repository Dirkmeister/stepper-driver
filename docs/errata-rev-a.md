# Rev A errata

Known problems with the Rev A boards (ordered by 2026-10-06, delivered 2026-10-12), with workarounds and Rev B fixes. Status reflects what the chats recorded as of 2026-10-14; "unknown" means nobody wrote down what happened on the built boards. Update the status column as things get checked.

| ID | Issue | Severity | Status |
|---|---|---|---|
| E1 | DIAG pin doesn't line up with J7 | medium | workaround, not recorded |
| E2 | J2 / J10 / J11 footprints are IDC box headers the owner didn't have | medium | workaround, not recorded |
| E3 | J13 terminal pitch not confirmed against inventory | low | unknown |
| E4 | L1 is an 0805 inductor that isn't in the inventory | low | workaround, not recorded |
| E5 | Encoder mounting slots exported in the wrong drill mode | low | unknown |
| E6 | Silkscreen lines below JLC's minimum, including J1's +/- | medium | unknown |
| E7 | J1 pin 1 is negative, opposite to convention | design trap | documented |

E2 to E4 came out of Claude's audit of the owner's inventory file during assembly (2026-10-13). J2/J10/J11 were picked without checking the inventory at all; J13 was assumed to be in stock; L1 followed a photo reading of an 0805 inductor book that the inventory file doesn't list. That's why `CLAUDE.md` rule 1 exists.

## E1: DIAG pin vs J7

- **Issue:** on the owner's BTT TMC2209 V1.2 the DIAG pin is in line with EN/VM, at the EN/VM end of the module (y 48.9 mm on the board). J7, the DIAG header, sits under the module at the pad 1/16 end (112.39, 66.67), the opposite end. J7 was moved out from under the module in Claude's v04 fixes, then ended up back under it after later edits; that's the ordered version.
- **Impact:** a DIAG pin pointing down won't land in J7, can keep the module from seating, and can touch copper under the module (the +5V trunk runs there).
- **Workaround:** dry-fit the module first. Clip or bend the DIAG pin clear. If you want DIAG, run a jumper from the pin to J7. Firmware v1.0 doesn't need DIAG: self-test 10 only reports whether it's wired, as a warning.
- **Still to do:** measure the pin (which way it points, how many 0.1 in steps from EN) and record what was done on each board.
- **Rev B:** put the DIAG landing where the measured pin actually is, clear of other copper, or make a module footprint with DIAG built in.

## E2: IDC box-header footprints (J2, J10, J11)

- **Issue:** J2 (2x3 ISP) and J10 / J11 (2x5 ribbon) use shrouded IDC box-header footprints. The owner had no box headers.
- **Workaround:** straight 2x3 / 2x5 male headers fit the same holes. Mark pin 1 with a paint-pen dot beside the square pad, and keep the ribbon's red stripe on pin 1 at both ends. An unkeyed ISP header is easy to plug in rotated, which puts VCC on RESET and GND on MISO; check orientation every time.
- **Recorded so far:** J2 isn't fitted on the boards built so far ("i dont have J2", 2026-10-14), so the first chip was programmed on a breadboard jig (`docs/build-and-flash.md`). What was fitted at J10/J11 isn't recorded.
- **Parts, verified on the product pages 2026-10-13 (not bought as far as recorded):**

  | Use | Listing | Verified |
  |---|---|---|
  | J10, J11 | https://www.amazon.com/dp/B0F9NSTWCV, 10 pcs 2x5 straight male shrouded header, $5.99 | 2.54 mm, male, 2x5, straight |
  | Ribbon ends | https://www.amazon.com/dp/B00O9Y6FL6, uxcell 25 pcs FC-10P, $8.19 | 2.54 mm, 10-pin female IDC |
  | J2 | https://www.amazon.com/dp/B07VQ8VLWG, 10 pcs 2x3 straight male shrouded header, $7.99 | 2.54 mm, male, 2x3, straight |

  Avoid B00W8U8CSC: its title suggests a header + socket set, but the page says 40 female sockets and no headers. Whether the IDC kit in drawer 3 already has FC-10 sockets was never checked.
- **Rev B:** footprints only for parts that are in the inventory, or buy the box headers and add them to the inventory before layout.

## E3: J13 terminal pitch

- **Issue:** J13 is a 4-position 5.08 mm (0.2 in) screw terminal (Phoenix MKDS footprint). The owner's terminal-block assortment doesn't list pitches; at design time J13 was called "from stock" without checking.
- **Workaround:** at 5.08 mm, two 2-position blocks slide together into a 4. At 3.5 mm they won't fit; leave J13 empty, because J8 and J12 carry the same four signals.
- **Rev B:** pick the pitch from the inventory.

## E4: L1 inductor

- **Issue:** L1 (10 uH, AVCC filter) has an 0805 footprint. The inventory doc lists no 0805 inductor; it lists Bourns SRN power inductors and DIP inductors, including a 10 uH in the Cofufu kit. During the SMD pass Claude read the owner's photos as showing an 0805 inductor book with 10 uH, which is why L1 got this footprint. Check that book before buying anything.
- **Workaround:** a 0 ohm part or solder bridge (AVCC still gets C8), or a DIP 10 uH with its legs bent onto the pads.
- **Rev B:** footprint for the inductor that's actually in stock, or drop L1 and keep C8.

## E5: Encoder mounting slots

- **Issue:** SW4's two 1.5 mm mounting slots were exported as routed slots instead of the "alternate drill mode" (G85) JLC asks for. The fix was recommended before ordering; whether it was applied isn't recorded.
- **Impact:** JLC may have drilled round holes.
- **Workaround:** if the holes are round, file them out or snip the tabs. The panel nut holds the encoder.
- **Fix for the next order:** KiCad, File > Fabrication Outputs > Drill Files, "Oval holes drill mode: Use alternate drill mode", regenerate, and check the slots are oblong in JLC's gerber viewer.

## E6: Silkscreen too thin

- **Issue:** J1's `+`/`-` symbol lines are 0.064 mm, some footprint graphics are 0.10 mm, and the added 0.8 mm labels use a 0.12 mm stroke. Claude's figure for JLC's minimum is about 0.15 mm line and 1.0 mm text height (confirm on JLC's capabilities page).
- **Impact:** some labels may be faint or missing. The one that matters is J1's polarity, because J1 is wired backwards from convention (E7).
- **Workaround:** mark J1's polarity on each board by hand.
- **Rev B:** lines at least 0.15 mm, text at least 1.0 mm, larger J1 marks.

## E7: J1 polarity convention

- **Issue:** J1 pin 1 is JACK_N (negative), pin 2 is VIN_SW (positive). Most people assume pin 1 is positive.
- **Protection:** Q1 blocks reverse polarity, so a reversed plug just doesn't power up.
- **Rev B:** consider swapping the pins, or a polarized connector with large marks.

## Rev B candidate list

Not decisions; collected so nothing gets lost.

- Fix E1 to E7.
- A third mouse-bite tab: two 2 mm tabs holding a 70 mm front board is on the light side.
- If a 2.42 in display is chosen, room for a 3.3 V regulator (AMS1117-3.3 is in stock).
- Re-check every footprint against the inventory before ordering, and run the production-zip check again (slots, silkscreen widths, outline, NPTH).
