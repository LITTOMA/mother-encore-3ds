#!/usr/bin/env python3
"""Embed the compiled PICA execution mechanism, never game content."""
from pathlib import Path
import sys

source, output = map(Path, sys.argv[1:])
data = source.read_bytes()
if data[:4] != b'DVLB' or len(data) % 4:
    raise ValueError('Invalid compiled mapped-texture PICA program')
lines = ['#pragma once', '#include <cstdint>',
         'alignas(4) static const uint8_t encore_gpu_mapped_texture_shader[] = {']
for offset in range(0, len(data), 24):
    lines.append(','.join(str(byte) for byte in data[offset:offset + 24]) + ',')
lines.extend(['};', ''])
output.write_text('\n'.join(lines), encoding='utf-8')
