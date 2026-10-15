// main.cpp - command-line interface: solve positions, play against the solver, build/verify books.
#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <thread>

#include "bookgen.hpp"
#include "solver.hpp"

constexpr int EXACT_FROM = 8;  // without a book: ply from which the AI plays exact (perfect) moves

static const char* USAGE =
    "usage:\n"
    "  connect4 solve [SEQ] [--weak] [--book FILE | --no-book]\n"
    "      SEQ = moves as column digits 1-7 (e.g. 4453); without SEQ, read one per line from stdin\n"
    "  connect4 play [human|ai] [--book FILE | --no-book]\n"
    "  connect4 genbook DEPTH [--threads N] [--out FILE] [--limit N] [--root SEQ] [--tt-bits B]\n"
    "      --limit N: solve at most N more positions, save progress and stop (re-run to continue)\n"
    "  connect4 verifybook [--book FILE] [--sample N] [--min-ply P]\n";

struct Args {
    std::vector<std::string> pos;  // positional arguments
    std::string book = "";         // explicit book path
    bool noBook = false, weak = false;
    int threads = 0, sample = 20, minPly = -1, ttBits = 23;
    long limit = 0;
    std::string out = "book.bin", root;
};

static Args parse(int argc, char** argv) {
    Args a;
    for (int i = 2; i < argc; i++) {
        std::string s = argv[i];
        auto val = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (s == "--no-book") a.noBook = true;
        else if (s == "--weak") a.weak = true;
        else if (s == "--book") a.book = val();
        else if (s == "--threads") a.threads = std::atoi(val().c_str());
        else if (s == "--out") a.out = val();
        else if (s == "--root") a.root = val();
        else if (s == "--sample") a.sample = std::atoi(val().c_str());
        else if (s == "--min-ply") a.minPly = std::atoi(val().c_str());
        else if (s == "--limit") a.limit = std::atol(val().c_str());
        else if (s == "--tt-bits") a.ttBits = std::atoi(val().c_str());
        else a.pos.push_back(s);
    }
    return a;
}

// Load the opening book: explicit --book, else ./book.bin, else book.bin next to the executable.
static bool loadBook(const Args& a, const char* argv0, Book& b) {
    if (a.noBook) return false;
    std::vector<std::string> cand;
    if (!a.book.empty()) cand.push_back(a.book);
    else {
        cand.push_back("book.bin");
        std::string p = argv0;
        size_t k = p.find_last_of("/\\");
        if (k != std::string::npos) cand.push_back(p.substr(0, k + 1) + "book.bin");
    }
    for (auto& c : cand)
        if (b.load(c)) {
            std::cerr << "[book] " << c << ": " << b.size() << " positions, up to ply " << b.depth << "\n";
            return true;
        }
    if (!a.book.empty()) std::cerr << "[book] could not load " << a.book << "\n";
    return false;
}

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

static void playGame(bool humanFirst, const Book* book) {
    Solver S;
    S.setBook(book);
    Position P;
    char g[W][H];
    for (auto& col : g) for (char& ch : col) ch = '.';
    int height[W] = {0};
    bool humanTurn = humanFirst;
    char humanSym = humanFirst ? 'X' : 'O', aiSym = humanFirst ? 'O' : 'X';

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
            // With an opening book the AI always uses exact scores: inside the book they are
            // lookups, just past its end they are a (slower) exact search. Without a book, exact
            // search is only practical from ply EXACT_FROM on.
            bool haveBook = book && book->coversOpening(0);
            bool exact = haveBook || P.moves >= EXACT_FROM;
            if (haveBook && !book->coversOpening(P.moves + 1))
                std::cout << "(past the end of the book: exact search, this can take a while)\n" << std::flush;
            if (!exact && P.moves == 0) {
                col = W / 2;
            } else {
                if (!exact)
                    std::cout << "(no opening book: heuristic search until move " << EXACT_FROM << ")\n";
                auto sc = S.analyze(P, exact);
                col = int(std::max_element(sc.begin(), sc.end()) - sc.begin());
                std::cout << "Column scores" << (exact ? "" : " (heuristic)") << ":";
                for (int c = 0; c < W; c++)
                    std::cout << " " << (sc[c] == -1000 ? std::string("-") : std::to_string(sc[c]));
                std::cout << "\n";
            }
            std::cout << "AI plays " << col + 1 << "\n";
        }
        bool win = P.isWinningMove(col);
        g[col][height[col]++] = humanTurn ? humanSym : aiSym;
        P.play(col);
        if (win) {
            printBoard(g);
            std::cout << (humanTurn ? "You win!\n" : "AI wins!\n");
            return;
        }
        humanTurn = !humanTurn;
    }
}

