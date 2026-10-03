#!/usr/bin/env python3
"""Pinned Podunk MusicChanger binding; no geometry or scene activation inference."""
from __future__ import annotations
import argparse, hashlib, json, math, re, shutil, struct, sys, tempfile, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEFAULT_PROJECT=ROOT if (ROOT/'upstream.lock').is_file() else ROOT.parents[1]/'encore-native'
SOURCE='Maps/podunk/podunk.tscn'
CHANGER='Nodes/Overworld/MusicChanger.tscn'
MANAGER='Scripts/global/audioManager.gd'
FLAGS='Scripts/global/globalData.gd'
COMMIT='7d9246600fffe518408f5830d4848635019005a3'
MAGIC=b'ENCMUS01'
MAX_VOICES=16 # native capability ceiling, not a source music priority
class MusicError(ValueError):pass
def require(ok,msg):
    if not ok:raise MusicError(msg)
def sha(b):return hashlib.sha256(b).hexdigest()
def stable(text):return zlib.crc32(text.encode())&0xffffffff
def blocks(text):
    matches=list(re.finditer(r'^\[node (.*?)\]\n',text,re.M))
    out=[]
    for i,m in enumerate(matches):
        fields=dict(re.findall(r'(\w+)="([^"]*)"',m[1]))
        body=text[m.end():matches[i+1].start() if i+1<len(matches) else len(text)]
        props={}
        for line in body.splitlines():
            p=re.match(r'^(\w+) = (.*)$',line)
            if p:props[p[1]]=p[2]
        out.append((fields,props))
    return out
def read_source(upstream):
    text=(upstream/SOURCE).read_text(); rows=blocks(text)
    changer=(upstream/CHANGER).read_text(); match=re.search(r'^script/source = ("(?:[^"\\]|\\.)*")$',changer,re.M)
    require(match,'Embedded MusicChanger script unavailable'); script=json.loads(match[1],strict=False)
    defaults={}
    for key in ('music','loop','appear_flag','disappear_flag','diegetic','volume_db','fadein_length','fadeout_length','disabled'):
        m=re.search(r'^export(?: \([^)]*\))? var '+key+r' = (.+)$',script,re.M)
        require(m,'Unreviewed changer default '+key);defaults[key]=json.loads(m[1])
    regions=[];tracks=[]
    for f,p in rows:
        if f.get('parent')!='Music' or 'instance=ExtResource( 6 )' not in next((m[1] for m in re.finditer(r'^\[node (.*?)\]$',text,re.M) if 'name="'+f['name']+'"' in m[1] and 'parent="Music"' in m[1]),''):continue
        require(set(p)<=set(defaults)|{'position','visible'},'Unreviewed music area property '+f['name'])
        r=defaults.copy()
        for k in defaults:
            if k in p:r[k]=json.loads(p[k])
        require(r['music']=='' and r['loop'] and r['diegetic'] is False,'Unreviewed intro/diegetic region '+f['name'])
        source='res://Audio/Music/'+r['loop']; ident=stable(source)
        if source not in [a['source_path'] for a in tracks]:
            relative=source[6:];tracks.append(dict(stable_id=ident,source_path=source,source_sha256=sha((upstream/relative).read_bytes()),import_sha256=sha((upstream/(relative+'.import')).read_bytes()),pcm_path='audio/podunk-'+str(ident)+'.pcm',gain_db=0))
        path='Music/'+f['name']; shapes=[]
        for sf,sp in rows:
            if sf.get('parent')==path:
                require(sf.get('type') in ('CollisionShape2D','CollisionPolygon2D'),'Unexpected music child '+path)
                shapes.append(path+'/'+sf['name'])
        require(shapes,'Missing area collision binding')
        regions.append(dict(id=stable(SOURCE+':'+path),source_path=path,track_id=ident,volume_db=r['volume_db'],fadein_seconds=r['fadein_length'],fadeout_seconds=r['fadeout_length'],appear_flag=r['appear_flag'],disappear_flag=r['disappear_flag'],disabled=r['disabled'],shape_paths=shapes))
    require(len(regions)==13 and len(tracks)==5,'Pinned Podunk shape/sound scope changed')
    manager=(upstream/MANAGER).read_text()
    silence=float(re.search(r'^const SILENT_SOUND_THRESHOLD = (.+)$',manager,re.M)[1])
    require('func music_fadeto(index, volume = 0, duration = 1):' in manager,'Unreviewed fade-to default')
    source_files=[SOURCE,CHANGER,MANAGER,FLAGS]
    return dict(schema=1,upstream_commit=COMMIT,sources={p:sha((upstream/p).read_bytes()) for p in source_files},silence_db=silence,fade_to_seconds=1,tracks=tracks,regions=regions)
