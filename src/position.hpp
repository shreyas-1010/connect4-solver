// position.hpp - bitboard representation of a Connect Four position (7x6).
//
// Layout: each column uses H+1 = 7 bits (6 cells + 1 sentinel bit on top), so
// shifting a stone can never "wrap" into the neighbouring column.
//
//   .  .  .  .  .  .  .   <- sentinel row (always empty)
//   5 12 19 26 33 40 47
//   4 11 18 25 32 39 46
//   3 10 17 24 31 38 45
//   2  9 16 23 30 37 44
//   1  8 15 22 29 36 43
//   0  7 14 21 28 35 42   <- bottom row
#pragma once
#include <cstdint>
#include <string>

using u64 = uint64_t;

constexpr int W = 7;  // columns
constexpr int H = 6;  // rows

constexpr u64 bottomRow() {
    u64 r = 0;
    for (int c = 0; c < W; c++) r |= 1ULL << (c * (H + 1));
    return r;
}
constexpr u64 BOTTOM = bottomRow();
constexpr u64 BOARD = BOTTOM * ((1ULL << H) - 1);

constexpr int MIN_SCORE = -(W * H) / 2 + 3;
constexpr int MAX_SCORE = (W * H + 1) / 2 - 3;

constexpr u64 colMask(int c) { return ((1ULL << H) - 1) << (c * (H + 1)); }
constexpr u64 botMask(int c) { return 1ULL << (c * (H + 1)); }
constexpr u64 topMask(int c) { return 1ULL << (H - 1 + c * (H + 1)); }

// All empty, playable cells that would complete four-in-a-row for `pos`.
inline u64 winningCells(u64 pos, u64 mask) {
    // vertical
    u64 r = (pos << 1) & (pos << 2) & (pos << 3);
    // horizontal and both diagonals: shifts of H+1, H, H+2
    for (int s : {H + 1, H, H + 2}) {
        u64 p = (pos << s) & (pos << 2 * s);
        r |= p & (pos << 3 * s);
        r |= p & (pos >> s);
        p = (pos >> s) & (pos >> 2 * s);
        r |= p & (pos << s);
        r |= p & (pos >> 3 * s);
    }
    return r & (BOARD ^ mask);
}

// Mirror a bitboard left-right (column c <-> column W-1-c).
inline u64 mirrorBits(u64 x) {
    u64 r = 0;
    for (int c = 0; c < W; c++)
        r |= ((x >> (c * (H + 1))) & ((1ULL << (H + 1)) - 1)) << ((W - 1 - c) * (H + 1));
    return r;
}

struct Position {
    u64 cur = 0;   // stones of the player to move
    u64 mask = 0;  // all stones
    int moves = 0;

    bool canPlay(int c) const { return !(mask & topMask(c)); }
    void play(int c) {
        cur ^= mask;            // switch perspective
        mask |= mask + botMask(c);
        moves++;
    }
    u64 key() const { return cur + mask; }  // unique per position (fits in 49 bits)
    // A column holds k stones, so mask_col = 2^k - 1 and key_col = mask_col + cur_col never carries
    // into the next column. Mirroring the key therefore equals the key of the mirrored position,
    // and a position and its mirror image share one canonical key (and the same score).
    u64 canonicalKey() const {
        u64 k = key(), m = mirrorBits(k);
        return k < m ? k : m;
    }
    // Inverse of key(): rebuild a position from its key.
    static Position fromKey(u64 key) {
        Position P;
        for (int c = 0; c < W; c++) {
            u64 v = (key >> (c * (H + 1))) & ((1ULL << (H + 1)) - 1);
            int k = 63 - __builtin_clzll(v + 1);  // stones in this column
            u64 m = (1ULL << k) - 1;
            P.mask |= m << (c * (H + 1));
            P.cur |= (v - m) << (c * (H + 1));
        }
        P.moves = __builtin_popcountll(P.mask);
        return P;
    }
    u64 possible() const { return (mask + BOTTOM) & BOARD; }
    u64 winning() const { return winningCells(cur, mask); }
    u64 opponentWinning() const { return winningCells(cur ^ mask, mask); }
    bool isWinningMove(int c) const { return winning() & possible() & colMask(c); }
    bool canWinNext() const { return winning() & possible(); }

    // Moves that don't immediately lose; 0 if every move loses.
    u64 nonLosingMoves() const {
        u64 poss = possible();
        u64 opp = opponentWinning();
        u64 forced = poss & opp;
        if (forced) {
            if (forced & (forced - 1)) return 0;  // two threats: can't block both
            poss = forced;
        }
        return poss & ~(opp >> 1);  // don't play below an opponent's winning cell
    }

    // Returns false on illegal input or if the game ended early.
    bool playSequence(const std::string& s) {
        for (char ch : s) {
            if (ch < '1' || ch > '0' + W) return false;
            int c = ch - '1';
            if (!canPlay(c) || isWinningMove(c)) return false;
            play(c);
        }
        return true;
    }
};

