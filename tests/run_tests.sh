#!/usr/bin/env bash
# Runs the solver on every line of tests/positions.txt ("<moves> <expected score>")
# and checks the result. Run from the repo root:  make test
cd "$(dirname "$0")/.." || exit 1
[ -x ./connect4 ] || { echo "build first: make"; exit 1; }

pass=0; fail=0
while read -r seq expected; do
  [ -z "$seq" ] && continue
  case "$seq" in \#*) continue;; esac
  got=$(./connect4 solve --no-book "$seq" | sed -n 's/.*score=\(-\{0,1\}[0-9]*\).*/\1/p')
  if [ "$got" == "$expected" ]; then pass=$((pass+1)); else
    fail=$((fail+1)); echo "FAIL $seq expected=$expected got=$got"; fi
done < tests/positions.txt
echo "passed=$pass failed=$fail"
[ "$fail" -eq 0 ]
