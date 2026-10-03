#!/usr/bin/env python3
"""Build, but do not run, official-SDK parity for all staged native T3X files."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", default="", help="Optional distinct binary/log suffix")
    args = parser.parse_args()
    if args.tag and not all(c.isalnum() or c == '-' for c in args.tag):
        raise SystemExit("Tag must contain only letters, numbers or hyphens")
    suffix = "-" + args.tag if args.tag else ""
    sdk = Path(os.environ["DEVKITPRO"])
    arm = Path(os.environ.get("DEVKITARM", sdk / "devkitARM"))
    staged = ROOT / "build/ctr/native-romfs"
    textures = sorted(staged.rglob("*.t3x"))
    if not textures:
        raise SystemExit("Run tools/stage_native_romfs.py first; no staged textures found")
    build = ROOT / "build/loading-texture-selfcheck"
    romfs = build / "romfs"
    romfs.mkdir(parents=True, exist_ok=True)
    # Own a separate deterministic fixture directory; never change game RomFS.
    expected = {str(p.relative_to(staged)) for p in textures}
    for old in romfs.rglob("*.t3x"):
        if str(old.relative_to(romfs)) not in expected and old.name != "truncated.t3x":
            old.unlink()
    paths = []
    for source in textures:
        relative = source.relative_to(staged)
        target = romfs / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        paths.append("romfs:/" + relative.as_posix())
    (romfs / "texture-paths.txt").write_text("\n".join(paths) + "\n")
    largest = max(textures, key=lambda p: p.stat().st_size)
    with largest.open("rb") as stream:
        (romfs / "truncated.t3x").write_bytes(stream.read(128))
    source = ROOT / "tests/loading_texture_arm_selfcheck.cpp"
    header = ROOT / "platform/ctr/loading_texture.hpp"
    name = "loading-texture-selfcheck" + suffix
    elf = build / (name + ".elf")
    smdh = build / (name + ".smdh")
    binary = build / (name + ".3dsx")
    pending = build / (name + ".pending.3dsx")
    sd_log = "sdmc:/encore-" + name + ".log"
    commands = [[
        str(arm / "bin/arm-none-eabi-g++"), "-std=gnu++17", "-O2", "-g", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
        "-march=armv6k", "-mtune=mpcore", "-mfloat-abi=hard", "-mtp=soft", "-D__3DS__",
        "-fno-exceptions", "-fno-rtti", "-ffunction-sections", "-fdata-sections",
        '-DSELF_CHECK_LOG_PATH="' + sd_log + '"',
        "-I" + str(ROOT), "-I" + str(ROOT / "include"), "-isystem", str(sdk / "libctru/include"),
        "-specs=3dsx.specs", "-Wl,--gc-sections", str(source), "-L" + str(sdk / "libctru/lib"),
        "-lcitro2d", "-lcitro3d", "-lctru", "-lm", "-o", str(elf),
    ], [
        str(sdk / "tools/bin/smdhtool"), "--create", "Texture Load SDK Parity", "Compare progressive and Citro2D", "Encore Native tests",
        str(ROOT / "assets/icon.png"), str(smdh),
    ], [str(sdk / "tools/bin/3dsxtool"), str(elf), str(pending), "--smdh=" + str(smdh), "--romfs=" + str(romfs)]]
    with (build / ("build" + suffix + ".log")).open("w") as log:
        for command in commands:
            log.write(shlex.join(command) + "\n"); log.flush()
            subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT)
    pending.replace(binary)
    report = {
        "scope": "ARM compile and package only; runtime parity has not been run",
        "binary": str(binary), "sha256": digest(binary), "textures": len(textures),
        "sources": {str(p.relative_to(ROOT)): digest(p) for p in [source, header, Path(__file__).resolve()]},
        "texture_sha256": {str(p.relative_to(staged)): digest(p) for p in textures},
        "sd_log": sd_log,
        "commands": [shlex.join(c) for c in commands],
    }
    (build / ("build" + suffix + ".json")).write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: report[key] for key in ["binary", "sha256", "textures", "scope"]}))


if __name__ == "__main__":
    main()
