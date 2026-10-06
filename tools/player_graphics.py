#!/usr/bin/env python3
"""Four actual Player textures; extract converts, compile only admits outputs."""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,re,struct,subprocess,sys,zlib
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,sha,require
from tools.player_visual_scripts import load as visuals,IR as VISUAL_IR
from tools.extract_battle_entry import Extractor
IR=ROOT/'content/native-player-graphics.json'
REVIEW=ROOT/'reports/player-graphics/source-review.json'
RECEIPT=ROOT/'content/asset-receipts/graphics/player/source.json'
PACK=ROOT/'romfs/data/player.encgraphics'
REUSE_RECEIPT=ROOT/'content/asset-receipts/graphics/actors/source.json'
FAMILY=0x454e005b
def write(p,d):p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes((json.dumps(d,indent=2,ensure_ascii=False)+'\n').encode())
def build():
    v=visuals();ex=Extractor(ROOT);required={}
    for c in v['shadow']['clips']:
        for f in c['frames']:
            previous=required.setdefault(f['source'],dict(id=f['texture_id'],sha=f['source_sha256'],regions=[]))
            require(previous['id']==f['texture_id'] and previous['sha']==f['source_sha256'],'Player texture identity collision')
            previous['regions'].append(f['rect'])
    b=v['bat'];require(b['texture']not in required,'Bat source texture alias changed');required[b['texture']]=dict(id=b['texture_id'],sha=b['texture_sha256'],regions=[])
    assets=[];reuse=read(REUSE_RECEIPT);ex.text('LICENSE')
    for source,a in required.items():
        raw=ex.data(source);require(ex.sources[source]==a['sha']and raw[:8]==b'\x89PNG\r\n\x1a\n','Player actual PNG source differs')
        size=list(struct.unpack('>II',raw[16:24]));require(all(0<x<=1024 for x in size),'Player texture exceeds actual GPU dimensions')
        imp=ex.text(source+'.import')
        require(all(line in imp.splitlines()for line in ['flags/filter=false','flags/repeat=0','flags/mipmaps=false','flags/anisotropic=false','process/premult_alpha=false','process/invert_color=false','compress/mode=0']),'Player source nearest/nonrepeat import differs')
        for x,y,w,h in a['regions']:require(x>=0 and y>=0 and w>0 and h>0 and x+w<=size[0]and y+h<=size[1],'Shadow frame outside source texture')
        if source==b['texture']:require(size[0]%b['columns']==size[1]%b['rows']==0,'Bat source frame grid does not divide texture')
        reused=source==reuse['recipe']['shadow_texture']
        if reused:
            require(reuse['schema']==1 and reuse['recipe']['commit']==PIN and reuse['recipe']['shadow_texture_sha256']==a['sha'],'Existing Shadow source receipt differs')
            path='graphics/actors/shadow.t3x';output=reuse['outputs']['shadow.t3x'];require(sha(ROOT/'romfs'/path)==output['sha256']and(ROOT/'romfs'/path).stat().st_size==output['bytes'],'Existing Shadow converted bytes differ')
        else:path='graphics/player/'+Path(source).stem.lower()+'.t3x'
        assets.append(dict(id=a['id'],source=source,source_sha256=a['sha'],import_sha256=ex.sources[source+'.import'],size=size,path=path,reused=reused,nearest=True,repeat=False))
    require(len({a['id']for a in assets})==len(assets)and len({a['path']for a in assets})==len(assets),'Player GPU source aliases')
    return dict(schema=1,format=1,capability=1,rules=1,family=FAMILY,commit=PIN,scene=v['scene'],scene_id=v['scene_id'],source_sha256=v['source_sha256'],visual_ir_sha256=sha(VISUAL_IR),assets=assets,sources=ex.sources,reuse_receipt_sha256=sha(REUSE_RECEIPT),licence_review='Original art retained under pinned upstream LICENSE; permitted game-related fork/modification, not MIT relicensing.')
