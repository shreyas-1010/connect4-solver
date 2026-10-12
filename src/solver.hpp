// solver.hpp - negamax / alpha-beta solver with a transposition table.
#pragma once
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

#include "position.hpp"

class Solver {
    struct Entry {
        u64 key;
        int8_t val;
        uint8_t flag;  // 1 = lower bound, 2 = upper bound, 3 = exact
    };
    static constexpr size_t TT_BITS = 22;  // 4M entries ~ 64 MB
    std::vector<Entry> tt;
    std::array<int, W> order;

    Entry& slot(u64 key) { return tt[(key * 0x9E3779B97F4A7C15ULL) >> (64 - TT_BITS)]; }

    int negamax(const Position& P, int alpha, int beta) {
        nodes++;
        if (P.canWinNext()) return (W * H + 1 - P.moves) / 2;

        u64 possible = P.nonLosingMoves();
        if (!possible) return -(W * H - P.moves) / 2;  // every move loses
        if (P.moves >= W * H - 2) return 0;            // draw

        // Static bounds: can't lose before opp's next move, can't win before ours.
        int lo = -(W * H - 2 - P.moves) / 2;
        if (alpha < lo) { alpha = lo; if (alpha >= beta) return alpha; }
        int hi = (W * H - 1 - P.moves) / 2;
        if (beta > hi) { beta = hi; if (alpha >= beta) return beta; }

        u64 key = P.key();
        Entry& e = slot(key);
        if (e.key == key && e.flag) {
            if (e.flag & 1) alpha = std::max(alpha, (int)e.val);
            if (e.flag & 2) beta = std::min(beta, (int)e.val);
            if (e.flag == 3 || alpha >= beta) return e.val;
        }

        // Order moves: most own threats first, ties broken by centre preference.
        std::array<std::pair<int, int>, W> mv;  // (score, col)
        int n = 0;
        for (int i = 0; i < W; i++) {
            int c = order[i];
            u64 m = possible & colMask(c);
            if (!m) continue;
            int sc = __builtin_popcountll(winningCells(P.cur | m, P.mask));
            int j = n++;
            while (j > 0 && mv[j - 1].first < sc) { mv[j] = mv[j - 1]; j--; }
            mv[j] = {sc, c};
        }

        int alpha0 = alpha;
        for (int i = 0; i < n; i++) {
            Position P2 = P;
            P2.play(mv[i].second);
            int score = -negamax(P2, -beta, -alpha);
            if (score >= beta) {
                Entry& s = slot(key);
                s = {key, (int8_t)score, 1};
                return score;
            }
            if (score > alpha) alpha = score;
        }
        Entry& s = slot(key);
        s = {key, (int8_t)alpha, (uint8_t)(alpha > alpha0 ? 3 : 2)};
        return alpha;
    }

    // Depth-limited alpha-beta with a heuristic evaluation (used in the opening,
    // where exact solving takes minutes).
    int heuristic(const Position& P, int depth, int alpha, int beta) {
        if (P.canWinNext()) return 1000 + depth;
        u64 possible = P.nonLosingMoves();
        if (!possible) return -1000 - depth;
        if (P.moves >= W * H - 2) return 0;
        if (depth == 0) {
            u64 opp = P.cur ^ P.mask;
            int threats = __builtin_popcountll(P.winning()) - __builtin_popcountll(P.opponentWinning());
            int centre = 3 * (__builtin_popcountll(P.cur & colMask(3)) - __builtin_popcountll(opp & colMask(3)))
                       + 2 * (__builtin_popcountll(P.cur & (colMask(2) | colMask(4))) -
                              __builtin_popcountll(opp & (colMask(2) | colMask(4))));
            return 8 * threats + centre;
        }
        for (int i = 0; i < W; i++) {
            int c = order[i];
            if (!(possible & colMask(c))) continue;
            Position P2 = P;
            P2.play(c);
            alpha = std::max(alpha, -heuristic(P2, depth - 1, -beta, -alpha));
            if (alpha >= beta) break;
        }
        return alpha;
    }

public:
    unsigned long long nodes = 0;

    Solver() : tt(1ULL << TT_BITS) {
        for (int i = 0; i < W; i++) order[i] = W / 2 + (1 - 2 * (i % 2)) * (i + 1) / 2;
        reset();
    }
    void reset() { std::memset(tt.data(), 0, tt.size() * sizeof(Entry)); nodes = 0; }

    // Exact score (or just win/draw/loss sign if `weak`).
    int solve(const Position& P, bool weak = false) {
        if (P.canWinNext()) return (W * H + 1 - P.moves) / 2;
        int lo = -(W * H - P.moves) / 2, hi = (W * H + 1 - P.moves) / 2;
        if (weak) { lo = -1; hi = 1; }
        while (lo < hi) {  // binary search with null-window probes
            int med = lo + (hi - lo) / 2;
            if (med <= 0 && lo / 2 < med) med = lo / 2;
            else if (med >= 0 && hi / 2 > med) med = hi / 2;
            int r = negamax(P, med, med + 1);
            if (r <= med) hi = r; else lo = r;
        }
        return lo;
    }

    // Score of every column from the mover's perspective (-1000 = illegal).
    // Exact when `exact`, otherwise heuristic (opening phase).
    std::array<int, W> analyze(const Position& P, bool exact = true) {
        std::array<int, W> out;
        for (int c = 0; c < W; c++) {
            if (!P.canPlay(c)) { out[c] = -1000; continue; }
            if (P.isWinningMove(c)) { out[c] = (W * H + 1 - P.moves) / 2; continue; }
            Position P2 = P;
            P2.play(c);
            out[c] = exact ? -solve(P2) : -heuristic(P2, 9, -100000, 100000);
        }
        return out;
    }
};

