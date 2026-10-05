#!/usr/bin/env python3
"""Reviewed diary special sprites and Present timeline; genuine tex3ds only."""
from __future__ import annotations
import argparse,math,re,struct,subprocess,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require,node
from tools.drawer_program import canonical,digest,fields,read_json,write_json,safe_path
IR='content/basement-actor-assets.json'
REVIEW='reports/basement-actor-assets/source-review.json'
RECEIPT='content/asset-receipts/graphics/story/basement/source.json'
PACK='romfs/data/house.encbasmanim'
FORMATS=[None,'<8I32s','<6If','<fI','<4f','<IIIff'];STRIDES=[1,64,28,8,16,20];HEADER=160
def build(root=ROOT):
 ex=Extractor(root);yaml=ex.yaml('Data/Animations/NintenCutscene.yaml')
 actor=ex.text('Scripts/Main/actor.gd');sprite=ex.text('Scripts/Main/character_sprite.gd');scene=ex.text('Nodes/Reusables/actor.tscn');base=ex.text('Nodes/Reusables/character_sprite.tscn');present=ex.text('Nodes/Overworld/Objects/Present.tscn');ps=ex.text('Scripts/Main/Present.gd');ex.text('LICENSE')
 require(yaml['size']==[20,10] and yaml['offset']==[0,0], 'Diary special grid/offset changed')
 require('frame_index + 1][0]-1)' in sprite and 'Animation.UPDATE_DISCRETE' in sprite and 'anim.length = animationLength' in sprite and 'anim.loop = false' in sprite,'Special frame/time/hold semantics changed')
 require('obj = _special_sprite if type == 1 else character_sprite' in actor and '_set_special_sprite(type == 1)' in actor and '_special_sprite.visible = enabled' in actor,'Special visibility source changed')
 special=node(scene,'SpecialSprite');regular=node(base,'.');require(special['position']==[0,11] and special['_auto_offset'] is False and regular['offset']==[0,-15],'Special inherited transform changed')
 primary=read_json(Path(root)/'compatibility/reviews/ninten-sprite-v0410.json');require(primary['commit']==PIN and primary['texture']=='Graphics/Character Sprites/Ninten/main.png','Special regular sprite lineage changed');ex.data(primary['texture']);require(ex.sources[primary['texture']]==primary['texture_sha256'],'Special regular sprite source changed')
 resources=[]
 for stable,name,path,grid,position,offset in [(1,'ninten-cutscene','Graphics/Character Sprites/Ninten/cutscene.png',yaml['size'],special['position'],regular['offset']),(2,'briefcase','Graphics/Objects/Common/Briefcase.png',[node(present,'Sprite')['hframes'],1],[0,0],[0,0])]:
  size=ex.png_size(path);ex.data(path+'.import');require(all(size[i]%grid[i]==0 for i in (0,1)),'Special source grid size')
  resources.append(dict(id=stable,name=name,source=path,size=size,grid=grid,source_size=size,source_grid=grid,frame_map=[],position=position,offset=offset,output='graphics/story/basement/'+name+'.t3x',primary_output='graphics/actors/ninten-main.t3x' if stable==1 else ''))
 animations=[]
 for stable,name in [(1,'NintenDiarySleep'),(2,'NintenDiaryWakeUp')]:
  a=yaml['animations'][name];require(set(a)=={'directions','type'} and a['type']==1 and len(a['directions'])==1,'Unknown special animation structure');d=a['directions'][0];require(d[0]==0,'Special animation delay changed');time=float(d[0]);keys=[]
  for f,duration in d[1:]:
   require(type(f)is int and 1<=f<=yaml['size'][0]*yaml['size'][1],'Special frame source range');require(type(duration)in(int,float) and math.isfinite(duration) and duration>0,'Special frame duration')
   keys.append(dict(time=time,frame=f-1));time+=duration
  animations.append(dict(id=stable,name=name,resource_id=1,type=1,length=time,keys=keys))
 # Original 1280px strip exceeds the PICA texture extent. Retain only the
 # referenced source cells, repacked losslessly; key frame identities are remapped.
 used=sorted({k['frame']for a in animations for k in a['keys']});r=resources[0];w=r['source_size'][0]//r['source_grid'][0];h=r['source_size'][1]//r['source_grid'][1];columns=min(8,len(used));rows=(len(used)+columns-1)//columns;r['frame_map']=used;r['size']=[w*columns,h*rows];r['grid']=[columns,rows]
 for a in animations:
  for k in a['keys']:k['frame']=used.index(k['frame'])
 block=re.search(r'\[sub_resource type="Animation" id=4\]\n([\s\S]*?)(?=\n\[)',present);require(block is not None,'Present Unwrapped source absent');b=block[1]
 require('tracks/0/path = NodePath("Sprite:frame")' in b and 'tracks/1/path = NodePath("AudioStreamPlayer:playing")' in b and b.count('tracks/0/type')==1 and b.count('tracks/1/type')==1 and '"update": 1' in b,'Present animation tracks changed')
 times=[float(v)for v in re.search(r'"times": PoolRealArray\( ([^)]+) \)',b)[1].split(',')];values=[int(v)for v in re.search(r'"values": \[ ([0-9, ]+) \]',b)[1].split(',')]
 require(times==[0,.0666667,.133333,.2,.266667] and values==[0,1,2,3,4] and '"values": [ true, false ]' in b and '"times": PoolRealArray( 0, 0.333333 )' in b,'Present frame/audio timeline changed')
 require('$AnimationPlayer.play("Unwrapped")' in ps and 'Briefcase.png' in ps,'Present selection changed');sound='Audio/Sound effects/Gift Box.mp3';ex.data(sound);ex.data(sound+'.import')
 animations.append(dict(id=3,name='Unwrapped',resource_id=2,type=1,length=float(re.search(r'^length = ([0-9.]+)',b,re.M)[1]),keys=[dict(time=t,frame=f)for t,f in zip(times,values)]))
 return dict(schema=1,kind='encore.basement-actor-assets.source-ir',commit=PIN,sources=dict(sorted(ex.sources.items())),resources=resources,animations=animations,present_sound=dict(source=sound,resource_id=animations[-1]['resource_id'],animation_id=animations[-1]['id'],start=0,stop=.333333),licence_review='Pinned upstream LICENSE permits game-related forks/modifications; original art remains under upstream terms, not MIT.')
