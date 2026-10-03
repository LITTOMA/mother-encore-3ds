#!/usr/bin/env python3
"""Generate original geometric branding and silence, without fonts or extra packages."""
import struct
import wave
import zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def png(path,w,h):
    def chunk(t,d):return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d))
    data=bytearray()
    for y in range(h):
        data.append(0)
        for x in range(w):
            u,v=x/w,y/h
            color=(13,24,35)
            if (x//8+y//8)%2:color=(18,33,44)
            # A stylized original tile/door emblem; no upstream or official artwork.
            if .3<u<.7 and .12<v<.88:color=(95,214,182)
            if .38<u<.65 and .22<v<.88:color=(13,24,35)
            if .53<u<.59 and .50<v<.58:color=(234,199,104)
            data.extend(color)
    blob=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(data,9))+chunk(b'IEND',b'')
    path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(blob)
def main():
    png(ROOT/'assets/icon.png',48,48);png(ROOT/'assets/banner.png',256,128)
    with wave.open(str(ROOT/'assets/silence.wav'),'wb') as out:
        out.setnchannels(2);out.setsampwidth(2);out.setframerate(22050);out.writeframes(b'\0'*22050*4)
    print('Generated original icon, banner and silent banner audio.')
if __name__=='__main__':main()
