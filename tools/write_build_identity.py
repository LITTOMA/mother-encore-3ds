#!/usr/bin/env python3
"""Write actual checkout identity to the console build directory."""
from pathlib import Path
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/ctr/build_identity.hpp'


def main():
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    if not re.fullmatch('[0-9a-f]{40}', head):
        raise ValueError('Console build has no actual Git commit identity')
    status = subprocess.check_output(
        ['git', 'status', '--porcelain=v1', '--untracked-files=normal'], cwd=ROOT)
    revision = head[:12] + ('-modified' if status else '')
    payload = ('#pragma once\nnamespace encore::ctr {\n'
               'inline constexpr char build_revision[] = "' + revision + '";\n}\n').encode()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    if OUT.exists() and OUT.read_bytes() == payload:
        return
    temporary = OUT.with_suffix('.tmp')
    temporary.write_bytes(payload)
    os.replace(temporary, OUT)


if __name__ == '__main__':
    main()
