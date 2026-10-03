#!/usr/bin/env python3
"""Embed a compiled PICA program, never game content, in the executable."""
from pathlib import Path
import sys
source,output=map(Path,sys.argv[1:]);data=source.read_bytes()
assert data[:4]==b'DVLB' and len(data)%4==0
lines=['#pragma once','#include <cstdint>','alignas(4) static const uint8_t encore_gpu_region_shader[] = {']
for i in range(0,len(data),24):lines.append(','.join(str(b) for b in data[i:i+24])+',')
lines+=['};',''];output.write_text('\n'.join(lines))
