#!/usr/bin/env python3
"""Actual Player texture identities and independently mutable scene materials."""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,re,struct,subprocess,sys,zlib
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,sha,require,decode
from tools.extract_battle_entry import Extractor,properties
from tools.player_initialization import load as initial,IR as INIT
from tools.player_graphics import load as graphics,outputs as graphics_outputs,IR as GFX
IR=ROOT/'content/native-player-resources.json';REVIEW=ROOT/'reports/player-resources/source-review.json'
RECEIPT=ROOT/'content/asset-receipts/graphics/player/resources-source.json';PACK=ROOT/'romfs/data/player.encresources';FAMILY=0x454e005f

def write(p,d):p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes((json.dumps(d,ensure_ascii=False,indent=2)+'\n').encode())
def stable(s):return int.from_bytes(hashlib.sha256(('player-resource:'+s).encode()).digest()[:4],'little')
def derive():
 d=initial();g=graphics();go=graphics_outputs(g);ex=Extractor(ROOT);ex.data('LICENSE');native=d['native_snapshot'];rows=[];images={};materials=[];nodes=[];audios=[]
 reuse={a['source']:(a['path'],o['bytes'],o['output_sha256'])for a,o in zip(g['assets'],go)}
 actor=read(ROOT/'content/asset-receipts/graphics/actors/source.json');require(actor['recipe']['commit']==PIN,'Actor source pin differs')
 for k,out in [('texture','ninten-main.t3x'),('emote_texture','emotes.t3x')]:
  source=actor['recipe'][k];require(actor['recipe'][k+'_sha256']==ex.sources.setdefault(source,sha(ROOT/'upstream/MOTHER-Encore'/source)),'Actor source texture differs');o=actor['outputs'][out];reuse[source]=('graphics/actors/'+out,o['bytes'],o['sha256'])
 def image(source):
  if source in images:return images[source]
  raw=ex.data(source);size=ex.png_size(source);imp=ex.text(source+'.import')
  require(all(0<x<=1024 for x in size),'Player GPU texture too large')
  require(all(x in imp.splitlines()for x in ['flags/filter=false','flags/repeat=0','flags/mipmaps=false','flags/anisotropic=false','process/premult_alpha=false','process/invert_color=false','compress/mode=0']),'Player texture sampling/import unknown')
  path=reuse[source][0]if source in reuse else'graphics/player/resource-'+str(stable(source))+'.t3x'
  images[source]=dict(id=stable(source),source=source,source_sha256=ex.sources[source],import_sha256=ex.sources[source+'.import'],size=size,path=path,reused=source in reuse)
  if source in reuse:
   o=ROOT/'romfs'/path;require(o.stat().st_size==reuse[source][1]and sha(o)==reuse[source][2],'Player reused converted texture differs')
  return images[source]
 scene='Nodes/Reusables/Player.tscn';text=ex.text(scene)
 def sub(scene,key,typ):
  text=ex.text(scene);m=re.search(r'^\[sub_resource type="'+typ+'" id='+str(key)+r'\]\n(.*?)(?=^\[|\Z)',text,re.M|re.S);require(m,'Missing source resource '+scene+':'+str(key));return {'code':json.loads(m[1].split('code = ',1)[1].strip(),strict=False)} if typ=='Shader' else properties(re.sub(r'Rect2\(\s*([^()]*)\)',r'[\1]',m[1]))
 for a in native['resources']:
  kind=a['class']
  if kind=='StreamTexture':
   im=image(a['path'][6:]);rows.append(dict(id=a['id'],kind=1,source=im['source'],texture=im['id'],rect=[0,0,*im['size']],local=False,instanced=False,shader=0,parameters=[]))
  elif kind=='AtlasTexture':
   path,key=a['path'][6:].split('::');v=sub(path,key,kind);require(set(v)=={'atlas','region'},'Unknown AtlasTexture margin/filter policy');ref=v['atlas']['ExtResource'];s=ex.text(path);m=re.search(r'\[ext_resource path="res://([^"\n]+)" type="Texture" id='+str(ref)+r'\]',s);require(m,'Atlas underlying texture missing');im=image(m[1]);rect=v['region'];require(len(rect)==4 and all(x>=0 for x in rect)and rect[2]>0 and rect[3]>0 and rect[0]+rect[2]<=im['size'][0]and rect[1]+rect[3]<=im['size'][1],'Atlas outside source image');rows.append(dict(id=a['id'],kind=2,source=path,texture=im['id'],rect=rect,local=False,instanced=False,shader=0,parameters=[]))
  elif kind=='ShaderMaterial':
   props=decode(a['properties']);require(props['script']is None and props['render_priority']==0,'Unknown source Material script/priority')
   source_path=a['path'][6:]if a['path']else scene+'::549';path,key=source_path.split('::');v=sub(path,key,kind);sid=props['shader']['id'];res=native['resources'][sid]
   require(res['id']==sid and res['class']=='Shader','Material shader reference differs')
   if '::'in res['path']:
    sh=sub(path,res['path'].split('::')[-1],'Shader')['code'];kernel=1
    require(sh=='shader_type canvas_item;\n\nuniform vec4 flash_color : hint_color = vec4(1.0);\nuniform vec4 glow_color : hint_color = vec4(1.0);\nuniform float flash_modifier : hint_range(0.0, 1.0) = 0.0;\nuniform float glow_modifier : hint_range(0.0, 1.0) = 0.0;\n\nvoid fragment() {\n\tvec4 color = texture(TEXTURE, UV);\n\tcolor.rgb = color.rgb + glow_color.rgb * glow_modifier;\n\tcolor.rgb = mix(color.rgb, flash_color.rgb, flash_modifier);\n\tCOLOR = color;\n}\n','Unknown Flash fragment source')
    roles=['flash_color','glow_color','flash_modifier','glow_modifier']
   else:
    sh=ex.text(res['path'][6:]);kernel=2;roles=['color','width','pattern','inside'];require('hasContraryNeighbour'in sh and 'COLOR.a += (1.0 - COLOR.a) * color.a;'in sh,'Unknown outline shader')
   params=[]
   for j,keyname in enumerate(roles):
    value=v['shader_param/'+keyname];k=2 if isinstance(value,list)else 3 if isinstance(value,bool)else 4 if isinstance(value,int)else 1;val=value if isinstance(value,list)else[float(value),0,0,0];require(len(val)==4,'Unknown material Color shape');params.append(dict(role=j+1,name=keyname,kind=k,value=val))
   rows.append(dict(id=a['id'],kind=3,source=path,texture=0,rect=[0,0,0,0],local=props['resource_local_to_scene'],instanced=not bool(a['path']),shader=kernel,parameters=params))
 for a in native['resources']:
  if a['class'] in ('AudioStreamSample','AudioStreamMP3'):
   source=a['path'][6:];ex.data(source);imp=ex.text(source+'.import');require('loop=false'in imp or 'loop=true'in imp,'Player audio loop policy unknown');require('loop_offset=0'in imp,'Nonzero Player MP3 loop offset unsupported');audios.append(dict(loop='loop=true'in imp,id=a['id'],native=a['class'],source=source,source_sha256=ex.sources[source],import_sha256=ex.sources[source+'.import'],path='sound/effects/player-'+str(stable(source))+'.pcm'))
 for f in d['fields']:
  if f.get('adapter')==2:
   source=f['resource'];require(f['native']=='AudioStreamSample','Player constructor audio source type changed');ex.data(source);imp=ex.text(source+'.import');require('compress/mode=0'in imp and 'force/mono=false'in imp and 'force/8_bit=false'in imp and 'edit/loop_mode=0'in imp,'Unknown Player WAV importer');audios.append(dict(loop=False,id=stable(source),native=f['native'],source=source,source_sha256=ex.sources[source],import_sha256=ex.sources[source+'.import'],path='sound/effects/player-'+str(stable(source))+'.pcm'))
 for n in native['nodes']:
  p=decode(n['properties']);texture=p.get('texture');material=p.get('material')
  if texture is not None or material is not None:
   texture=texture['id']if texture is not None else 0xffffffff;material=material['id']if material is not None else 0xffffffff
   require(all(any(a['id']==k for a in rows)for k in [texture,material]if k!=0xffffffff),'Native Sprite binding has unknown resource')
   cols=p.get('hframes',1);rs=p.get('vframes',1);frame=p.get('frame',0)
   if texture!=0xffffffff:
    r=next(a for a in rows if a['id']==texture);require(r['rect'][2]%cols==r['rect'][3]%rs==0 and 0<=frame<cols*rs,'Player frame grid outside source')
   nodes.append(dict(path=n['path'],texture=texture,material=material,columns=cols,rows=rs,frame=frame))
 require(len(rows)==len({a['id']for a in rows}),'Native Player resource ID collision')
 return dict(schema=1,format=1,capability=1,rules=1,family=FAMILY,commit=PIN,scene=d['scene'],scene_id=d['scene_id'],source_sha256=d['source_sha256'],initialization_ir_sha256=sha(INIT),graphics_ir_sha256=sha(GFX),images=list(images.values()),audios=audios,resources=rows,nodes=nodes,sources=ex.sources,unsupported=['Non-neutral outline/flash draw kernel pending; parameters are owned and checked, never ignored','Unknown textures, node resource classes and out-of-source material parameters reject','Complete PackedScene factories remain their actual owner obligation'])
