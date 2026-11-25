#!/usr/bin/env bash
# Headless build of firmware/stepper_tester the way the Arduino IDE does it:
# core.a + sketch, LTO, --gc-sections, warnings on for the sketch.
# Used to check flash/RAM and warnings after every change, and to feed the simulator.
#
# usage: tools/build.sh [minicore|arduino] [extra CPPFLAGS...]
#   minicore (default)  MiniCore "ATmega328", 16 MHz external: the settings the real board uses
#   arduino             stock Arduino AVR core, board "Arduino Uno" (the no-MiniCore fallback)
#
# Needs: avr-gcc 7.3 (Debian/Ubuntu: apt install gcc-avr avr-libc binutils-avr)
#        and tools/fetch_deps.sh run once (pinned ArduinoCore-avr and MiniCore sources).
# Output: build/<core>/fw.elf and fw.hex at the repo root; sizes printed at the end.
#
# Reference sizes (avr-gcc 7.3.0): CLAUDE.md, Firmware checks.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CORE=${1:-minicore}; shift || true
EXTRA="$*"
D=${DEPS:-$ROOT/tools/.deps}
SK=$ROOT/firmware/stepper_tester/stepper_tester.ino
OUT=$ROOT/build/$CORE

case "$ROOT$D" in *" "*) echo "build.sh: paths with spaces are not supported ($ROOT)" >&2; exit 1;; esac
for dep in "$D/ArduinoCore-avr" "$D/MiniCore"; do
  [ -d "$dep" ] || { echo "missing $dep: run tools/fetch_deps.sh first" >&2; exit 1; }
done
command -v avr-gcc >/dev/null || { echo "avr-gcc not found (apt install gcc-avr avr-libc binutils-avr)" >&2; exit 1; }

mkdir -p "$OUT/core" "$OUT/sk"
if [ "$CORE" = arduino ]; then
  CDIR=$D/ArduinoCore-avr/cores/arduino; VDIR=$D/ArduinoCore-avr/variants/standard
  WDIR=$D/ArduinoCore-avr/libraries/Wire/src; SDIR=$D/ArduinoCore-avr/libraries/SPI/src
  DEFS="-DARDUINO=10819 -DARDUINO_AVR_UNO -DARDUINO_ARCH_AVR"; CXXSTD=-std=gnu++11
else
  CDIR=$D/MiniCore/avr/cores/MCUdude_corefiles; VDIR=$D/MiniCore/avr/variants/standard
  WDIR=$D/MiniCore/avr/libraries/Wire/src; SDIR=$D/MiniCore/avr/libraries/SPI/src
  DEFS="-DARDUINO=10819 -DARDUINO_AVR_ATmega328 -DARDUINO_ARCH_AVR"; CXXSTD=-std=gnu++17
fi
COMMON="-mmcu=atmega328p -DF_CPU=16000000L $DEFS -Os -g -flto -ffunction-sections -fdata-sections $EXTRA"
INC="-I$CDIR -I$VDIR -I$SDIR -I$WDIR -I$WDIR/utility"
CF="$COMMON -w -std=gnu11 -fno-fat-lto-objects $INC"
XF="$COMMON -w $CXXSTD -fpermissive -fno-exceptions -fno-threadsafe-statics -Wno-error=narrowing $INC"

# core is cached between builds (delete build/<core> to force a rebuild)
if [ ! -f "$OUT/core.a" ]; then
  : > "$OUT/jobs"
  for f in "$CDIR"/*.c;   do echo "avr-gcc $CF -c $f -o $OUT/core/$(basename "$f").o"; done >> "$OUT/jobs"
  for f in "$CDIR"/*.cpp; do echo "avr-g++ $XF -c $f -o $OUT/core/$(basename "$f").o"; done >> "$OUT/jobs"
  for f in "$CDIR"/*.S;   do echo "avr-gcc -mmcu=atmega328p -DF_CPU=16000000L $DEFS -flto -c -x assembler-with-cpp $INC $f -o $OUT/core/$(basename "$f").o"; done >> "$OUT/jobs"
  xargs -d '\n' -P 4 -I{} sh -c '{}' < "$OUT/jobs"
  avr-gcc-ar rcs "$OUT/core.a" "$OUT"/core/*.o
fi

# sketch: warnings on, as the Arduino IDE would see it (Arduino.h first)
{ echo '#include <Arduino.h>'; echo '#line 1 "stepper_tester.ino"'; cat "$SK"; } > "$OUT/sk/sketch.cpp"
avr-g++ ${XF/-w /} -Wall -Wextra -c "$OUT/sk/sketch.cpp" -o "$OUT/sk/sketch.o"
avr-gcc -w -Os -g -flto -fuse-linker-plugin -Wl,--gc-sections -mmcu=atmega328p \
  -o "$OUT/fw.elf" "$OUT/sk/sketch.o" "$OUT/core.a" -L"$OUT" -lm
avr-objcopy -O ihex -R .eeprom "$OUT/fw.elf" "$OUT/fw.hex"
echo "[$CORE] $(avr-size -C --mcu=atmega328p "$OUT/fw.elf" | grep -E 'Program|Data' | tr -s ' ' | tr '\n' ' ')"
