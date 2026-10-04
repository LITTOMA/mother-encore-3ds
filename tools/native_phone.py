#!/usr/bin/env python3
"""Compile the independently checked phone pack from reviewed staging content."""
from __future__ import annotations
import argparse
import json
import struct
import sys
import zlib
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools import phone_assets
from tools.doll_dialogue import PIN,require,sha

def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w', encoding='utf-8', newline='\n') as stream:
        stream.write(json.dumps(value, indent=2, sort_keys=True, ensure_ascii=False)+'\n')

SECTIONS=['Strings','Resources','Objects','Clips','FrameKeys','SoundKeys','FlagRefs','Dispatch']
FORMATS=[None,'<7I32s','<18I20f','<9Id','<dI','<dI','<2I','<2I']
STRIDES=[1]+[struct.calcsize(f)for f in FORMATS[1:]]
HEADER=64+16*len(SECTIONS)
OUT=ROOT/'romfs/data/opening.encphone'
NO_INDEX=0xffffffff


def lower(ir,root=ROOT):
    require(ir==phone_assets.build(root),'Unreviewed phone staging IR')
    phone_assets.verify(root,Path(root)/'romfs/phone-preview')
    bindings=phone_assets.presentation.load(root)
    clips_by_name={row['animation']['name']: row for row in bindings['clips']}
    clip_names={row['role']: row['animation']['name']for row in bindings['clips']}
    pool=bytearray(b'\0');strings={'':0}
    def string(value):
        require(isinstance(value,str)and '\0'not in value and len(value.encode())<=4096,'Phone invalid string')
        if value not in strings:
            strings[value]=len(pool);pool.extend(value.encode()+b'\0')
        return strings[value]
    table={name:[]for name in SECTIONS}
    resources={}
    for resource in ir['resources']:
        resources[resource['role']]=len(table['Resources'])
        texture=Path(root)/'romfs'/resource['path']
        table['Resources'].append([resource['id'],string(resource['path']),resource['kind'],
            resource['width'],resource['height'],resource['columns'],resource['rows'],bytes.fromhex(sha(texture))])
    flag_refs={}
    for index,obj in enumerate(ir['objects']):
        first_dispatch=len(table['Dispatch'])
        for row in obj['dispatch']['overrides']:
            if row['flag']not in flag_refs:
                flag_refs[row['flag']]=len(table['FlagRefs'])
                table['FlagRefs'].append([len(table['FlagRefs'])+1,string(row['flag'])])
            table['Dispatch'].append([flag_refs[row['flag']],string(row['dialogue'])])
        clip_indices={}
        for clip in obj['clips']:
            clip_indices[clip['name']]=len(table['Clips'])
            first_frame,first_sound=len(table['FrameKeys']),len(table['SoundKeys'])
            frame_events=[e for e in clip['events']if e['kind']=='Frame']
            sound_events=[e for e in clip['events']if e['kind']=='PlaySound']
            for event in frame_events:table['FrameKeys'].append([event['time'],event['frame']])
            for event in sound_events:table['SoundKeys'].append([event['time'],string(event['resource'])])
            table['Clips'].append([len(table['Clips'])+1,clips_by_name[clip['name']]['native_kind'],
                first_frame,len(frame_events),first_sound,len(sound_events),int(clip['loop']),
                frame_events[0]['track'],sound_events[0]['track']if sound_events else NO_INDEX,clip['length']])
        interaction,sprite,collider,audio=obj['interaction'],obj['sprite'],obj['collider'],obj['audio']
        policy=(1 if interaction['player_turn']['x']else 0)|(2 if interaction['player_turn']['y']else 0)
        policy|=(4 if sprite['centered']else 0)|(8 if audio['positional']else 0)|(16 if not obj['use']['is_payphone']else 0)
        vectors=[obj['position'],sprite['center'],interaction['center'],interaction['effective_extents'],
                 interaction['source_offset'],interaction['source_extents'],interaction['source_scale'],
                 collider['center'],collider['extents'],audio['center']]
        table['Objects'].append([index+1,string(obj['source_path']),resources[sprite['resource']],
            string(obj['dispatch']['default']),first_dispatch,len(obj['dispatch']['overrides']),
            clip_indices[clip_names['idle']],clip_indices[clip_names['ring']],string(audio['ring']),string(audio['hangup']),
            string(audio['bus']),string(obj['use']['save_location']),policy,
            collider['collision_layer'],collider['collision_mask'],interaction['collision_layer'],
            interaction['collision_mask'],sprite['initial_frame'],*[n for v in vectors for n in v]])
    table['Strings']=bytes(pool)
    return table


