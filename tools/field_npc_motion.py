"""Eight-direction NPC motion codec; no dialogue or release staging.

Capability 6 admits the source PartyMember direction order. Capability 5 was
withdrawn and remains unknown. Existing Mick / four-direction bytes stay exact.
"""
import struct,zlib
from tools import mick_treats as m
from tools.extract_battle_entry import require
from tools.podunk_field import pack_container


def encode(ir,owner,features):
    # The common lowerer supplies the existing nine section layouts. Repack
    # those sections into the independent capability before admitting them.
    rows=m.parse(m.lower(ir,motion_owner=owner,motion_flags=features),allow_motion=True)
    blob=bytearray(pack_container(m.MAGIC,owner,[rows[n]for n in m.SECTIONS],m.STRIDES,m.LIMIT,capability=6))
    struct.pack_into('<I',blob,28,features)
    struct.pack_into('<I',blob,56,8)
    struct.pack_into('<I',blob,16,zlib.crc32(blob[32:]))
    parse(blob)
    return bytes(blob)


def parse(blob):
    require(isinstance(blob,(bytes,bytearray))and 64<=len(blob)<=m.LIMIT,'NPC eight-direction resource size')
    require(struct.unpack_from('<I',blob,20)[0]==6 and struct.unpack_from('<2I',blob,56)==(8,0),'NPC eight-direction capability/policy')
    require(zlib.crc32(blob[32:])==struct.unpack_from('<I',blob,16)[0],'NPC eight-direction checksum')
    require(not(struct.unpack_from('<I',blob,28)[0]&1),'NPC eight-direction wandering is outside reviewed scope')
    common=bytearray(blob)
    struct.pack_into('<I',common,20,4);struct.pack_into('<I',common,56,0)
    struct.pack_into('<I',common,16,zlib.crc32(common[32:]))
    rows=m.parse(common,allow_motion=True)
    texture=struct.unpack('<I4H',rows['Texture']);frames=texture[3]*texture[4]
    actor=rows['Actor'];clips=[struct.unpack_from('<4H',rows['Clips'],i)for i in range(0,len(rows['Clips']),8)]
    spans=struct.unpack_from('<6I',actor,172)
    require(spans[0]==0 and spans[2]==spans[1] and spans[4]==spans[2]+spans[3] and spans[4]+spans[5]==len(clips),'NPC eight-direction clip ownership')
    for role in range(3):
        first,count=spans[role*2:role*2+2];selected=clips[first:first+count]
        require(count>0 and {c[1]for c in selected}==set(range(8)),'NPC eight-direction missing direction')
        require(all(c[0]==role and c[2]<frames and 0<c[3]<=10000 for c in selected),'NPC eight-direction clip')
        require([c[1]for c in selected]==sorted(c[1]for c in selected),'NPC eight-direction clip order')
    return rows
