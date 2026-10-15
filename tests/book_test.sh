#!/usr/bin/env bash
# Opening-book tests, on a small tree rooted at a mid-game position so that it runs in seconds.
# (A full book from the empty board takes hours; see README.)
#   1. build the same book with 1 and with 3 threads  -> files must be byte-identical
#   2. verifybook: every entry is consistent with its children, and sampled entries match a
#      fresh book-less search
#   3. solving a book position with the book returns the same score as a full search
#   4. an interrupted build can be resumed and still produces a correct book
cd "$(dirname "$0")/.." || exit 1
[ -x ./connect4 ] || { echo "build first: make"; exit 1; }
ROOT=635464334346437421   # 18 stones
DEPTH=26
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
fail() { echo "FAIL: $1"; exit 1; }

./connect4 genbook $DEPTH --root $ROOT --threads 1 --tt-bits 20 --out "$T/a.bin" >/dev/null || fail "genbook (1 thread)"
./connect4 genbook $DEPTH --root $ROOT --threads 3 --tt-bits 20 --out "$T/b.bin" >/dev/null || fail "genbook (3 threads)"
cmp -s "$T/a.bin" "$T/b.bin" || fail "books from 1 and 3 threads differ"
echo "ok: 1-thread and 3-thread books are identical ($(stat -c%s "$T/a.bin" 2>/dev/null || stat -f%z "$T/a.bin") bytes)"

./connect4 verifybook --book "$T/a.bin" --sample 400 --min-ply 0 2>/dev/null | tee "$T/v.txt"
grep -q "consistency: .* 0 problems" "$T/v.txt" || fail "consistency check"
grep -q "cross-check: .* 0 mismatches" "$T/v.txt" || fail "search cross-check"

with=$(./connect4 solve $ROOT --book "$T/a.bin" 2>/dev/null | sed -n 's/.*score=\(-\{0,1\}[0-9]*\).*/\1/p')
without=$(./connect4 solve $ROOT --no-book | sed -n 's/.*score=\(-\{0,1\}[0-9]*\).*/\1/p')
[ "$with" == "$without" ] || fail "book score $with != search score $without"
echo "ok: root score with book ($with) == without book ($without)"

# resume: stop a build after 700 positions (--limit), then finish it from the saved progress
./connect4 genbook $((DEPTH+2)) --root $ROOT --threads 2 --tt-bits 20 --limit 700 --out "$T/r.bin" >/dev/null
[ $? -eq 2 ] || fail "--limit should stop early with exit code 2"
[ -f "$T/r.bin.part" ] || fail "no progress file written"
./connect4 genbook $((DEPTH+2)) --root $ROOT --threads 2 --tt-bits 20 --out "$T/r.bin" | tee "$T/g.txt" | grep "already solved"
grep -q "already solved (resumed): 7" "$T/g.txt" || fail "build did not resume from saved progress"
./connect4 genbook $((DEPTH+2)) --root $ROOT --threads 1 --tt-bits 20 --out "$T/full.bin" >/dev/null
cmp -s "$T/r.bin" "$T/full.bin" || fail "resumed book differs from an uninterrupted build"
./connect4 verifybook --book "$T/r.bin" --sample 200 --min-ply 0 2>/dev/null | tee "$T/v2.txt" >/dev/null
grep -q "consistency: .* 0 problems" "$T/v2.txt" && grep -q "cross-check: .* 0 mismatches" "$T/v2.txt" || fail "resumed book failed verification"
echo "ok: interrupted build resumed; result identical to an uninterrupted build and verified"
echo "ALL BOOK TESTS PASSED"