def extract(root=ROOT):
 root=Path(root);ir=build(root);write_json(root/IR,ir);write_json(root/REVIEW,dict(schema=1,commit=PIN,sources=ir['sources'],ir_sha256=digest(root/IR),semantics=['CharacterSprite source frame minus one and cumulative source time','type1 non-looping clips retain final frame; special visibility remains until replaced/restored','Special child position plus inherited Sprite offset; original PNG lossless nearest sampling','Present Unwrapped discrete frames and bounded Gift Box audio track'],unverified=['No tests run','Genuine tex3ds build and rendering integration pending','Emulator/hardware parity pending']));return ir
def load(root=ROOT):
 root=Path(root);ir=read_json(root/IR);require(canonical(ir)==canonical(build(root)),'Unreviewed basement animation IR');r=read_json(root/REVIEW);require(r['schema']==1 and r['commit']==PIN and r['sources']==ir['sources'] and r['ir_sha256']==digest(root/IR),'Basement animation review');return ir
def compile_assets(root,tex3ds):
 from PIL import Image
 root=Path(root);ir=load(root);require(Path(tex3ds).is_file(),'Need genuine tex3ds');outputs={};tmp=root/'build/basement-actor-assets';tmp.mkdir(parents=True,exist_ok=True)
 for r in ir['resources']:
  target=root/'romfs'/r['output'];target.parent.mkdir(parents=True,exist_ok=True);source=root/'upstream/MOTHER-Encore'/r['source']
  if r['frame_map']:
   with Image.open(source)as image:
    require(list(image.size)==r['source_size']and image.format=='PNG','Changed special sprite source image');image=image.convert('RGBA');atlas=Image.new('RGBA',tuple(r['size']));w=image.width//r['source_grid'][0];h=image.height//r['source_grid'][1]
    for i,f in enumerate(r['frame_map']):x=(f%r['source_grid'][0])*w;y=(f//r['source_grid'][0])*h;atlas.paste(image.crop((x,y,x+w,y+h)),((i%r['grid'][0])*w,(i//r['grid'][0])*h))
    source=tmp/(r['name']+'.png');atlas.save(source)
  subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(source)],check=True);outputs[r['output']]=dict(bytes=target.stat().st_size,sha256=digest(target),size=r['size'])
 receipt=dict(schema=1,commit=PIN,ir_sha256=digest(root/IR),producer_sha256=digest(root/'tools/basement_actor_assets.py'),tex3ds_sha256=digest(tex3ds),outputs=outputs);write_json(root/RECEIPT,receipt);compile_pack(root);return receipt
