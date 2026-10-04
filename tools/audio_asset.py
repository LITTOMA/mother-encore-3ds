#!/usr/bin/env python3
"""Compile pinned original audio to independent PCM and checked ENCAUD01 metadata.

Offline only. This tool does not generate C++, alter the room schema, bundle DSP
firmware, or make an audibility claim. Output conversion is recorded verbatim.
"""
from __future__ import annotations
import argparse
import configparser
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT=Path(__file__).resolve().parents[1]

def manifest_path(output, bank='opening'):
    output = Path(output)
    if output.resolve() == (ROOT / 'romfs').resolve():
        return ROOT / 'content/asset-receipts/audio' / (bank + '.json')
    return output / 'data' / (bank + '-audio-manifest.json')
sys.path.insert(0,str(ROOT))
HEADER=64
STRIDE=96
MAGIC=b'ENCAUD01'
MAX_META=65536

class AudioError(ValueError): pass

def check(condition,message):
    if not condition: raise AudioError(message)

def exact(value,fields,label):
    check(type(value) is dict and set(value)==set(fields),label+': unknown/missing fields')

def safe_path(value):
    check(type(value) is str and value and value.isascii() and all(32<=ord(c)<=126 for c in value),'Invalid path encoding')
    check(not any(c in value for c in ':\\') and all(p not in ('','.','..') for p in value.split('/')),'Unsafe audio path')
    return value

def source_path(value):
    check(type(value) is str and value.startswith('res://'),'Expected res:// audio source')
    return safe_path(value[6:])

def sha(data): return hashlib.sha256(data).hexdigest()

def verified(root,path,digest):
    check(type(digest) is str and re.fullmatch(r'[0-9a-f]{64}',digest) is not None and int(digest,16)!=0,'Invalid source digest')
    path=safe_path(path); target=(root/path).resolve()
    check(target.is_relative_to(root.resolve()),'Source escaped upstream root')
    data=target.read_bytes();check(sha(data)==digest,'Reviewed audio source fingerprint mismatch: '+path);return data

def import_settings(data,source):
    c=configparser.ConfigParser(interpolation=None,strict=True)
    try:c.read_string(data.decode('utf-8'))
    except (UnicodeError,configparser.Error) as e:raise AudioError('Malformed Godot audio importer') from e
    check(not c.defaults() and set(c.sections())=={'remap','deps','params'},'Unsupported Godot audio importer sections')
    check(set(c['remap'])=={'importer','type','path'} and set(c['deps'])=={'source_file','dest_files'},'Unsupported Godot importer/dependency fields')
    check(c['deps'].get('source_file')==json.dumps(source),'Importer source mismatch')
    if c['remap'].get('importer')=='"wav"':
        # The reviewed menu WAVs use untouched, uncompressed, nonlooping PCM.
        # Reject every importer operation that would require a separate adapter.
        expected={'force/8_bit':'false','force/mono':'false','force/max_rate':'false',
                  'force/max_rate_hz':'44100','edit/trim':'false','edit/normalize':'false',
                  'edit/loop_mode':'0','edit/loop_begin':'0','edit/loop_end':'-1','compress/mode':'0'}
        check(c['remap'].get('type')=='"AudioStreamSample"' and dict(c['params'])==expected,
              'Unsupported WAV sample transformation or loop')
        return False,0.0
    check(c['remap'].get('importer') in ('"ogg_vorbis"','"mp3"'),'Unsupported Godot audio codec')
    check(set(c['params'])=={'loop','loop_offset'},'Unsupported Godot audio import params')
    loop=c['params']['loop'];check(loop in ('true','false'),'Invalid loop flag')
    try:offset=float(c['params']['loop_offset'])
    except ValueError as e:raise AudioError('Invalid loop offset') from e
    check(math.isfinite(offset) and offset>=0 and (loop=='true' or offset==0),'Invalid loop offset')
    return loop=='true',offset