def encode(table,commit=PIN):
    require(set(table)==set(SECTIONS),'Unknown phone table')
    data=bytearray(HEADER)
    for index,name in enumerate(SECTIONS):
        block=table[name]if index==0 else b''.join(struct.pack(FORMATS[index],*row)for row in table[name])
        if block:
            while len(data)%4:data.append(0)
        offset=len(data)if block else 0
        struct.pack_into('<HHIII',data,64+index*16,index+1,STRIDES[index],offset,len(block)//STRIDES[index],len(block))
        data.extend(block)
    struct.pack_into('<8s6I20s12x',data,0,b'ENCPHN01',1,len(data),0,len(SECTIONS),1,1,bytes.fromhex(commit))
    struct.pack_into('<I',data,16,zlib.crc32(data))
    return bytes(data)


def parse_pack(blob):
    require(HEADER<=len(blob)<=1024*1024,'Phone size')
    magic,schema,size,checksum,count,capabilities,rules,commit=struct.unpack_from('<8s6I20s',blob)
    require((magic,schema,size,count,capabilities,rules)==(b'ENCPHN01',1,len(blob),len(SECTIONS),1,1),'Phone schema/rules/capabilities')
    require(not any(blob[52:64]),'Phone reserved header')
    checked=bytearray(blob);checked[16:20]=b'\0'*4
    require(zlib.crc32(checked)==checksum,'Phone CRC')
    end=HEADER;table={}
    for index,name in enumerate(SECTIONS):
        section,stride,offset,n,length=struct.unpack_from('<HHIII',blob,64+index*16)
        require(section==index+1 and stride==STRIDES[index]and n*stride==length,'Phone directory')
        if not n:
            require(offset==length==0,'Phone empty section')
            table[name]=b''if index==0 else [];continue
        require(offset%4==0 and offset>=end and offset+length<=len(blob)and not any(blob[end:offset]),'Phone section span')
        block=blob[offset:offset+length]
        table[name]=block if index==0 else [list(struct.unpack_from(FORMATS[index],block,i*stride))for i in range(n)]
        end=offset+length
    require(end==len(blob),'Phone trailing bytes')
    return dict(commit=commit.hex(),sections=table)


def compile_pack(root=ROOT):
    root=Path(root)
    ir=json.loads((root/'content/phone-stage/presentation.json').read_text())
    table=lower(ir,root)
    blob=encode(table)
    require(encode(parse_pack(blob)['sections'])==blob,'Phone encode roundtrip')
    return blob


def stage_files(source_root):
    """Return RomFS-relative bytes only after current source/pack/asset checks."""
    source_root=Path(source_root)
    resolved_root=source_root.resolve()
    def read(relative):
        require(not relative.is_absolute()and '..'not in relative.parts
                and all(part not in ('','.','..')for part in relative.parts),'Unsafe phone staged path')
        path=(source_root/relative).resolve()
        require(path.is_relative_to(resolved_root),'Phone staged path escape')
        return path.read_bytes()
    relative=Path('data/opening.encphone')
    blob=read(relative)
    parsed=parse_pack(blob)
    require(blob==compile_pack(ROOT),'Staged phone pack/source IR mismatch')
    files={relative:blob}
    pool=parsed['sections']['Strings']
    for resource in parsed['sections']['Resources']:
        start=resource[1]
        require(0<start<len(pool)and pool[start-1]==0,'Phone resource string offset')
        end=pool.find(b'\0',start)
        require(end>=start,'Phone resource string terminator')
        name=pool[start:end].decode('utf-8')
        require('\\'not in name and ':'not in name,'Unsafe phone resource path')
        path=Path(name)
        data=read(path)
        import hashlib
        require(hashlib.sha256(data).digest()==resource[7],'Staged phone texture fingerprint mismatch')
        require(path not in files,'Duplicate phone staging path')
        files[path]=data
    return files


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('action',choices=['compile','verify']);ap.add_argument('--root',type=Path,default=ROOT)
    args=ap.parse_args()
    try:
        blob=compile_pack(args.root);out=args.root/OUT.relative_to(ROOT)
        if args.action=='compile':
            out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(blob)
            write_json(args.root/'compatibility/reviews/phone-pack.json',dict(schema=1,commit=PIN,
                kind='encore.native-phone.checked-pack',pack_schema=1,capabilities=1,rules=1,
                pack_sha256=sha(out),pack_bytes=len(blob),
                dependencies={p:sha(args.root/p)for p in ['content/phone-presentation-bindings.json','content/phone-stage/presentation.json',
                    'compatibility/reviews/phone-presentation.json','romfs/phone-preview/source.json','romfs/phone-preview/phone.t3x']},
                sections=dict(zip(SECTIONS,STRIDES)),scope='Free InteractDialog phone animation, sound requests and ordered dispatch only'))
        else:require(out.read_bytes()==blob,'Stale compiled phone pack')
        print('Native phone '+args.action+' complete: '+str(len(blob))+' bytes');return 0
    except (ValueError,OSError,KeyError,TypeError,struct.error)as error:
        print('NATIVE PHONE ERROR:',error,file=sys.stderr);return 1


if __name__=='__main__':raise SystemExit(main())
