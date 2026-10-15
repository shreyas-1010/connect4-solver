// book.hpp - opening book: exact scores for all positions up to a given ply.
//
// Storage: a sorted array of 64-bit words, (canonical_key << 8) | (score + 64).
// Mirror-image positions share an entry, so each position is stored once.
//
// File format (little-endian):
//   "C4BK" | u32 version=1 | u32 depth | u32 rootMoves | u64 count | count x u64 entries
#pragma once
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "position.hpp"

class Book {
    std::vector<u64> t;  // sorted

public:
    uint32_t depth = 0;      // last ply (stones on board) covered
    uint32_t rootMoves = 0;  // 0 => covers the full game tree from the empty board

    static u64 pack(u64 key, int score) { return (key << 8) | u64(score + 64); }
    static u64 keyOf(u64 e) { return e >> 8; }
    static int scoreOf(u64 e) { return int(e & 255) - 64; }

    bool empty() const { return t.empty(); }
    size_t size() const { return t.size(); }
    const std::vector<u64>& entries() const { return t; }
    // True if every position up to `ply` from the empty board is in the book.
    bool coversOpening(int ply) const { return !t.empty() && rootMoves == 0 && ply <= (int)depth; }

    void set(std::vector<u64> e, uint32_t d, uint32_t root) {
        std::sort(e.begin(), e.end());
        e.erase(std::unique(e.begin(), e.end()), e.end());
        t = std::move(e);
        depth = d;
        rootMoves = root;
    }

    bool lookup(const Position& P, int& score) const {
        if (t.empty()) return false;
        u64 key = P.canonicalKey();
        auto it = std::lower_bound(t.begin(), t.end(), key << 8);
        if (it == t.end() || keyOf(*it) != key) return false;
        score = scoreOf(*it);
        return true;
    }

    bool save(const std::string& path) const {
        std::string tmp = path + ".tmp";
        FILE* f = std::fopen(tmp.c_str(), "wb");
        if (!f) return false;
        uint32_t ver = 1;
        uint64_t n = t.size();
        bool ok = std::fwrite("C4BK", 1, 4, f) == 4 && std::fwrite(&ver, 4, 1, f) == 1 &&
                  std::fwrite(&depth, 4, 1, f) == 1 && std::fwrite(&rootMoves, 4, 1, f) == 1 &&
                  std::fwrite(&n, 8, 1, f) == 1 && std::fwrite(t.data(), 8, t.size(), f) == t.size();
        ok = (std::fclose(f) == 0) && ok;
        return ok && std::rename(tmp.c_str(), path.c_str()) == 0;
    }

    bool load(const std::string& path) {
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) return false;
        char magic[4];
        uint32_t ver = 0, d = 0, r = 0;
        uint64_t n = 0;
        bool ok = std::fread(magic, 1, 4, f) == 4 && std::string(magic, 4) == "C4BK" &&
                  std::fread(&ver, 4, 1, f) == 1 && ver == 1 && std::fread(&d, 4, 1, f) == 1 &&
                  std::fread(&r, 4, 1, f) == 1 && std::fread(&n, 8, 1, f) == 1 && n < (1ULL << 32);
        std::vector<u64> e;
        if (ok) {
            e.resize(n);
            ok = std::fread(e.data(), 8, n, f) == n;
        }
        std::fclose(f);
        if (!ok || !std::is_sorted(e.begin(), e.end())) return false;
        t = std::move(e);
        depth = d;
        rootMoves = r;
        return true;
    }
};