def verify_receipt(root=ROOT):
 root=Path(root);ir=load(root);r=read_json(root/RECEIPT);fields(r,('schema','commit','ir_sha256','producer_sha256','tex3ds_sha256','outputs'),'Basement animation receipt');require(r['schema']==1 and r['commit']==PIN and r['ir_sha256']==digest(root/IR) and r['producer_sha256']==digest(root/'tools/basement_actor_assets.py') and re.fullmatch('[0-9a-f]{64}',r['tex3ds_sha256']),'Basement texture producer');require(set(r['outputs'])=={a['output']for a in ir['resources']},'Basement texture set')
 for a in ir['resources']:
  o=r['outputs'][a['output']];fields(o,('bytes','sha256','size'),'Basement texture output');p=root/'romfs'/a['output'];require(o['size']==a['size'] and o['bytes']>0 and p.stat().st_size==o['bytes'] and digest(p)==o['sha256'],'Changed basement texture')
 return r
def lower(ir,receipt):
 strings=bytearray(b'\0');ids={'':0}
 def string(s):
  if s not in ids:ids[s]=len(strings);strings.extend(s.encode()+b'\0')
  return ids[s]
 resources=[];geometry=[];animations=[];keys=[]
 for r in ir['resources']:
  resources.append([r['id'],string(r['output']),string(r['source']),*r['size'],*r['grid'],string(r['primary_output']),bytes.fromhex(receipt['outputs'][r['output']]['sha256'])]);geometry.append(r['position']+r['offset'])
 for a in ir['animations']:
  animations.append([a['id'],string(a['name']),a['resource_id'],len(keys),len(a['keys']),a['type'],a['length']]);keys.extend([k['time'],k['frame']]for k in a['keys'])
 sound=ir['present_sound'];audio=[[string(sound['source']),sound['resource_id'],sound['animation_id'],sound['start'],sound['stop']]]
 return [bytes(strings),resources,animations,keys,geometry,audio]
def encode(tables):
 out=bytearray(HEADER);directories=[]
 for i,(rows,fmt)in enumerate(zip(tables,FORMATS)):
  while len(out)%4:out.append(0)
  raw=rows if fmt is None else b''.join(struct.pack(fmt,*r)for r in rows);directories.append([i+1,len(out),len(rows),STRIDES[i]]);out.extend(raw)
 struct.pack_into('<8s6I20s12x',out,0,b'ENCBANM1',1,len(out),0,1,1,6,bytes.fromhex(PIN))
 for i,d in enumerate(directories):struct.pack_into('<4I',out,64+i*16,*d)
 struct.pack_into('<I',out,16,zlib.crc32(out)&0xffffffff);return bytes(out)
def compile_pack(root=ROOT):
 root=Path(root);raw=encode(lower(load(root),verify_receipt(root)));parse_pack(raw);(root/PACK).write_bytes(raw);return raw