def load():
 d=read(IR);r=read(REVIEW);require(d==derive()and r['ir_sha256']==sha(IR)and r['sources']==d['sources'],'Player resources review/dependency differs');return d

def extract(tex3ds,ffmpeg,ffprobe):
 require(tex3ds and Path(tex3ds).is_file(),'Actual tex3ds required');d=derive();write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],execution='platform/ctr/podunk_player_resources.cpp',semantics=['Original native resources, frame grids and sampling','Local-to-scene Flash398 is a distinct instance from template1 and Outline42','Real source GPU texture allocations; no placeholder Resource'],unverified=['No tests/emulator/hardware; non-neutral material rendering rejects']))
 def convert(a):
  p=ROOT/'romfs'/a['path'];p.parent.mkdir(parents=True,exist_ok=True)
  if not a['reused']:subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(p),str(ROOT/'upstream/MOTHER-Encore'/a['source'])],check=True)
  return dict(id=a['id'],bytes=p.stat().st_size,output_sha256=sha(p),path=a['path'])
 with concurrent.futures.ThreadPoolExecutor(max_workers=4)as pool:o=list(pool.map(convert,d['images']))
 def audio(a):
  probe=json.loads(subprocess.check_output([str(ffprobe),'-v','error','-show_streams','-of','json',str(ROOT/'upstream/MOTHER-Encore'/a['source'])]));streams=[x for x in probe['streams']if x['codec_type']=='audio'];require(len(streams)==1,'Player audio stream ambiguity');stream=streams[0];rate=int(stream['sample_rate']);channels=int(stream['channels']);require(rate>0 and channels in(1,2),'Player audio source channels/rate unsupported');p=ROOT/'romfs'/a['path'];p.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(ffmpeg),'-v','error','-y','-i',str(ROOT/'upstream/MOTHER-Encore'/a['source']),'-map','0:a:0','-ac',str(channels),'-ar',str(rate),'-f','s16le',str(p)],check=True);require(p.stat().st_size%(channels*2)==0,'Player actual PCM frames invalid');return dict(id=a['id'],path=a['path'],bytes=p.stat().st_size,output_sha256=sha(p),rate=rate,channels=channels,frames=p.stat().st_size//(channels*2),format=16,loop=a['loop'])
 with concurrent.futures.ThreadPoolExecutor(max_workers=4)as pool:audio_out=list(pool.map(audio,d['audios']))
 write(RECEIPT,dict(schema=1,commit=PIN,producer_sha256=sha(Path(__file__)),ir_sha256=sha(IR),tex3ds_sha256=sha(tex3ds),workers=4,outputs=o,audios=audio_out,ffmpeg_sha256=sha(ffmpeg),ffprobe_sha256=sha(ffprobe)));compile_pack()
def outputs(d):
 r=read(RECEIPT);require(r['commit']==PIN and r['producer_sha256']==sha(Path(__file__))and r['ir_sha256']==sha(IR)and r['workers']>=4 and len(r['outputs'])==len(d['images']),'Player resource GPU receipt differs')
 for a,o in zip(d['images'],r['outputs']):
  p=ROOT/'romfs'/a['path'];require(a['id']==o['id']and a['path']==o['path']and p.stat().st_size==o['bytes']and sha(p)==o['output_sha256'],'Player resource converted bytes differ')
 require(len(r['audios'])==len(d['audios']),'Player audio receipt count differs')
 for a,o in zip(d['audios'],r['audios']):
  p=ROOT/'romfs'/a['path'];require(a['id']==o['id']and a['path']==o['path']and p.stat().st_size==o['bytes']and sha(p)==o['output_sha256']and o['channels']in(1,2)and o['format']==16 and o['loop']==a['loop']and o['frames']*o['channels']*2==o['bytes'],'Player PCM receipt differs')
 return r['outputs']
def encode(d,o):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def s(v):z=v.encode();u(len(z));b.extend(z)
 def h(v):b.extend(bytes.fromhex(v))
 h(d['initialization_ir_sha256']);h(d['graphics_ir_sha256']);u(len(d['sources']))
 for p,v in sorted(d['sources'].items()):s(p);h(v)
 u(len(d['images']))
 for a,v in zip(d['images'],o):u(a['id'],*a['size'],v['bytes']);s(a['source']);s(a['path']);h(a['source_sha256']);h(a['import_sha256']);h(v['output_sha256'])
 ao=read(RECEIPT)['audios'];u(len(ao))
 for a,v in zip(d['audios'],ao):u(a['id'],1 if a['native']=='AudioStreamSample'else 2,v['rate'],v['channels'],v['frames'],v['bytes'],int(v['loop']));s(a['source']);s(a['path']);h(a['source_sha256']);h(a['import_sha256']);h(v['output_sha256'])
 u(len(d['resources']))
 for a in d['resources']:
  u(a['id'],a['kind'],a['texture'],int(a['local'])|int(a['instanced'])<<1,a['shader']);s(a['source']);f(*a['rect']);u(len(a['parameters']))
  for p in a['parameters']:u(p['role'],p['kind']);s(p['name']);f(*p['value'])
 u(len(d['nodes']))
 for n in d['nodes']:s(n['path']);u(n['texture'],n['material'],n['columns'],n['rows'],n['frame'])
 struct.pack_into('<8s8I',b,0,b'ENCPRES1',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)
def compile_pack():
 d=load();raw=encode(d,outputs(d));PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw);return raw

