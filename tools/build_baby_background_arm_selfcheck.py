#!/usr/bin/env python3
"""Build the separate real-ARM Baby pixel selfcheck with its checked RomFS."""
from __future__ import annotations
import hashlib
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", choices=["production", "guarded", "blocks", "inline", "combined"], default="production")
    args = parser.parse_args()
    sdk = Path(os.environ["DEVKITPRO"])
    arm = Path(os.environ.get("DEVKITARM", str(sdk / "devkitARM")))
    header = ROOT / "include/encore/background_kernel.hpp"
    guarded_reference = ROOT / "tests/baby_background_guarded_reference.hpp"
    oracle = ROOT / "tests/baby_background_scalar_reference.hpp"
    if digest(header) not in {"b41cc9696f8c94076bb3decf143f68fd7994c86808d917cd1204882afba7adb1", "47a3bcd332e3c22c9e0f23f0cad3030fe866615c76574ab8f45cf058e4e8902e", "e722c99c7a475444faf71a1d5165bb1c0f9443995dd9f6426a2a399d4b6054c5"}:
        raise SystemExit("Selfcheck production header differs from reviewed revisions")
    if digest(guarded_reference) != "b41cc9696f8c94076bb3decf143f68fd7994c86808d917cd1204882afba7adb1":
        raise SystemExit("Frozen guarded baseline changed")
    if digest(oracle) != "883d150df13a8bfefda5009a078fdc07051e2ec195ab8b86a01424e03470bbef":
        raise SystemExit("Frozen scalar oracle changed")
    build = ROOT / "build/baby-background-arm-selfcheck"
    candidate = None
    if args.candidate != "production":
        build /= args.candidate
        candidate = guarded_reference if args.candidate == "guarded" else ROOT / ("tests/baby_background_block_prototype.hpp" if args.candidate == "blocks" else "tests/baby_background_combined_prototype.hpp" if args.candidate == "combined" else "tests/baby_background_inline_prototype.hpp")
    staged = build / "romfs"
    build.mkdir(parents=True, exist_ok=True)
    paths = ["data/doll-entry.encbattle", "graphics/battle/doll/doll-background.bpx", "graphics/battle/doll/doll-palette.bpx"]
    for path in paths:
        target = staged / path
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / "romfs" / path, target)
    executable = build / "baby-background-selfcheck.elf"
    output = build / "baby-background-selfcheck.3dsx"
    temporary_output = build / "baby-background-selfcheck.pending.3dsx"
    smdh = build / "baby-background-selfcheck.smdh"
    compiler = arm / "bin/arm-none-eabi-g++"
    command = [str(compiler), "-std=gnu++17", "-O2", "-g", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-ffp-contract=off",
               "-march=armv6k", "-mtune=mpcore", "-mfloat-abi=hard", "-mtp=soft", "-D__3DS__", "-fno-exceptions", "-fno-rtti",
               "-ffunction-sections", "-fdata-sections", "-I" + str(ROOT), "-I" + str(ROOT / "include"),
               "-isystem", str(sdk / "libctru/include"), "-specs=3dsx.specs", "-Wl,--gc-sections",
               str(ROOT / "tests/baby_background_arm_selfcheck.cpp"), str(ROOT / "runtime/battle_data.cpp"), str(ROOT / "runtime/file_io.cpp"),
               "-L" + str(sdk / "libctru/lib"), "-lctru", "-lm", "-o", str(executable)]
    if candidate:
        command.insert(1, "-DENCORE_GUARDED_REFERENCE" if args.candidate == "guarded" else "-DENCORE_BLOCK_PROTOTYPE" if args.candidate == "blocks" else "-DENCORE_COMBINED_PROTOTYPE" if args.candidate == "combined" else "-DENCORE_INLINE_PROTOTYPE")
    else:
        command.insert(1, '-DSELF_CHECK_KERNEL_TAG="' + digest(header)[:8] + '"')
    icon = [str(sdk / "tools/bin/smdhtool"), "--create", "Baby Kernel ARM Selfcheck", "Scalar and guarded CPU parity", "Encore Native tests", str(ROOT / "assets/icon.png"), str(smdh)]
    package = [str(sdk / "tools/bin/3dsxtool"), str(executable), str(temporary_output), "--smdh=" + str(smdh), "--romfs=" + str(staged)]
    commands = [command, icon, package]
    with (build / "build.log").open("w") as log:
        for cmd in commands:
            log.write(shlex.join(cmd) + "\n");log.flush()
            subprocess.run(cmd, check=True, stdout=log, stderr=subprocess.STDOUT)
    temporary_output.replace(output)
    report = dict(scope="Real ARM compile/package only; parent must launch and inspect separate lower-screen selfcheck",
                  candidate=args.candidate,
                  commands=[shlex.join(c) for c in commands],
                  source_sha256={str(p.relative_to(ROOT)): digest(p) for p in [header, guarded_reference, oracle, ROOT / "tests/baby_background_arm_selfcheck.cpp", Path(__file__).resolve()]},
                  romfs_sha256={p: digest(staged / p) for p in paths},
                  elf_sha256=digest(executable), binary_sha256=digest(output), binary=str(output),
                  expected_success=dict(frames=8, sizes=[[400, 240], [320, 180]], times=[0, 1.25, 60, 1000], pixels=614400, mapped_bytes=4194304),
                  sd_log="sdmc:/encore-baby-background-selfcheck.log")
    if candidate:
        report["source_sha256"][str(candidate.relative_to(ROOT))] = digest(candidate)
    (build / "build.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"binary": str(output), "sha256": report["binary_sha256"], "manifest": str(build / "build.json")}))


if __name__ == "__main__":
    main()