def parse_pack(raw):
 require(HEADER<=len(raw)<=1024*1024,'Basement animation pack size');h=struct.unpack_from('<8s6I20s12s',raw);require(h[:3]==(b'ENCBANM1',1,len(raw)) and h[4:7]==(1,1,6) and h[7].hex()==PIN and h[8]==bytes(12),'Basement animation header');c=bytearray(raw);c[16:20]=bytes(4);require(zlib.crc32(c)&0xffffffff==h[3],'Basement animation CRC');tables=[];end=HEADER
 for i,fmt in enumerate(FORMATS):
  k,o,n,s=struct.unpack_from('<4I',raw,64+16*i);require(k==i+1 and s==STRIDES[i] and o==(end+3)&~3 and o+n*s<=len(raw) and not any(raw[end:o]),'Basement animation directory');tables.append(raw[o:o+n]if fmt is None else[struct.unpack_from(fmt,raw,o+j*s)for j in range(n)]);end=o+n*s
 require(end==len(raw),'Basement animation trailing bytes');strings,res,anims,keys,geometry,audio=tables;require(strings[:1]==b'\0' and strings[-1:]==b'\0' and len(strings)<=65536 and len(res)==len(geometry)==2 and len(anims)==3 and 0<len(keys)<=1024,'Basement animation counts');starts={0};strings.decode('utf8');starts.update(i+1 for i,b in enumerate(strings)if b==0 and i+1<len(strings))
 def text(i):require(i in starts,'Basement string reference');return strings[i:strings.index(0,i)].decode()
 for r,g in zip(res,geometry):
  require(0<r[0]<0xffffffff and r[3]>0 and r[4]>0 and r[5]>0 and r[6]>0 and r[3]%r[5]==r[4]%r[6]==0 and any(r[8]),'Basement sprite geometry');require((not text(r[7])or text(r[7]).startswith('graphics/actors/')and text(r[7]).endswith('.t3x')) and text(r[1]).startswith('graphics/story/basement/') and text(r[1]).endswith('.t3x') and text(r[2]).startswith('Graphics/') and all(math.isfinite(v)and abs(v)<1e6 for v in g),'Basement sprite path/offset')
 require(len({r[0]for r in res})==len(res) and len({a[0]for a in anims})==len(anims),'Basement duplicate stable ID');owned=0
 for a in anims:
  resource=next((r for r in res if r[0]==a[2]),None);require(a[0]>0 and text(a[1])and resource is not None and a[3]==owned and a[4]>0 and owned+a[4]<=len(keys)and a[5]==1 and math.isfinite(a[6])and 0<a[6]<=3600,'Basement animation span/policy');span=keys[owned:owned+a[4]];require(span[0][0]==0 and all(math.isfinite(k[0])and 0<=k[0]<a[6]and k[1]<resource[5]*resource[6]for k in span)and all(p[0]<q[0]for p,q in zip(span,span[1:])),'Basement animation keys');owned+=a[4]
 require(owned==len(keys),'Orphan basement keys');require(len(audio)==1 and text(audio[0][0]).startswith('Audio/') and any(r[0]==audio[0][1]for r in res)and any(a[0]==audio[0][2]and a[2]==audio[0][1]for a in anims)and all(math.isfinite(v)for v in audio[0][3:])and audio[0][3]==0 and 0<audio[0][4]<=3600,'Basement Present audio track');return tables
def stage_files(source):
 raw=encode(lower(load(),verify_receipt()));parse_pack(raw);source=Path(source);require((source/'data/house.encbasmanim').read_bytes()==raw,'Stale basement animation binary');out={Path('data/house.encbasmanim'):raw}
 for r in load()['resources']:
  p=source/r['output'];b=p.read_bytes();require(digest(p)==verify_receipt()['outputs'][r['output']]['sha256'],'Changed staged basement texture');out[Path(r['output'])]=b
 return out
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','pack','verify']);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
 try:
  if a.action=='extract':extract()
  elif a.action=='compile':compile_assets(ROOT,a.tex3ds)
  elif a.action=='pack':compile_pack()
  else:require((ROOT/PACK).read_bytes()==encode(lower(load(),verify_receipt())),'Stale animation binary');parse_pack((ROOT/PACK).read_bytes())
 except (ValueError,KeyError,TypeError,OSError,struct.error,subprocess.CalledProcessError)as e:print('Basement animation rejected:',e,file=sys.stderr);return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