def validate(recipe):
    require(set(recipe)=={'schema','upstream_commit','sources','silence_db','fade_to_seconds','tracks','regions'},'Unknown/missing music recipe field')
    require(recipe['schema']==1 and recipe['upstream_commit']==COMMIT,'Unreviewed music identity')
    require(type(recipe['sources']) is dict and set(recipe['sources'])=={SOURCE,CHANGER,MANAGER,FLAGS},'Unreviewed sources')
    def digest(v):require(type(v)is str and re.fullmatch('[0-9a-f]{64}',v) and int(v,16),'Invalid digest')
    for h in recipe['sources'].values():digest(h)
    def text(v,empty=False):require(type(v)is str and (empty or bool(v)) and len(v)<=1024 and all(32<=ord(c)<127 for c in v),'Invalid text')
    def number(v,lo,hi):require(type(v)in(int,float) and math.isfinite(v) and lo<=v<=hi,'Invalid number')
    number(recipe['silence_db'],-120,-20);number(recipe['fade_to_seconds'],0,60)
    require(type(recipe['tracks'])is list and 1<=len(recipe['tracks'])<=16,'Invalid track count')
    ids=set();paths=set()
    for a in recipe['tracks']:
        require(set(a)=={'stable_id','source_path','source_sha256','import_sha256','pcm_path','gain_db'},'Invalid track fields')
        require(type(a['stable_id'])is int and 0<a['stable_id']<2**32 and a['stable_id'] not in ids,'Invalid track identity');ids.add(a['stable_id'])
        text(a['source_path']);require(a['source_path'].startswith('res://Audio/Music/') and a['source_path'] not in paths,'Invalid track source');paths.add(a['source_path'])
        text(a['pcm_path']);require(a['pcm_path'].startswith('audio/') and '..' not in a['pcm_path'] and ':' not in a['pcm_path'],'Unsafe PCM path')
        digest(a['source_sha256']);digest(a['import_sha256']);number(a['gain_db'],-120,0)
    require(type(recipe['regions'])is list and 1<=len(recipe['regions'])<=64,'Invalid region count')
    rid=set();rpath=set();shapes=set()
    for r in recipe['regions']:
        require(set(r)=={'id','source_path','track_id','volume_db','fadein_seconds','fadeout_seconds','appear_flag','disappear_flag','disabled','shape_paths'},'Invalid region fields')
        require(type(r['id'])is int and 0<r['id']<2**32 and r['id'] not in rid,'Invalid region identity');rid.add(r['id'])
        text(r['source_path']);require(r['source_path'].startswith('Music/') and r['source_path'] not in rpath,'Invalid region source');rpath.add(r['source_path'])
        require(r['track_id'] in ids,'Unknown region track');number(r['volume_db'],-120,24);number(r['fadein_seconds'],0,60);number(r['fadeout_seconds'],0,60)
        text(r['appear_flag'],True);text(r['disappear_flag'],True);require(type(r['disabled'])is bool,'Invalid disabled flag')
        require(type(r['shape_paths'])is list and 1<=len(r['shape_paths'])<=16,'Invalid shapes')
        for p in r['shape_paths']:text(p);require(p.startswith(r['source_path']+'/') and p not in shapes,'Invalid/duplicate shape binding');shapes.add(p)
    return recipe
