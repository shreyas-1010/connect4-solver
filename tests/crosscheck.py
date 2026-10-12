#!/usr/bin/env python3
"""Randomized cross-check: optimized solver vs. naive reference solver.

Usage (from repo root):  make crosscheck        or
                         python3 tests/crosscheck.py [num_positions] [stones_played]
"""
import random
import subprocess
import sys

n_target = int(sys.argv[1]) if len(sys.argv) > 1 else 25
stones = int(sys.argv[2]) if len(sys.argv) > 2 else 26
random.seed(5)

tested = bad = 0
while tested < n_target:
    heights, seq = [0] * 7, ""
    for _ in range(stones):
        c = random.choice([i for i in range(7) if heights[i] < 6])
        heights[c] += 1
        seq += str(c + 1)
    fast = subprocess.run(["./connect4", "solve", seq], capture_output=True, text=True).stdout
    if "invalid" in fast:  # game already over in this random sequence
        continue
    try:
        ref = subprocess.run(["./reference", seq], capture_output=True, text=True, timeout=120).stdout
    except subprocess.TimeoutExpired:
        continue
    a = int(fast.split("score=")[1].split()[0])
    b = int(ref.split()[1])
    tested += 1
    if a != b:
        bad += 1
        print(f"MISMATCH {seq}: solver={a} reference={b}")
print(f"tested {tested} positions, {bad} mismatches")
sys.exit(1 if bad else 0)
