#!/usr/bin/env python3
"""Build the Baby shader subset CPU/renderer test; this does not run an emulator."""
from __future__ import annotations
import argparse
import ast
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--prototype", action="store_true")
    args = parser.parse_args()
    build = ROOT / "build/baby-background-kernel"
    build.mkdir(parents=True, exist_ok=True)
    # Use the established renderer GPU stubs. All image checks, shader math,
    # preparation, error paths and palette mapping remain real production code.
    tree = ast.parse((ROOT / "tests/test_battle_assets.py").read_text())
    function = next(n for n in ast.walk(tree) if isinstance(n, ast.FunctionDef) and n.name == "test_actual_renderer_expansion_repeat_and_reference_sampling")
    stub = next(ast.literal_eval(n.value) for n in function.body if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "stub" for t in n.targets))
    (build / "3ds.h").write_text(stub)
    (build / "citro2d.h").write_text('#pragma once\n#include "3ds.h"\n')
    binary = build / ("baby-kernel-sanitize" if args.sanitize else "baby-kernel")
    flags = ["-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", "-ffp-contract=off"]
    if args.prototype:
        flags += ["-DENCORE_BLOCK_PROTOTYPE"]
    if args.sanitize:
        flags += ["-g", "-fsanitize=address,undefined,float-cast-overflow", "-fno-omit-frame-pointer", "-fno-pie", "-no-pie"]
    subprocess.run([args.compiler, *flags, "-I" + str(build), "-I" + str(ROOT), "-I" + str(ROOT / "include"), str(ROOT / "tests/baby_background_kernel_tests.cpp"), "-o", str(binary)], check=True)
    env = dict(os.environ)
    if args.sanitize:
        env["ASAN_OPTIONS"] = "detect_leaks=0"
    subprocess.run([str(binary), str(build)], check=True, env=env)


if __name__ == "__main__":
    main()