def load():
    d=read(IR);r=read(REVIEW);require(d==build()and r['ir_sha256']==sha(IR)and r['sources']==d['sources']and r['commit']==PIN,'Player graphics source/dependency review differs');return d
def extract(tex3ds):
    require(tex3ds and Path(tex3ds).is_file(),'Explicit extract requires genuine tex3ds')
    d=build();write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],semantics=['Four exact source texture identities from reviewed Player visual scripts','Lossless RGBA8 single whole-image atlas; nearest/no repeat/mipmap from source import','Existing reviewed Shadow GPU output reused without new encoding'],unverified=['GPU loading/drawing requires actual native owner; no tests/emulator/hardware run']))
    def convert(a):
        path=ROOT/'romfs'/a['path'];path.parent.mkdir(parents=True,exist_ok=True)
        if not a['reused']:subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(path),str(ROOT/'upstream/MOTHER-Encore'/a['source'])],check=True)
        return dict(id=a['id'],source=a['source'],source_sha256=a['source_sha256'],path=a['path'],bytes=path.stat().st_size,output_sha256=sha(path),size=a['size'],reused=a['reused'])
    with concurrent.futures.ThreadPoolExecutor(max_workers=4)as pool:outputs=list(pool.map(convert,d['assets']))
    write(RECEIPT,dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),tex3ds_sha256=sha(tex3ds),workers=4,outputs=outputs));compile_pack()
def outputs(d):
    r=read(RECEIPT);require(r['schema']==1 and r['commit']==PIN and r['ir_sha256']==sha(IR)and r['producer_sha256']==sha(Path(__file__))and re.fullmatch('[0-9a-f]{64}',r['tex3ds_sha256'])and r['workers']>=4 and len(r['outputs'])==len(d['assets']),'Player GPU conversion receipt differs')
    for a,o in zip(d['assets'],r['outputs']):
        require(all(o[k]==a[k]for k in ['id','source','source_sha256','path','size','reused']),'Player GPU output provenance differs')
        p=ROOT/'romfs'/o['path'];require(o['bytes']==p.stat().st_size and 0<o['bytes']<=64*1024*1024 and o['output_sha256']==sha(p),'Player GPU converted output differs')
    return r['outputs']
def encode(d,o):
    b=bytearray(128)
    def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
    def text(s):raw=s.encode();u(len(raw));b.extend(raw)
    b.extend(bytes.fromhex(d['visual_ir_sha256']));u(len(o))
    for a,r in zip(d['assets'],o):
        u(a['id'],*a['size'],r['bytes'],int(a['nearest']),int(a['repeat']));text(a['source']);text(a['path']);b.extend(bytes.fromhex(a['source_sha256']));b.extend(bytes.fromhex(a['import_sha256']));b.extend(bytes.fromhex(r['output_sha256']))
    struct.pack_into('<8s8I',b,0,b'ENCPGFX1',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)
def compile_pack():
    d=load();raw=encode(d,outputs(d));PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw);return raw
def stage_files(source_root):
    d=load();o=outputs(d);raw=encode(d,o);root=Path(source_root);require((root/'data/player.encgraphics').read_bytes()==raw,'Staged Player GPU binary differs');files={Path('data/player.encgraphics'):raw}
    for a in o:
        p=root/a['path'];data=p.read_bytes();require(len(data)==a['bytes']and hashlib.sha256(data).hexdigest()==a['output_sha256'],'Staged Player GPU image differs '+a['path']);files[Path(a['path'])]=data
    return files
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
    try:
        if a.action=='extract':extract(a.tex3ds)
        elif a.action=='compile':compile_pack()
        else:d=load();require(PACK.read_bytes()==encode(d,outputs(d)),'Player GPU binary stale')
        print('Player graphics: four source images; complete checked GPU staging')
    except(ValueError,KeyError,TypeError,OSError,subprocess.CalledProcessError,struct.error)as e:sys.exit('PLAYER GRAPHICS ERROR: '+str(e))