def build_bank(records,master_db,silence_db):
    check(1<=len(records)<=64,'Audio asset count out of bounds')
    check(all(type(v) in (int,float) and math.isfinite(v) and -120<=v<=0 for v in (master_db,silence_db)) and silence_db<=-20,'Invalid bank gain')
    strings=bytearray();directory=bytearray();base=HEADER+STRIDE*len(records)
    for record in records:
        p=bytearray(STRIDE)
        struct.pack_into('<I32sIHHIIII',p,0,record['stable_id'],bytes.fromhex(record['source_sha256']),record['sample_rate'],record['channels'],int(record['loop']),record['frames'],record['loop_start'],record['pcm_bytes'],record['pcm_crc32'])
        for field,key in ((60,'pcm_path'),(68,'source_path')):
            value=record[key].encode('ascii');struct.pack_into('<II',p,field,base+len(strings),len(value));strings.extend(value+b'\0')
        struct.pack_into('<f',p,76,record['gain_db']);directory.extend(p)
    out=bytearray(HEADER)+directory+strings
    struct.pack_into('<8s7I2f',out,0,MAGIC,1,len(out),0,len(records),STRIDE,base,len(strings),master_db,silence_db)
    struct.pack_into('<I',out,16,zlib.crc32(out)&0xffffffff)
    parse_bank(bytes(out));return bytes(out)

def parse_bank(data):
    check(HEADER<=len(data)<=MAX_META,'Audio bank size out of bounds')
    magic,version,size,crc,count,stride,strings,string_bytes,master,silence=struct.unpack_from('<8s7I2f',data)
    check(magic==MAGIC and version==1,'Unsupported audio bank version')
    check(size==len(data) and stride==STRIDE and data[44:64]==bytes(20),'Invalid audio bank header')
    check(1<=count<=64 and strings==HEADER+count*STRIDE and strings<=size and string_bytes==size-strings,'Invalid audio bank directory')
    check(all(math.isfinite(v) and -120<=v<=0 for v in (master,silence)) and silence<=-20,'Invalid audio bank gains')
    copy=bytearray(data);copy[16:20]=bytes(4);check(zlib.crc32(copy)&0xffffffff==crc,'Audio bank checksum mismatch')
    result=[];next_string=strings;ids=set();paths=set();sources=set()
    for i in range(count):
        p=data[HEADER+i*STRIDE:HEADER+(i+1)*STRIDE]
        ident,source_hash,rate,channels,flags,frames,loop_start,pcm_bytes,pcm_crc=struct.unpack_from('<I32sIHHIIII',p)
        gain=struct.unpack_from('<f',p,76)[0]
        check(ident and ident not in ids and any(source_hash) and p[80:]==bytes(16),'Invalid/duplicate audio identity or reserved data');ids.add(ident)
        check(8000<=rate<=48000 and channels in (1,2) and flags in (0,1) and frames and frames*channels*2==pcm_bytes and pcm_bytes<=64*1024*1024,'Unsupported audio PCM dimensions')
        check((flags and loop_start<frames) or (not flags and loop_start==0),'Invalid audio loop')
        check(math.isfinite(gain) and -120<=gain<=0,'Invalid asset gain')
        r=dict(stable_id=ident,source_sha256=source_hash.hex(),sample_rate=rate,channels=channels,loop=bool(flags),frames=frames,loop_start=loop_start,pcm_bytes=pcm_bytes,pcm_crc32=pcm_crc,gain_db=gain)
        for offset,key in ((60,'pcm_path'),(68,'source_path')):
            start,length=struct.unpack_from('<II',p,offset)
            check(start==next_string and 1<=length<=1024 and start+length<size and data[start+length]==0,'Invalid audio string bounds/order')
            try:r[key]=data[start:start+length].decode('ascii')
            except UnicodeError as e:raise AudioError('Invalid audio path encoding') from e
            (safe_path if offset==60 else source_path)(r[key]);next_string+=length+1
        check(r['pcm_path'] not in paths and r['source_path'] not in sources,'Duplicate audio path');paths.add(r['pcm_path']);sources.add(r['source_path']);result.append(r)
    check(next_string==size,'Trailing audio bank data');return dict(master_db=master,silence_db=silence,assets=result)