def build(recipe):
    validate(recipe);data=bytearray(64)
    def u(v):data.extend(struct.pack('<I',v))
    def s(v):b=v.encode('ascii');u(len(b));data.extend(b)
    for a in recipe['tracks']:
        u(a['stable_id']);data.extend(bytes.fromhex(a['source_sha256']));data.extend(bytes.fromhex(a['import_sha256']));s(a['source_path'])
    for r in recipe['regions']:
        u(r['id']);u(r['track_id']);u(int(r['disabled']));data.extend(struct.pack('<3f',r['volume_db'],r['fadein_seconds'],r['fadeout_seconds']));s(r['source_path']);s(r['appear_flag']);s(r['disappear_flag']);u(len(r['shape_paths']))
        for p in r['shape_paths']:s(p)
    struct.pack_into('<8s6I2f',data,0,MAGIC,1,len(data),0,len(recipe['regions']),len(recipe['tracks']),sum(len(r['shape_paths']) for r in recipe['regions']),recipe['silence_db'],recipe['fade_to_seconds'])
    struct.pack_into('<I',data,16,zlib.crc32(data)&0xffffffff);return bytes(data)
def checked(recipe,upstream):
    validate(recipe)
    for p,h in recipe['sources'].items():require(sha((upstream/p).read_bytes())==h,'Reviewed source changed: '+p)
    actual=read_source(upstream);require(actual==recipe,'Binding differs from pinned original source');return build(recipe)
def compile_audio_isolated(recipe,upstream,output,compiler):
    """Compile with generic audio tooling while preserving the existing House bank."""
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='podunk-audio-',dir=output.parent) as temporary:
        staging=Path(temporary);manifest=compiler(recipe,upstream,staging)
        owned={a['pcm_path'] for a in recipe['assets']}|{'data/opening.encaudio'}
        require({row['path'] for row in manifest['files']}==owned,'Unexpected Podunk audio output')
        for row in manifest['files']:
            old=row['path'];target='data/podunk.encaudio' if old=='data/opening.encaudio' else old
            require(target=='data/podunk.encaudio' or re.fullmatch(r'audio/podunk-[0-9]+\.pcm',target),'Non-Podunk output path')
            data=(staging/old).read_bytes();require(len(data)==row['size'] and sha(data)==row['sha256'],'Podunk audio output fingerprint mismatch')
        for row in manifest['files']:
            old=row['path'];target='data/podunk.encaudio' if old=='data/opening.encaudio' else old
            dest=output/target;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(staging/old,dest);row['path']=target
        (output/'data/podunk-audio-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        return manifest
def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['audit','compile','audio']);p.add_argument('--project',type=Path,default=DEFAULT_PROJECT);p.add_argument('--output',type=Path,default=ROOT/'romfs');args=p.parse_args();upstream=args.project/'upstream/MOTHER-Encore';recipe_path=ROOT/'content/podunk-music.json'
    if args.action=='audit':recipe_path.write_text(json.dumps(read_source(upstream),indent=2)+'\n');return
    recipe=json.loads(recipe_path.read_text());binary=checked(recipe,upstream);args.output.mkdir(parents=True,exist_ok=True);(args.output/'data').mkdir(exist_ok=True);(args.output/'data/podunk.encmusic').write_bytes(binary)
    if args.action=='audio':
        sys.path.insert(0,str(args.project/'tools'));import audio_asset
        base=json.loads((args.project/'content/native-audio.json').read_text());base['assets']=recipe['tracks'];(ROOT/'content/podunk-audio.json').write_text(json.dumps(base,indent=2)+'\n')
        compile_audio_isolated(base,upstream,args.output,audio_asset.compile_assets)
    print(json.dumps(dict(region_bytes=len(binary),regions=len(recipe['regions']),tracks=len(recipe['tracks']),shapes=sum(len(r['shape_paths']) for r in recipe['regions']))))
if __name__=='__main__':main()
