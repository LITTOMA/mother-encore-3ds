#!/usr/bin/env python3
"""Compare actual Baby output with the frozen literal scalar kernel; host only."""
from __future__ import annotations
import argparse
import ast
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    p.add_argument("--sanitize", action="store_true")
    p.add_argument("--prototype", action="store_true")
    p.add_argument("--inline-prototype", action="store_true")
    p.add_argument("--combined-prototype", action="store_true")
    p.add_argument("--temporal-prototype", action="store_true")
    p.add_argument("--step", type=float, default=0.071)
    p.add_argument("--samples", type=int, default=256)
    p.add_argument("--iterations", type=int, default=80)
    a = p.parse_args()
    build = ROOT / "build/baby-background-performance"
    build.mkdir(parents=True, exist_ok=True)
    tree = ast.parse((ROOT / "tests/test_battle_assets.py").read_text())
    function = next(n for n in ast.walk(tree) if isinstance(n, ast.FunctionDef) and n.name == "test_actual_renderer_expansion_repeat_and_reference_sampling")
    stub = next(ast.literal_eval(n.value) for n in function.body if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "stub" for t in n.targets))
    (build / "3ds.h").write_text(stub)
    (build / "citro2d.h").write_text('#pragma once\n#include "3ds.h"\n')
    binary = build / ("benchmark-sanitize" if a.sanitize else "benchmark")
    flags = ["-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", "-ffp-contract=off"]
    if a.temporal_prototype:
        flags += ["-DENCORE_TEMPORAL_PROTOTYPE"]
    elif a.combined_prototype:
        flags += ["-DENCORE_COMBINED_PROTOTYPE"]
    elif a.prototype:
        flags += ["-DENCORE_BLOCK_PROTOTYPE"]
    elif a.inline_prototype:
        flags += ["-DENCORE_INLINE_PROTOTYPE"]
    if a.sanitize:
        flags += ["-g", "-fsanitize=address,undefined,float-cast-overflow", "-fno-omit-frame-pointer", "-fno-pie", "-no-pie"]
    subprocess.run([a.compiler, *flags, "-I" + str(build), "-I" + str(ROOT), "-I" + str(ROOT / "include"), str(ROOT / "tests/baby_background_performance_tests.cpp"), str(ROOT / "runtime/battle_data.cpp"), str(ROOT / "runtime/file_io.cpp"), "-o", str(binary)], check=True)
    env = dict(os.environ)
    if a.sanitize:
        env["ASAN_OPTIONS"] = "detect_leaks=0"
    for w, h in [(400, 240), (320, 180)]:
        subprocess.run([str(binary), str(ROOT / "romfs/data/doll-entry.encbattle"), str(ROOT / "romfs"), str(w), str(h), str(a.samples), str(a.iterations), str(a.step)], check=True, env=env)


if __name__ == "__main__":
    main()
