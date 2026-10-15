#!/usr/bin/env bash
# Fetch the exact third-party sources the build and the simulator were verified with,
# into tools/.deps (gitignored). Safe to re-run.
#
# usage: tools/fetch_deps.sh         ArduinoCore-avr + MiniCore (enough for tools/build.sh)
#        tools/fetch_deps.sh sim     also simavr (for tools/sim)
#
# Pinned commits = what was current when firmware v1.0 was built and simulated (2026-10-12).
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
D=${DEPS:-$ROOT/tools/.deps}
mkdir -p "$D"

fetch() {   # name url commit
  local name=$1 url=$2 rev=$3
  if [ ! -d "$D/$name/.git" ]; then
    echo "cloning $name"
    git clone -q "$url" "$D/$name"
  fi
  if [ "$(git -C "$D/$name" rev-parse HEAD)" != "$rev" ]; then
    git -C "$D/$name" fetch -q origin || true
    git -C "$D/$name" -c advice.detachedHead=false checkout -q "$rev"
  fi
  echo "$name @ $(git -C "$D/$name" rev-parse --short HEAD)"
}

fetch ArduinoCore-avr https://github.com/arduino/ArduinoCore-avr.git 11b9130371e8447920edb65a75706a6c951e51fc
fetch MiniCore        https://github.com/MCUdude/MiniCore.git        d62c83555b66b7650e635e52415b6a436a342bb5
if [ "$1" = sim ]; then
  fetch simavr        https://github.com/buserror/simavr.git          557b07441f02a41a8bc7f95edcb16c24b2f9eeda
fi