// Two checks: (1) internal consistency of every entry against its children (catches derivation
// bugs), (2) a random sample compared against a fresh full search with NO book.
static int verifyBook(const Book& b, int sample, int minPly) {
    int bad = 0;
    size_t checked = 0;
    for (u64 e : b.entries()) {
        Position P = Position::fromKey(Book::keyOf(e));
        if (P.canonicalKey() != Book::keyOf(e)) { std::cout << "key roundtrip failed\n"; bad++; continue; }
        if (P.moves >= (int)b.depth && !P.canWinNext()) continue;
        int expect;
        if (P.canWinNext()) expect = (W * H + 1 - P.moves) / 2;
        else {
            expect = -100;
            for (int c = 0; c < W; c++) {
                if (!P.canPlay(c)) continue;
                Position Q = P;
                Q.play(c);
                int s;
                if (!b.lookup(Q, s)) { std::cout << "missing child of ply-" << P.moves << " entry\n"; bad++; expect = 1000; break; }
                expect = std::max(expect, -s);
            }
        }
        checked++;
        if (expect != Book::scoreOf(e)) { bad++; std::cout << "INCONSISTENT entry at ply " << P.moves << "\n"; }
    }
    std::cout << "consistency: " << checked << " entries checked, " << bad << " problems\n";

    if (minPly < 0) minPly = std::max(0, (int)b.depth - 1);
    std::vector<u64> pool;
    for (u64 e : b.entries())
        if (Position::fromKey(Book::keyOf(e)).moves >= minPly) pool.push_back(e);
    std::mt19937_64 rng(12345);
    std::shuffle(pool.begin(), pool.end(), rng);
    if ((int)pool.size() > sample) pool.resize(sample);
    Solver S(22);  // no book attached
    int bad2 = 0;
    for (u64 e : pool) {
        Position P = Position::fromKey(Book::keyOf(e));
        int got = S.solve(P);
        if (got != Book::scoreOf(e)) {
            bad2++;
            std::cout << "MISMATCH key=" << Book::keyOf(e) << " ply=" << P.moves << " book=" << Book::scoreOf(e)
                      << " search=" << got << "\n";
        }
    }
    std::cout << "search cross-check: " << pool.size() << " sampled entries (ply >= " << minPly << "), "
              << bad2 << " mismatches\n";
    return bad + bad2;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cout << USAGE; return 1; }
    std::string mode = argv[1];
    Args a = parse(argc, argv);

    if (mode == "genbook") {
        if (a.pos.empty()) { std::cout << USAGE; return 1; }
        Position root;
        if (!root.playSequence(a.root)) { std::cerr << "invalid --root\n"; return 1; }
        int threads = a.threads > 0 ? a.threads : (int)std::max(1u, std::thread::hardware_concurrency());
        return generateBook(root, std::atoi(a.pos[0].c_str()), threads, a.ttBits, a.out, (size_t)a.limit);
    }

    Book book;
    bool haveBook = loadBook(a, argv[0], book);

    if (mode == "verifybook") {
        if (!haveBook) { std::cerr << "no book loaded (use --book FILE)\n"; return 1; }
        return verifyBook(book, a.sample, a.minPly) ? 1 : 0;
    }
    if (mode == "play") {
        bool humanFirst = !(a.pos.size() > 0 && a.pos[0] == "ai");
        playGame(humanFirst, haveBook ? &book : nullptr);
        return 0;
    }
    if (mode == "solve") {
        Solver S;
        if (haveBook) S.setBook(&book);
        if (!a.pos.empty()) { solveOne(S, a.pos[0], a.weak); return 0; }
        std::string line;
        while (std::getline(std::cin, line))
            if (!line.empty()) solveOne(S, line, a.weak);
        return 0;
    }
    std::cout << USAGE;
    return 1;
}
