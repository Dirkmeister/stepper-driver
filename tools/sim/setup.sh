#!/usr/bin/env bash
# One-time setup for the board simulator:
#   1. fetch the pinned simavr source (tools/fetch_deps.sh sim)
#   2. apply simavr-icr1-write.patch (Timer1 picks up a new ICR1 TOP when it is written,
#      as real hardware does; stock simavr only re-reads it on a mode change)
#   3. build libsimavr and the harness -> build/sim/sim2
# Needs: gcc, make, libelf-dev, pkg-config (Debian/Ubuntu: apt install build-essential libelf-dev pkg-config)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
D=${DEPS:-$ROOT/tools/.deps}
B=$ROOT/build/sim

"$ROOT/tools/fetch_deps.sh" sim
S=$D/simavr
if git -C "$S" apply --check "$HERE/simavr-icr1-write.patch" 2>/dev/null; then
  git -C "$S" apply "$HERE/simavr-icr1-write.patch"
  echo "patched simavr (ICR1 write)"
elif git -C "$S" apply -R --check "$HERE/simavr-icr1-write.patch" 2>/dev/null; then
  echo "simavr already patched"
else
  echo "simavr patch does not apply: check $S" >&2; exit 1
fi

mkdir -p "$B/inc" "$B/out"
echo "building libsimavr (a minute or two the first time)"
make -C "$S/simavr" RELEASE=1 -j"$(nproc)" > "$B/libsimavr.log" 2>&1 || { tail -20 "$B/libsimavr.log"; exit 1; }
OBJ=$S/simavr/obj-$(gcc -dumpmachine)
ln -sfn "$S/simavr/sim" "$B/inc/simavr"
gcc -g -O2 -Wall -Wno-unused-function -Wno-unused-variable -I"$B/inc" -I"$S/simavr/sim" -I"$OBJ" \
  -o "$B/sim2" "$HERE/sim.c" "$OBJ/libsimavr.a" -lelf -lm
echo "built $B/sim2"
