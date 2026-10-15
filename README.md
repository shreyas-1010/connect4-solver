# Connect Four Solver (C++17)

A perfect-play solver for standard 7x6 Connect Four. Given any position it computes the exact
game-theoretic outcome (win / draw / loss and how fast). It can play against you, and it includes
a parallel, resumable **opening-book generator** with built-in verification.
No dependencies beyond the C++ standard library.

## Features

- **Bitboard representation**: the whole position is two 64-bit integers.
- **Branch-free win detection**: bit-shift tricks find every winning cell at once.
- **Negamax + alpha-beta pruning** with **forced-move / losing-move pruning** (never play under an
  opponent's threat, always block a single threat, detect unstoppable double threats).
- **Move ordering**: centre columns first, then moves that create the most threats.
- **Transposition table**: one 64-bit word per entry (key, score, bound type), exact / lower / upper bounds.
- **Null-window binary search** over the score range.
- **Opening book**: exact scores for every position up to a chosen ply, mirror images stored once.
  Multi-threaded, resumable generator; the upper layers are derived by minimax with no extra search.
- **Included book**: `book.bin` (depth 6, 11,094 positions, 86 KB) so the AI plays well out of the box.
- **Verification tooling**: consistency check of every book entry plus a cross-check against a fresh search.
- **Play mode**: perfect play from move 1 with a book; without one, a heuristic search for the first
  few plies and exact play afterwards.

## Quick start

```bash
make              # builds ./connect4
make test         # regression tests + opening-book tests
./connect4 solve 4453
./connect4 play
```

Requires `g++` or `clang++` with C++17 and `make`.

## Usage

### Solve a position

Positions are move sequences: each digit is the column (1-7) played, alternating players,
first player first.

```
$ ./connect4 solve 4453 --no-book
4453  score=-2  nodes=73064758  time=6711.96ms
```

The score is from the point of view of the player **to move**:

| score | meaning |
|-------|---------|
| `> 0` | side to move wins; larger = faster win. `1` = win with the very last stone |
| `0`   | draw with perfect play |
| `< 0` | side to move loses; more negative = faster loss |

A win after which the winner still holds `k` unplayed stones scores `k + 1`.

Without a position argument, `solve` reads one sequence per line from stdin. `--weak` computes only
the sign (win / draw / loss). If a `book.bin` is present (current directory, or next to the
executable) it is used automatically; `--book FILE` picks a file and `--no-book` disables it.

### Play against the solver

```bash
./connect4 play        # you move first (X)
./connect4 play ai     # the AI moves first
```

Enter a column from 1 to 7. The AI prints its per-column scores before it moves.

- **With a book** (the included `book.bin` is picked up automatically), the AI plays perfectly from
  its first move. The depth-6 book covers plies 0-6; for the next two plies the AI runs an exact
  search instead, which can take seconds to a minute or so before it moves.
- **Without a book**, the AI uses a depth-limited heuristic search for its first few moves and exact
  solving from ply 8 on (`EXACT_FROM` in `src/main.cpp`), because exact search from the opening
  takes too long for interactive play. It can therefore be beaten early on.

## Opening book

Exact solving is slow for the first moves of a game, because those positions have the most game
left. A book stores precomputed exact scores for every position up to a chosen ply, so lookups
take microseconds.

### How it is built

1. Enumerate every reachable, unfinished position down to the chosen depth. Mirror images are
   merged, which halves each layer. (Ply 8 has 90,289 positions.)
2. Solve **only the deepest layer** with the full alpha-beta solver. This is the expensive part. It
   runs on several threads, each keeping its transposition table across positions, and every
   result is appended to `<out>.part` as it is found, so an interrupted run resumes.
3. Derive **every shallower layer by minimax** over the layer below,
   `score(P) = max over moves of -score(child)`. No search is needed.

### Building one

```bash
make book                            # depth 6 (BOOK_DEPTH) on all cores -> book.bin
make book BOOK_DEPTH=8
./connect4 genbook 7 --threads 8 --out book.bin
./connect4 genbook 7 --threads 8 --limit 2000     # solve at most 2000 more positions, then stop;
                                                  # re-run the same command to continue
./connect4 verifybook --book book.bin             # check the result
```

The generator prints progress and an ETA every 30 seconds. It is safe to interrupt with Ctrl-C
and re-run. Progress is saved to `book.bin.part`.

### The included book

`book.bin` is a depth-6 book: all 11,094 positions up to ply 6, mirror images merged
(1 + 4 + 25 + 121 + 568 + 2,144 + 8,231 positions per ply), 86 KB. It took 80 minutes (4,799 s) to
build on an Intel Core i5-8265U with the default thread count. The generator reports a root score
of `1`, the known result that Connect Four is a first-player win (with the very last stone).

Deeper books cost much more. Solving the ply-8 layer (90,289 positions) would take roughly 28-37
core-hours by my estimate from random samples of 40 and 60 positions (about 1.1-1.5 s each on a
2.8 GHz Xeon core); that figure is an extrapolation, not a completed run. A depth-7 book would be
about 38,000 entries (~300 KB) and depth 8 about 128,000 entries (~1 MB).

### Verification

`verifybook` runs two independent checks:

1. **Consistency**: every entry equals the minimax of its children's entries (or the immediate-win
   score), which catches enumeration and derivation bugs.
