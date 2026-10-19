#!/usr/bin/env bash
# Run the full comparison battery: oracle (real PICO-8) vs pico-c.
export LC_ALL=C
set -u
pass=0; fail=0; failed=()
run() {
  local cart="$1"
  local oracle ours
  oracle=$(mktemp /tmp/oracle.XXXXXX)
  ours=$(mktemp /tmp/ours.XXXXXX)
  ( /tmp/p8app/Contents/MacOS/pico8 -x "$PWD/$cart" > "$oracle" 2>&1 & )
  sleep 6
  pkill -f p8app
  ./pico-c.exe -headless "$cart" > "$ours" 2>&1
  if diff <(grep -v '^RUNNING' "$oracle" | sed '/^$/d') \
          <(grep -v '^DEBUG' "$ours" | sed '/^$/d') > /tmp/cmp.diff; then
    echo "MATCH: $cart"; pass=$((pass+1))
  else
    echo "DIFF : $cart"; fail=$((fail+1)); failed+=("$cart")
    cat /tmp/cmp.diff
  fi
  rm -f "$oracle" "$ours"
}

for c in "$@"; do run "$c"; done
echo "---- $pass matched, $fail differed"
[ $fail -eq 0 ] || printf 'differed: %s\n' "${failed[*]}"