def compile_assets(recipe,upstream,output,ffmpeg='ffmpeg',ffprobe='ffprobe'):
    exact(recipe,['schema','upstream_commit','bus_source','bus_sha256','manager_source','manager_sha256','assets'],'Audio recipe')
    check(recipe['schema']==1 and re.fullmatch('[0-9a-f]{40}',recipe['upstream_commit']) is not None,'Unsupported recipe identity')
    manager=verified(upstream,recipe['manager_source'],recipe['manager_sha256']).decode('utf8')
    buses=verified(upstream,recipe['bus_source'],recipe['bus_sha256']).decode('utf8')
    floor_match=re.search(r'^const SILENT_SOUND_THRESHOLD = (-?[0-9.]+)$',manager,re.M)
    master_match=re.search(r'^bus/0/volume_db = (-?[0-9.]+)$',buses,re.M)
    check(floor_match and master_match,'Unrecognized audited audio tuning')
    master=float(master_match[1]);floor=float(floor_match[1])
    check(type(recipe['assets']) is list and 1<=len(recipe['assets'])<=64,'Invalid asset list')
    ffmpeg_path=shutil.which(ffmpeg);ffprobe_path=shutil.which(ffprobe);check(ffmpeg_path and ffprobe_path,'FFmpeg/ffprobe unavailable')
    version=subprocess.run([ffmpeg_path,'-version'],check=True,text=True,capture_output=True).stdout
    records=[];receipts=[];payloads={}
    with tempfile.TemporaryDirectory(prefix='encore-audio-') as tmp:
        for index,entry in enumerate(recipe['assets']):
            fields=['stable_id','source_path','source_sha256','import_sha256','pcm_path','gain_db']
            if 'output_sample_rate' in entry:fields.append('output_sample_rate')
            exact(entry,fields,'Audio asset recipe')
            check(type(entry['stable_id']) is int and 0<entry['stable_id']<2**32,'Invalid stable audio ID')
            check(type(entry['gain_db']) in (int,float) and math.isfinite(entry['gain_db']) and -120<=entry['gain_db']<=0,'Invalid asset gain')
            relative=source_path(entry['source_path']);safe_path(entry['pcm_path'])
            check(entry['pcm_path'] not in ('sound/banks/opening.encaudio','data/opening-audio-manifest.json'),'PCM path collides with metadata')
            verified(upstream,relative,entry['source_sha256'])
            imported=verified(upstream,relative+'.import',entry['import_sha256']);loop,offset=import_settings(imported,entry['source_path'])
            src=upstream/relative
            probe=json.loads(subprocess.run([ffprobe_path,'-v','error','-show_entries','stream=codec_type,channels,sample_rate','-of','json',str(src)],check=True,text=True,capture_output=True).stdout)
            check(len(probe['streams'])==1 and probe['streams'][0]['codec_type']=='audio','Expected a single audio stream')
            stream=probe['streams'][0];source_rate=int(stream['sample_rate']);channels=int(stream['channels'])
            rate=entry.get('output_sample_rate',source_rate)
            check(8000<=source_rate<=192000 and type(rate)is int and 8000<=rate<=48000 and channels in (1,2),'Unsupported source/output audio format')
            dest=Path(tmp)/f'{index}.pcm'
            command=[ffmpeg_path,'-nostdin','-v','error','-i',str(src),'-map','0:a:0','-vn','-sn','-dn','-c:a','pcm_s16le','-f','s16le','-bitexact','-threads','1']
            if rate!=source_rate:command+=['-ar',str(rate)]
            command.append(str(dest))
            subprocess.run(command,check=True,capture_output=True);pcm=dest.read_bytes()
            check(pcm and len(pcm)%(2*channels)==0 and len(pcm)<=64*1024*1024,'Invalid decoded PCM size')
            frames=len(pcm)//(channels*2);loop_frame=int(offset*rate)
            check((loop and loop_frame<frames) or (not loop and loop_frame==0),'Loop point outside decoded stream')
            record=dict(stable_id=entry['stable_id'],source_sha256=entry['source_sha256'],sample_rate=rate,channels=channels,loop=loop,frames=frames,loop_start=loop_frame,pcm_bytes=len(pcm),pcm_crc32=zlib.crc32(pcm)&0xffffffff,pcm_path=entry['pcm_path'],source_path=entry['source_path'],gain_db=entry['gain_db'])
            records.append(record);payloads[entry['pcm_path']]=pcm
            receipts.append(dict(**record,source_sample_rate=source_rate,import_sha256=entry['import_sha256'],loop_offset_seconds=offset,pcm_sha256=sha(pcm),command=command))
    bank=build_bank(records,master,floor);payloads['sound/banks/opening.encaudio']=bank
    manifest=dict(schema=1,upstream_commit=recipe['upstream_commit'],recipe_sha256=sha(json.dumps(recipe,sort_keys=True,separators=(',',':')).encode()),ffmpeg_version=version,ffmpeg_sha256=sha(Path(ffmpeg_path).read_bytes()),assets=receipts,files=[dict(path=path,size=len(data),sha256=sha(data)) for path,data in sorted(payloads.items())],scope='PCM decode and stream metadata only; no DSP firmware; no hardware audibility or Godot decoder bit-exactness claim')
    for path,data in payloads.items():target=output/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    report=manifest_path(output);report.parent.mkdir(parents=True,exist_ok=True);report.write_text(json.dumps(manifest,indent=2)+'\n');return manifest

