#!/usr/bin/env bash
# Regression run: build the firmware (MiniCore settings), run every simulator scenario,
# and compare each one's self-test result with what it must report.
#
# usage: tools/sim/run_all.sh [scenario ...]     (default: all 20)
# Prereqs: tools/fetch_deps.sh, tools/sim/setup.sh (once).
# Output:  build/sim/out/<scenario>.log (decoded debug log), .stdout, .pgm screen dumps.
# Exit 1 if any scenario differs from its expected result.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SIM=$ROOT/build/sim/sim2
[ -x "$SIM" ] || { echo "run tools/sim/setup.sh first" >&2; exit 1; }

"$ROOT/tools/build.sh" minicore
ELF=$ROOT/build/minicore/fw.elf
export SIM_OUT=$ROOT/build/sim/out
mkdir -p "$SIM_OUT"

# scenario | expected "result:" line | expected flagged tests (TEST=detail, sorted)
EXPECT='
happy|result: 0 fail, 0 warn.|
step|result: 0 fail, 0 warn.|
steplow|result: 0 fail, 0 warn.|
novm|result: 0 fail, 1 warn.|VM=0.00V
addr2|result: 1 fail, 0 warn.|LINK=addr
noecho|result: 1 fail, 0 warn.|UART=no
linelow|result: 1 fail, 0 warn.|UART=line
notmc|result: 1 fail, 0 warn.|LINK=no
nodiag|result: 0 fail, 1 warn.|DIAG=not
enbroken|result: 1 fail, 0 warn.|PINS=EN
dirbroken|result: 1 fail, 0 warn.|PINS=DIR
stepbroken|result: 1 fail, 0 warn.|PINS=STP
nooled|result: 1 fail, 0 warn.|OLED=no
short|result: 1 fail, 0 warn.|OUT=SHORT
tmc2208|result: 1 fail, 0 warn.|DRV=ver
nomotor|result: 0 fail, 0 warn.|
coilA|result: 0 fail, 0 warn.|
vmdrop|result: 0 fail, 0 warn.|
holdboot|result: 0 fail, 0 warn.|
clk8|result: 1 fail, 0 warn.|CLK=~8
'
WANT="$*"
bad=0; n=0
while IFS='|' read -r sc want_r want_f; do
  [ -z "$sc" ] && continue
  if [ -n "$WANT" ] && ! echo " $WANT " | grep -q " $sc "; then continue; fi
  rm -f "$SIM_OUT/$sc".* "$SIM_OUT/${sc}_"*.pgm
  set +e; timeout 900 "$SIM" "$ELF" "$sc" > "$SIM_OUT/$sc.stdout" 2>&1; rc=$?; set -e
  r=$(grep -m1 "result:" "$SIM_OUT/$sc.log" 2>/dev/null | sed 's/  [0-9]*mA.*//')
  fl=$(awk '/self test ===/{n++} n>=1' "$SIM_OUT/$sc.log" 2>/dev/null | grep -E " XX | !\? " | awk '{print $2"="$4}' | sort -u | tr '\n' ' ' | sed 's/ $//')
  n=$((n+1))
  if [ "$rc" = 0 ] && [ "$r" = "$want_r" ] && [ "$fl" = "$want_f" ]; then
    printf "  ok    %-10s %-26s %s\n" "$sc" "$r" "$fl"
  else
    printf "  DIFF  %-10s rc=%s got [%s] [%s] want [%s] [%s]\n" "$sc" "$rc" "$r" "$fl" "$want_r" "$want_f"
    bad=$((bad+1))
  fi
done <<< "$EXPECT"

grep -h "stack:" "$SIM_OUT"/*.stdout 2>/dev/null | sort -t'>' -k2 -n | tail -1 | sed 's/^/  worst /'
grep -h "steady STEP\|expect 3.333\|expect 6.667" "$SIM_OUT"/step.stdout "$SIM_OUT"/steplow.stdout 2>/dev/null | sed 's/^ */  /' || true
echo "$n scenarios, $bad differ"
[ "$bad" = 0 ]
