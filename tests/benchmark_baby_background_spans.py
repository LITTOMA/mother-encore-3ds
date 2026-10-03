#!/usr/bin/env python3
"""Isolated exact fallback-span experiment. Never changes the production header."""
from __future__ import annotations
import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PRODUCTION_SHA = "47a3bcd332e3c22c9e0f23f0cad3030fe866615c76574ab8f45cf058e4e8902e"
PROMOTED_SHA = "e722c99c7a475444faf71a1d5165bb1c0f9443995dd9f6426a2a399d4b6054c5"


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--samples", type=int, default=64)
    p.add_argument("--iterations", type=int, default=120)
    p.add_argument("--sanitize", action="store_true")
    p.add_argument("--compiler", default="c++")
    p.add_argument("--fixture", action="store_true")
    a = p.parse_args()
    assert hashlib.sha256((ROOT / "include/encore/background_kernel.hpp").read_bytes()).hexdigest() in {PRODUCTION_SHA, PROMOTED_SHA}
    assert hashlib.sha256((ROOT / "tests/baby_background_combined_prototype.hpp").read_bytes()).hexdigest() == PRODUCTION_SHA
    build = ROOT / "build/baby-background-span-experiment"
    build.mkdir(parents=True, exist_ok=True)
    tree = ast.parse((ROOT / "tests/test_battle_assets.py").read_text())
    function = next(n for n in ast.walk(tree) if isinstance(n, ast.FunctionDef) and n.name == "test_actual_renderer_expansion_repeat_and_reference_sampling")
    stub = next(ast.literal_eval(n.value) for n in function.body if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "stub" for t in n.targets))
    (build / "3ds.h").write_text(stub)
    (build / "citro2d.h").write_text('#pragma once\n#include "3ds.h"\n')
    # Reuse the existing broad fixture and real-content oracle. Only substitute
    # the candidate include and the benchmark baseline, in build-local copies.
    fixture = (ROOT / "tests/baby_background_kernel_tests.cpp").read_text()
    fixture = fixture.replace('#include "tests/baby_background_guarded_reference.hpp"', '#include "tests/baby_background_combined_prototype.hpp"')
    fixture = fixture.replace('#include "tests/baby_background_block_prototype.hpp"', '#include "tests/baby_background_span_prototype.hpp"')
    (build / "fixture.cpp").write_text(fixture)
    benchmark = (ROOT / "tests/baby_background_performance_tests.cpp").read_text()
    benchmark = benchmark.replace('#include "tests/baby_background_kernel_tests.cpp"', '#include "fixture.cpp"')
    benchmark = benchmark.replace('std::vector<uint32_t> expected(size),actual(size),mapped(texture_size);', '''require(guarded_baseline.prepare_mapped_output(assets.surface_offsets_.data(),size,texture_size,error),error.c_str());
    std::vector<uint32_t> expected(size),actual(size),mapped(texture_size);''')
    benchmark = benchmark.replace('    assets.free();return 0;', '''
    for(unsigned round=0;round<7;++round){
        double baseline_mapped=0,candidate_mapped=0;
        auto baseline_run=[&]{return timed(iterations,[&](unsigned i){require(guarded_baseline.compose_mapped(float(i)*step,0,mapped.data(),mapped.size()),"mapped baseline");checksum^=mapped[assets.surface_offsets_[i%size]];});};
        auto candidate_run=[&]{return timed(iterations,[&](unsigned i){require(optimized.compose_mapped(float(i)*step,0,mapped.data(),mapped.size()),"mapped candidate");checksum^=mapped[assets.surface_offsets_[i%size]];});};
        if(round&1){candidate_mapped=candidate_run();baseline_mapped=baseline_run();}
        else{baseline_mapped=baseline_run();candidate_mapped=candidate_run();}
        std::printf("{\\"paired_round\\":%u,\\"width\\":%u,\\"height\\":%u,\\"baseline_mapped_ms\\":%.6f,\\"candidate_mapped_ms\\":%.6f,\\"speedup\\":%.6f}\\n",round,w,h,baseline_mapped,candidate_mapped,baseline_mapped/candidate_mapped);
    }
    assets.free();return 0;''')
    (build / "benchmark.cpp").write_text(benchmark)
    flags = ["-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", "-ffp-contract=off", "-DENCORE_BLOCK_PROTOTYPE"]
    if a.sanitize:
        flags += ["-g", "-fsanitize=address,undefined,float-cast-overflow", "-fno-omit-frame-pointer", "-fno-pie", "-no-pie"]
    target = "fixture" if a.fixture else "benchmark"
    binary = build / (target + ("-sanitize" if a.sanitize else ""))
    command = [a.compiler, *flags, "-I"+str(build), "-I"+str(ROOT), "-I"+str(ROOT/"include"), str(build/(target+".cpp"))]
    if not a.fixture:
        command += [str(ROOT/"runtime/battle_data.cpp"), str(ROOT/"runtime/file_io.cpp")]
    command += ["-o",str(binary)]
    subprocess.run(command,check=True)
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
    if a.fixture:
        subprocess.run([str(binary),str(build)],check=True,env=env)
    else:
        for w,h in [(400,240),(320,180),(37,29),(401,241)]:
            subprocess.run([str(binary),str(ROOT/"romfs/data/doll-entry.encbattle"),str(ROOT/"romfs"),str(w),str(h),str(a.samples),str(a.iterations),"0.066666667"],check=True,env=env)
    manifest = {"scope":"Host only; exact scalar parity and frozen current-production timing baseline", "command":command,
                "sources":{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ["include/encore/background_kernel.hpp","tests/baby_background_span_prototype.hpp","tests/baby_background_kernel_tests.cpp","tests/baby_background_performance_tests.cpp"]}}
    (build/(target+"-manifest.json")).write_text(json.dumps(manifest,indent=2)+"\n")


if __name__ == "__main__":
    main()
