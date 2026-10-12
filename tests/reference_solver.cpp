// reference_solver.cpp - deliberately naive Connect Four solver used ONLY to
// verify the optimized solver. Plain 2D array, plain negamax, no pruning, no
// cache. Correct but slow: use it on positions with <= ~18 empty cells.
//
// Output score convention matches the main solver:
//   score = (W*H + 1 - stones_played_before_the_winning_move) / 2  for the winner
#include <algorithm>
#include <cstdio>
#include <string>

static int g[7][6], h[7], moves = 0;

static bool makesFour(int c, int p) {  // did the stone just dropped in column c win?
    int r = h[c] - 1;
    const int d[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    for (auto& v : d) {
        int n = 1;
        for (int s = -1; s <= 1; s += 2) {
            int x = c + s * v[0], y = r + s * v[1];
            while (x >= 0 && x < 7 && y >= 0 && y < 6 && g[x][y] == p) {
                n++;
                x += s * v[0];
                y += s * v[1];
            }
        }
        if (n >= 4) return true;
    }
    return false;
}

static int negamax(int p) {
    if (moves == 42) return 0;
    int best = -100;
    for (int c = 0; c < 7; c++) {
        if (h[c] == 6) continue;
        g[c][h[c]++] = p;
        int before = moves++;
        int s = makesFour(c, p) ? (43 - before) / 2 : -negamax(3 - p);
        moves--;
        h[c]--;
        g[c][h[c]] = 0;
        best = std::max(best, s);
    }
    return best;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: %s <move sequence>\n", argv[0]); return 1; }
    int p = 1;
    for (char ch : std::string(argv[1])) {
        int c = ch - '1';
        g[c][h[c]++] = p;
        moves++;
        p = 3 - p;
    }
    std::printf("%s %d\n", argv[1], negamax(p));
}