def stage_files(source_root):
 d=load();o=outputs(d);raw=encode(d,o);root=Path(source_root);require((root/'data/player.encresources').read_bytes()==raw,'Staged Player resources differ');f={Path('data/player.encresources'):raw}
 for a,v in zip(d['images'],o):
  data=(root/a['path']).read_bytes();require(len(data)==v['bytes']and hashlib.sha256(data).hexdigest()==v['output_sha256'],'Staged Player pixels differ');f[Path(a['path'])]=data
 for a,v in zip(d['audios'],read(RECEIPT)['audios']):
  data=(root/a['path']).read_bytes();require(len(data)==v['bytes']and hashlib.sha256(data).hexdigest()==v['output_sha256'],'Staged Player PCM differs');f[Path(a['path'])]=data
 return f
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--tex3ds',type=Path);p.add_argument('--ffmpeg',type=Path);p.add_argument('--ffprobe',type=Path);a=p.parse_args()
 try:
  if a.action=='extract':extract(a.tex3ds,a.ffmpeg,a.ffprobe)
  elif a.action=='compile':compile_pack()
  else:d=load();require(PACK.read_bytes()==encode(d,outputs(d)),'Player resource binary stale')
  print('Player resources: actual textures, AtlasTexture regions and independent ShaderMaterial bodies')
 except(ValueError,KeyError,TypeError,OSError,subprocess.CalledProcessError,struct.error)as e:sys.exit('PLAYER RESOURCES ERROR: '+str(e))
