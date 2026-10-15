// bookgen.hpp - opening-book generator.
//
// Algorithm
//   1. Enumerate every reachable, unfinished position from the root, ply by ply, down to `depth`.
//      Mirror images are merged (canonical key), which roughly halves every layer.
//      Positions where the side to move can win immediately are not expanded: their score is known
//      without looking at any child.
//   2. Solve only the DEEPEST layer with the full alpha-beta solver. This is the expensive part;
//      it runs on several threads, each with its own transposition table that persists across
//      positions, and every result is appended to "<out>.part" so an interrupted run resumes.
//   3. Derive every shallower layer by plain minimax over the layer below:
//          score(P) = max over moves m of -score(child(P, m))
//      No search is needed for those layers at all.
#pragma once
#include <atomic>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "book.hpp"
#include "solver.hpp"

// Returns 0 on success, 1 on error, 2 if stopped early because of `limit` (progress is saved).
inline int generateBook(const Position& root, int depth, int threads, unsigned ttBits,
                        const std::string& outPath, size_t limit = 0) {
    using clk = std::chrono::steady_clock;
    auto t0 = clk::now();
    auto elapsed = [&] { return std::chrono::duration<double>(clk::now() - t0).count(); };

    if (depth < root.moves || depth > W * H) {
        std::cerr << "depth must be between " << root.moves << " and " << W * H << "\n";
        return 1;
    }
    if (threads < 1) threads = 1;

    // ---- 1. enumerate layers
    std::vector<std::vector<Position>> layer(depth - root.moves + 1);
    layer[0] = {root};
    auto byKey = [](const Position& a, const Position& b) { return a.canonicalKey() < b.canonicalKey(); };
    auto sameKey = [](const Position& a, const Position& b) { return a.canonicalKey() == b.canonicalKey(); };
    for (size_t i = 0; i + 1 < layer.size(); i++) {
        auto& next = layer[i + 1];
        for (const Position& P : layer[i]) {
            if (P.canWinNext()) continue;
            for (int c = 0; c < W; c++) {
                if (!P.canPlay(c)) continue;
                Position Q = P;
                Q.play(c);
                next.push_back(Q);
            }
        }
        std::sort(next.begin(), next.end(), byKey);
        next.erase(std::unique(next.begin(), next.end(), sameKey), next.end());
        if (next.size() > 30000000) {
            std::cerr << "layer too large (" << next.size() << " positions); choose a smaller depth\n";
            return 1;
        }
    }
    size_t total = 0;
    for (size_t i = 0; i < layer.size(); i++) {
        std::cout << "ply " << root.moves + i << ": " << layer[i].size() << " positions\n";
        total += layer[i].size();
    }
    std::cout << "total " << total << " positions; deepest layer to solve: " << layer.back().size()
              << "\n" << std::flush;

    // ---- 2. solve the deepest layer (parallel, resumable)
    const std::string partPath = outPath + ".part";
    std::unordered_map<u64, int> top;  // canonical key -> score
    {
        FILE* f = std::fopen(partPath.c_str(), "rb");
        if (f) {
            u64 e;
            while (std::fread(&e, 8, 1, f) == 1) top[Book::keyOf(e)] = Book::scoreOf(e);
            std::fclose(f);
        }
    }
    std::vector<Position> todo;
    for (const Position& P : layer.back())
        if (!top.count(P.canonicalKey())) todo.push_back(P);
    size_t remaining = 0;
    if (limit && todo.size() > limit) { remaining = todo.size() - limit; todo.resize(limit); }
    std::cout << "already solved (resumed): " << layer.back().size() - todo.size() << ", to solve: "
              << todo.size() << " on " << threads << " thread(s), TT " << ((8ULL << ttBits) >> 20)
              << " MB each\n" << std::flush;

    if (!todo.empty()) {
        FILE* part = std::fopen(partPath.c_str(), "ab");
        if (!part) { std::cerr << "cannot write " << partPath << "\n"; return 1; }
        std::atomic<size_t> next{0}, finished{0};
        std::mutex mu;
        auto worker = [&] {
            Solver S(ttBits);  // TT is kept across positions: they share most of their subtrees
            std::vector<u64> buf;
            auto flush = [&] {
                std::lock_guard<std::mutex> lk(mu);
                for (u64 e : buf) top[Book::keyOf(e)] = Book::scoreOf(e);
                std::fwrite(buf.data(), 8, buf.size(), part);
                std::fflush(part);
                buf.clear();
            };
            for (;;) {
                size_t i = next++;
                if (i >= todo.size()) break;
                int s = S.solve(todo[i]);
                buf.push_back(Book::pack(todo[i].canonicalKey(), s));
                finished++;
                if (buf.size() >= 16) flush();
            }
            flush();
        };
        std::vector<std::thread> pool;
        for (int i = 0; i < threads; i++) pool.emplace_back(worker);
        double lastPrint = 0;
        while (finished < todo.size()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            double t = elapsed();
            if (t - lastPrint >= 30) {
                lastPrint = t;
                size_t d = finished;
                double eta = d ? t * (double)(todo.size() - d) / (double)d : 0;
                std::printf("[%7.0fs] solved %zu/%zu (%.1f%%), ETA ~%.0f min\n", t, d, todo.size(),
                            100.0 * d / todo.size(), eta / 60);
                std::fflush(stdout);
            }
        }
        for (auto& th : pool) th.join();
        std::fclose(part);
    }

    if (remaining) {
        std::cout << "stopped after --limit; " << remaining << " positions left. Re-run the same command to continue.\n";
        return 2;
    }

    // ---- 3. derive shallower layers by minimax
    std::vector<u64> all;
    std::unordered_map<u64, int> below = std::move(top);
    for (const Position& P : layer.back()) all.push_back(Book::pack(P.canonicalKey(), below[P.canonicalKey()]));
    for (int d = (int)layer.size() - 2; d >= 0; d--) {
        std::unordered_map<u64, int> cur;
        for (const Position& P : layer[d]) {
            int best;
            if (P.canWinNext()) {
                best = (W * H + 1 - P.moves) / 2;
            } else {
                best = -100;
                for (int c = 0; c < W; c++) {
                    if (!P.canPlay(c)) continue;
                    Position Q = P;
                    Q.play(c);
                    auto it = below.find(Q.canonicalKey());
                    if (it == below.end()) { std::cerr << "internal error: child missing\n"; return 1; }
                    best = std::max(best, -it->second);
                }
            }
            cur[P.canonicalKey()] = best;
            all.push_back(Book::pack(P.canonicalKey(), best));
        }
        below = std::move(cur);
    }
    int rootScore = below[root.canonicalKey()];

    Book book;
    book.set(std::move(all), (uint32_t)depth, (uint32_t)root.moves);
    if (!book.save(outPath)) { std::cerr << "cannot write " << outPath << "\n"; return 1; }
    std::remove(partPath.c_str());
    std::cout << "root position score: " << rootScore << "\n"
              << "wrote " << outPath << ": " << book.size() << " entries, "
              << (book.size() * 8 + 20) / 1024 << " KB, in " << elapsed() << " s\n";
    return 0;
}
