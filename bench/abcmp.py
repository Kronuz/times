#!/usr/bin/env python3
"""Pair the runs of the times benchmark binaries: abcmp.py BUILD_DIR [ROUNDS].

Runs times_bench_old, times_bench_new and times_bench_old2 (a second copy of the
old code, for the A/A spread) in BUILD_DIR back to back ROUNDS times (default 5) and prints, per
row, the best ns/op of the old and new code, their ratio, and the A/A ratio old2/old, which is
the noise floor of the machine. Rows that exist only in the new binary show "-" for the old.

  cmake -B build -DCMAKE_BUILD_TYPE=Release -DTIMES_BUILD_BENCH=ON && cmake --build build
  python3 bench/abcmp.py build 5

Time it on an otherwise idle machine, under a hold (see the shared PERFORMANCE.md).
"""
import os
import subprocess
import sys

PREFIX = "times_bench_"
out = sys.argv[1]
rounds = int(sys.argv[2]) if len(sys.argv) > 2 else 5
variants = ["old", "new", "old2"]
for v in variants:
    if not os.access(f"{out}/{PREFIX}{v}", os.X_OK):
        sys.exit(f"missing {out}/{PREFIX}{v}: configure with the bench option and build")
best = {}
order = []
for _ in range(rounds):
    for variant in variants:
        text = subprocess.run([f"{out}/{PREFIX}{variant}"], capture_output=True, text=True, check=True).stdout
        for line in text.splitlines():
            name, _, value = line.rpartition(" ")
            name = name.strip()
            if name not in order:
                order.append(name)
            key = (name, variant)
            best[key] = min(best.get(key, 1e30), float(value))

print(f"{'row':46} {'old':>8} {'new':>8} {'new/old':>8} {'A/A':>6}")
for name in order:
    old, new, old2 = (best.get((name, v)) for v in variants)
    ratio = f"{new / old:8.3f}" if old and new else f"{'-':>8}"
    aa = f"{old2 / old:6.3f}" if old and old2 else f"{'-':>6}"
    print(f"{name:46} " + (f"{old:8.3f}" if old else f"{'-':>8}") + " " + (f"{new:8.3f}" if new else f"{'-':>8}") + f" {ratio} {aa}")
