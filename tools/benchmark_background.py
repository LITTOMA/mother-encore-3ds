#!/usr/bin/env python3
"""Compare linear and direct-tiled output with the frozen CPU reference/upload.

All production background parameters and image bytes come from external RomFS.
Numbers describe this host CPU only, never 3DS or GPU performance/equivalence.
"""
from __future__ import annotations
import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default=os.environ.get('CXX', 'c++'))
    parser.add_argument('--samples', type=int, default=1024)
    parser.add_argument('--iterations', type=int, default=200)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--report', type=Path)
    parser.add_argument('--arm-compiler', type=Path, help='Optional devkitARM compiler for a mapped/linear object compile; does not execute ARM code')
    parser.add_argument('--reference', type=Path, default=ROOT / 'reports/background-performance-cpu/reference-renderer.hpp')
    args = parser.parse_args()
    build = ROOT / 'build/background-kernel'
    build.mkdir(parents=True, exist_ok=True)
    # Reuse the existing renderer's no-op GPU fixture, rather than inventing a
    # second platform emulation. This does not intercept any compositor code.
    tree = ast.parse((ROOT / 'tests/test_battle_assets.py').read_text())
    function = next(n for n in ast.walk(tree) if isinstance(n, ast.FunctionDef) and n.name == 'test_actual_renderer_expansion_repeat_and_reference_sampling')
    stub = next(ast.literal_eval(n.value) for n in function.body if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'stub' for t in n.targets))
    (build / '3ds.h').write_text(stub)
    (build / 'citro2d.h').write_text('#pragma once\n#include "3ds.h"\n')
    binary = build / ('background-test-sanitize' if args.sanitize else 'background-test')
    flags = ['-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', '-ffp-contract=off']
    if args.sanitize:
        flags += ['-g', '-fsanitize=address,undefined,float-cast-overflow', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
    command = [args.compiler, *flags, '-I'+str(build), '-I'+str(ROOT), '-I'+str(ROOT / 'include'),
               '-DENCORE_BACKGROUND_REFERENCE_HEADER="'+str(args.reference.resolve())+'"',
               str(ROOT / 'tests/background_kernel_tests.cpp'), str(ROOT / 'runtime/battle_data.cpp'),
               str(ROOT / 'runtime/file_io.cpp'), '-o', str(binary)]
    subprocess.run(command, check=True)
    results = []
    env = dict(os.environ)
    if args.sanitize:
        env['ASAN_OPTIONS'] = 'detect_leaks=0'
    for width, height in [(400, 240), (320, 180)]:
        run = [str(binary), str(ROOT / 'romfs/data/opening.encbattle'), str(ROOT / 'romfs'),
               str(width), str(height), str(args.samples), str(args.iterations)]
        result = subprocess.run(run, check=True, capture_output=True, text=True, env=env)
        print(result.stdout, end='', flush=True)
        results.append(json.loads(result.stdout))
    record = dict(scope='host CPU linear-word and full padded-texture byte equality/benchmark; mocked GPU; not GPU or 3DS verification',
                  timestamp_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                  platform=platform.platform(), compiler=subprocess.check_output([args.compiler, '--version'], text=True).splitlines()[0],
                  build_command=shlex.join(command), sanitizer=args.sanitize, leak_sanitizer=False if args.sanitize else None,
                  sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [args.reference, ROOT / 'include/encore/background_kernel.hpp', ROOT / 'tests/background_kernel_tests.cpp', Path(__file__).resolve(), ROOT / 'romfs/data/opening.encbattle', ROOT / 'romfs/battle-preview/background.bpx']},
                  results=results)
    if args.arm_compiler:
        probe = build / 'background-mapped-arm.cpp'
        probe.write_text('#include "encore/background_kernel.hpp"\n'
                         'bool mapped(encore::BackgroundKernel& k,float t,uint32_t c,uint32_t* p,size_t n){return k.compose_mapped(t,c,p,n);}\n'
                         'bool linear(encore::BackgroundKernel& k,float t,uint32_t c,uint32_t* p){return k.compose(t,c,p);}\n')
        obj = probe.with_suffix('.o')
        arm_command = [str(args.arm_compiler), '-I'+str(ROOT / 'include'), '-march=armv6k', '-mtune=mpcore',
                       '-mfloat-abi=hard', '-mtp=soft', '-std=gnu++17', '-O2', '-g', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                       '-mword-relocations', '-ffunction-sections', '-fdata-sections', '-fno-exceptions', '-fno-rtti',
                       '-c', str(probe), '-o', str(obj)]
        subprocess.run(arm_command, check=True)
        arm_prefix = str(args.arm_compiler).removesuffix('g++')
        record['arm_compile'] = dict(scope='ARMv6k object cross-compile only; not ARM execution',
                                     compiler=subprocess.check_output([str(args.arm_compiler), '--version'], text=True).splitlines()[0],
                                     command=shlex.join(arm_command), probe_source=probe.read_text(),
                                     object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),
                                     size=subprocess.check_output([arm_prefix+'size', str(obj)], text=True))
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            disassembly = args.report.with_suffix('.arm-disassembly.txt')
            disassembly.write_text(subprocess.check_output([arm_prefix+'objdump', '-d', '-C', str(obj)], text=True))
            record['arm_compile']['disassembly'] = str(disassembly)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(record, indent=2)+'\n')


if __name__ == '__main__':
    main()