2. **Search cross-check**: a random sample of entries is compared against a fresh full search with
   no book attached.

The generator prints the root (empty-board) score. For the included book it is `1`, matching the
known result: perfect play is a first-player win, and in this scoring a win with the very last
stone is `1`.

`verifybook` on the included `book.bin`: 3,456 entries consistent with their children (0 problems);
20 sampled entries matched a fresh search (0 mismatches).

## Benchmarks

Measured on an Intel Core i5-8265U (single thread), default `make` flags (`-O3`), no book, cold
transposition table. Timings vary by run; node counts are deterministic.

| position (moves played) | stones | score | nodes | time |
|-------------------------|--------|-------|-------|------|
| `444444`                | 6      | 1     | 44.6M | 4.1 s |
| `4435`                  | 4      | -2    | 63.8M | 5.8 s |
| `4453`                  | 4      | -2    | 73.1M | 6.7 s |

With the included book, positions up to ply 6 are answered by lookup (`nodes=0`, microseconds).

### What did not help (measured)

- A mirror-symmetric transposition-table key halved the node count on one symmetric position but
  made the average ply-8 position about 12% *slower*, so the search uses the plain key. Mirror
  merging is used for book keys only.
- A best-move ("hash move") heuristic and a larger transposition table gave no measurable gain.

## How it works

**Bitboards.** Each column uses 7 bits (6 cells plus a sentinel bit), so shifting never wraps into
the next column. `cur` holds the stones of the player to move and `mask` all stones; the opponent's
stones are `cur ^ mask`. `cur + mask` is a unique key for the position. Dropping a stone is
`mask |= mask + bottom(col)`. The key can be decoded back into a position, which `verifybook` uses.

**Win detection.** Shift the player's stones by 1 (vertical), 7 (horizontal), 6 and 8 (diagonals)
and AND the results to find every cell that would complete four-in-a-row, in about 30 bit
operations and with no loops over the board.

**Pruning.** If the player to move can win immediately, return. If the opponent has two threats the
position is lost. If the opponent has one threat, blocking it is the only move worth considering.
Moves directly underneath an opponent's winning cell are never played.

**Search.** Negamax with alpha-beta, static score bounds from the number of stones left, and a
transposition table. The outer loop binary-searches the exact score with null-window probes
(`negamax(P, med, med+1)`), which is much cheaper than a full-window search.

## Project layout

```
src/position.hpp    bitboard position, move generation, win detection, key encode/decode
src/solver.hpp      negamax / alpha-beta, transposition table, book lookup, heuristic search
src/book.hpp        opening-book storage, lookup and file format
src/bookgen.hpp     book generator (layer enumeration, parallel solve, minimax derivation)
src/main.cpp        command-line interface (solve / play / genbook / verifybook)
tests/              regression positions, reference solver, cross-check, book tests
Makefile
```

## Testing

```bash
make test         # 30 regression positions + opening-book tests
make crosscheck   # random positions: optimized solver vs. naive brute-force reference
```

- `tests/positions.txt`: expected scores generated by `tests/reference_solver.cpp`, a naive solver
  with no pruning or caching, so the optimized solver is checked against independent code.
- `tests/book_test.sh` (runs in seconds, using a tree rooted at a mid-game position): books built
  with 1 and 3 threads are byte-identical; every entry is consistent with its children; sampled
  entries match a fresh search; the root score with the book equals the score without it; and an
  interrupted build (`--limit`) resumes to a result identical to an uninterrupted one.

## Future work

- A deeper book (ply 7-8), which needs a multi-core machine for many hours.
- Faster search: better move ordering and a smarter transposition-table replacement scheme.
- Outcome-only (win/draw/loss) books, which are about 2x cheaper to build, at the cost of not
  preferring the fastest win.
- Distributed book generation.

## Credits

This project implements published techniques; the ideas belong to their authors:

- Pascal Pons, ["Solving Connect 4: how to build a perfect AI"](http://blog.gamesolver.org) -
  the bitboard layout, win-detection shifts, non-losing-move pruning, score convention and
  binary-search structure follow this tutorial.
- [Chessprogramming wiki](https://www.chessprogramming.org) - alpha-beta, transposition tables,
  move ordering.
- John Tromp's work on solving Connect-4.

The code in this repository is an independent implementation (own transposition table, heuristic
opening search, book generator and verifier, CLI, tests).