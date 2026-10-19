#!/usr/bin/env bash
set -u
cart="${1:?cart path}"
off=$(/Applications/PICO-8.app/Contents/MacOS/pico8 -x "$cart" 2>&1 | sed -E 's/^RUNNING:.*//; /^$/d')
pc=$(./pico-c.exe -headless "$cart" 2>&1 | sed -E 's/^DEBUG:.*//; /^$/d')
if diff <(printf '%s\n' "$off") <(printf '%s\n' "$pc") >/tmp/cmp.diff; then
  echo "MATCH: $cart"
else
  echo "DIFF: $cart"
  cat /tmp/cmp.diff
fi