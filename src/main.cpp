// main.cpp - command-line interface: solve positions or play against the solver.
#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>

#include "solver.hpp"

// ---------------------------------------------------------------- CLI helpers

static void solveOne(Solver& S, const std::string& seq, bool weak) {
    Position P;
    if (!P.playSequence(seq)) { std::cout << seq << "  invalid/finished position\n"; return; }
    S.reset();
    auto t0 = std::chrono::steady_clock::now();
    int score = S.solve(P, weak);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::cout << seq << "  score=" << score << "  nodes=" << S.nodes << "  time=" << ms << "ms\n";
}

static void printBoard(const char g[W][H]) {
    for (int r = H - 1; r >= 0; r--) {
        std::cout << " ";
        for (int c = 0; c < W; c++) std::cout << "| " << g[c][r] << " ";
        std::cout << "|\n";
    }
    std::cout << "  ";
    for (int c = 1; c <= W; c++) std::cout << " " << c << "  ";
    std::cout << "\n";
}

constexpr int EXACT_FROM = 14;  // plies played before the AI switches to perfect play

static void playGame(bool humanFirst) {
    Solver S;
    Position P;
    char g[W][H];
    for (auto& col : g) for (char& ch : col) ch = '.';
    int height[W] = {0};
    bool humanTurn = humanFirst;
    char humanSym = humanFirst ? 'X' : 'O', aiSym = humanFirst ? 'O' : 'X';

    auto drop = [&](int c, char sym) {
        g[c][height[c]++] = sym;
    };

    while (true) {
        printBoard(g);
        if (P.moves == W * H) { std::cout << "Draw!\n"; return; }
        int col;
        if (humanTurn) {
            while (true) {
                std::cout << "Your move (1-7): ";
                int x;
                if (!(std::cin >> x)) return;
                col = x - 1;
                if (col >= 0 && col < W && P.canPlay(col)) break;
                std::cout << "Illegal move.\n";
            }
        } else {
            std::cout << "AI thinking...\n" << std::flush;
            if (P.moves == 0) col = W / 2;  // known winning opening
            else {
                bool exact = P.moves >= EXACT_FROM;
                if (!exact) std::cout << "(opening: heuristic search; exact solving starts at move " << EXACT_FROM << ")\n";
                auto sc = S.analyze(P, exact);
                col = int(std::max_element(sc.begin(), sc.end()) - sc.begin());
                std::cout << "Column scores:";
                for (int c = 0; c < W; c++) std::cout << " " << (sc[c] == -1000 ? std::string("-") : std::to_string(sc[c]));
                std::cout << "\n";
            }
            std::cout << "AI plays " << col + 1 << "\n";
        }
        bool win = P.isWinningMove(col);
        drop(col, humanTurn ? humanSym : aiSym);
        P.play(col);
        if (win) {
            printBoard(g);
            std::cout << (humanTurn ? "You win!\n" : "AI wins!\n");
            return;
        }
        humanTurn = !humanTurn;
    }
}

int main(int argc, char** argv) {
    std::string mode = argc > 1 ? argv[1] : "solve";
    if (mode == "play") {
        bool humanFirst = !(argc > 2 && std::string(argv[2]) == "ai");
        playGame(humanFirst);
        return 0;
    }
    Solver S;
    bool weak = false;
    if (argc > 2 && std::string(argv[2]) != "--weak") {
        solveOne(S, argv[2], weak);
        return 0;
    }
    if (argc > 2) weak = true;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty()) solveOne(S, line, weak);
    }
    return 0;
}
