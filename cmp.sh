#!/usr/bin/env bash
# Compare one cart between the oracle (real PICO-8) and pico-c.
# /Applications/PICO-8.app hangs on this machine, so the oracle runs from a
# signed copy in /tmp/p8app (see progress.md section 0).
export LC_ALL=C
set -u
cart="${1:?cart path}"
oracle=$(mktemp /tmp/oracle.XXXXXX)
ours=$(mktemp /tmp/ours.XXXXXX)
( /tmp/p8app/Contents/MacOS/pico8 -x "$PWD/$cart" > "$oracle" 2>&1 & )
sleep 6
pkill -f p8app
./pico-c.exe -headless "$cart" > "$ours" 2>&1
if diff <(grep -v '^RUNNING' "$oracle" | sed '/^$/d') \
        <(grep -v '^DEBUG' "$ours" | sed '/^$/d') > /tmp/cmp.diff; then
  echo "MATCH: $cart"
else
  echo "DIFF: $cart"
  cat /tmp/cmp.diff
fi
rm -f "$oracle" "$ours"