def stage_files(source):
    """Return only verified audio paths/bytes for the existing host staging tool."""
    manifest=json.loads(manifest_path(source).read_text());check(manifest['schema']==1,'Unsupported audio staging manifest')
    files={}
    for item in manifest['files']:
        path=safe_path(item['path']);resolved=(source/path).resolve();check(resolved.is_relative_to(source.resolve()),'Staged audio escaped root');data=resolved.read_bytes()
        check(len(data)==item['size'] and sha(data)==item['sha256'],'Audio staged fingerprint mismatch: '+path)
        check(path not in files,'Duplicate staged audio file');files[path]=data
    check('sound/banks/opening.encaudio' in files,'Audio metadata absent')
    bank=parse_bank(files['sound/banks/opening.encaudio'])
    check(set(files)=={'sound/banks/opening.encaudio'}|{a['pcm_path'] for a in bank['assets']},'Audio staging manifest contains unrelated/missing files')
    for a in bank['assets']:
        pcm=files[a['pcm_path']];check(len(pcm)==a['pcm_bytes'] and zlib.crc32(pcm)&0xffffffff==a['pcm_crc32'],'Audio PCM metadata mismatch')
    return {Path(p):data for p,data in files.items()}

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('action',choices=['compile','verify'],nargs='?',default='compile');parser.add_argument('--recipe',type=Path,default=ROOT/'content/native-audio.json');parser.add_argument('--upstream',type=Path,default=ROOT/'upstream/MOTHER-Encore');parser.add_argument('--output',type=Path,default=ROOT/'romfs');args=parser.parse_args()
    recipe=json.loads(args.recipe.read_text());
    if args.recipe.resolve()==(ROOT/'content/native-audio.json').resolve():
        from tools.phone_linker_bindings import verify_audio
        verify_audio(recipe,ROOT)
    lock=json.loads((ROOT/'upstream.lock').read_text());check(recipe['upstream_commit']==lock['commit'],'Audio recipe/upstream lock mismatch')
    if args.action=='verify':
        manifest=json.loads(manifest_path(args.output).read_text())
        check(manifest['recipe_sha256']==sha(json.dumps(recipe,sort_keys=True,separators=(',',':')).encode()),'Audio recipe changed since compilation')
        verified(args.upstream,recipe['bus_source'],recipe['bus_sha256']);verified(args.upstream,recipe['manager_source'],recipe['manager_sha256'])
        for entry in recipe['assets']:
            relative=source_path(entry['source_path']);verified(args.upstream,relative,entry['source_sha256']);verified(args.upstream,relative+'.import',entry['import_sha256'])
        stage_files(args.output);print('Verified original audio sources and checked PCM bank');return
    manifest=compile_assets(recipe,args.upstream,args.output)
    print(f"Compiled {len(manifest['assets'])} original audio assets, {sum(a['pcm_bytes'] for a in manifest['assets'])} PCM bytes; no C++ compiler used")
if __name__=='__main__':main()
