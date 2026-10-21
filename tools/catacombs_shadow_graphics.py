"""Source-owned Shadow pixels and Player child geometry;candidate conversion."""
import argparse,hashlib,io,json,math,re,shlex,struct,subprocess,sys,zlib
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_steps as steps,podunk_field as container
from tools.dungeon_keys import fields,path as safe
from tools.dungeon_key_indicator import unique
from tools.godot_text import Document,Vec2
from tools.extract_battle_entry import require,PIN
RECIPE=ROOT/'content/catacombs-shadow-graphics.json'
MAGIC=b'ENCSHDW1';STRIDES=(1,64,32,32);LIMIT=1048576

def read(path=RECIPE):
 def invalid(value):raise ValueError('Nonfinite Shadow review JSON')
 p=json.loads(Path(path).read_bytes(),object_pairs_hook=unique,parse_constant=invalid)
 fields(p,'schema kind commit source_recipe source_recipe_sha256 player_scene node owner_id atlas_columns output texture engine_commit engine_sources engine_license_sha256 scope license_review','Shadow graphics review')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.catacombs-shadow.reviewed-graphics'and p['commit']==PIN,'Shadow graphics review identity')
 for k in['source_recipe','player_scene','output','texture']:safe(p[k])
 require(p['source_recipe']=='content/catacombs-stepping.json'and p['player_scene']=='Nodes/Reusables/Player.tscn'and p['node']=='Shadow'and type(p['owner_id'])is int and 0<p['owner_id']<2**32-1 and p['owner_id']==zlib.crc32((p['player_scene']+':'+p['node']).encode())&0xffffffff,'Shadow source/profile binding')
 require(type(p['atlas_columns'])is int and 1<=p['atlas_columns']<=16 and p['output'].startswith('data/world/')and p['output'].endswith('.encshadow')and p['texture'].startswith('graphics/world/')and p['texture'].endswith('.t3x')and all(type(p[k])is str and 0<len(p[k])<=4096 for k in['scope','license_review']),'Shadow output/packing/scope')
 require(hashlib.sha256((ROOT/p['source_recipe']).read_bytes()).hexdigest()==p['source_recipe_sha256'],'Shadow stepping source review changed')
 require(p['engine_commit']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'and type(p['engine_sources'])is dict and set(p['engine_sources'])=={'scene/2d/animated_sprite.cpp','scene/2d/animated_sprite.h'},'Shadow engine source identity')
 for source,digest in p['engine_sources'].items():
  raw=(ROOT/'reports/catacombs-shadow-graphics/engine'/Path(source).name).read_bytes();require(type(digest)is str and re.fullmatch('[0-9a-f]{64}',digest)and hashlib.sha256(raw).hexdigest()==digest,'Shadow engine source changed:'+source)
 require(hashlib.sha256((ROOT/'docs/licenses/Godot-3.6.2-LICENSE.txt').read_bytes()).hexdigest()==p['engine_license_sha256'],'Shadow engine MIT license changed')
 return p

def build(path=RECIPE):
 p=read(path);base,m,step_proof=steps.source(ROOT/p['source_recipe']);_,ex=steps.read(ROOT/p['source_recipe']);doc=Document(p['player_scene'],ex.text(p['player_scene']));nodes={n.attrs.get('name'):n for n in doc.nodes if n.attrs.get('parent')=='.'};node=nodes.get(p['node'])
 require(node is not None and doc.ext_path(node.attrs['instance'],'PackedScene')==base['shadow_scene']and set(node.props)=={'position'}and type(node.props['position'])is Vec2,'Shadow Player child source geometry changed')
 original=Document(base['shadow_scene'],ex.text(base['shadow_scene']));root=original.nodes[0]
 require(not original.connections,'Shadow source signals are not admitted');text=ex.text('project.godot');snaps=re.findall(r'^2d/snapping/use_gpu_pixel_snap=(true|false)$',text,re.M);require(len(snaps)==1,'Shadow original GPU pixel snap setting')
 # These constructor defaults are checked by the actual native profile probe.
 position=node.props['position'];centered=True;offset=[0,0];flip_h=flip_v=False;visible=True;flags=int(centered)|int(flip_h)*2|int(flip_v)*4|int(visible)*8|int(snaps[0]=='true')*16
 images={};tiles=[];modes=[];maxw=maxh=0
 for index,animation in enumerate(m['shadows']['animations']):
  first=len(tiles)
  for local,frame in enumerate(animation['frames']):
   source=frame['atlas'][6:]
   if source not in images:
    with Image.open(io.BytesIO(ex.data(source)))as image:images[source]=image.convert('RGBA')
   x,y,w,h=map(int,frame['region']);image=images[source];require(0<=x<image.width and 0<=y<image.height and 0<w<=image.width-x and 0<h<=image.height-y,'Shadow atlas frame outside original image');tile=image.crop((x,y,x+w,y+h));maxw=max(maxw,w);maxh=max(maxh,h)
   tiles.append(dict(mode=index,index=local,source=source,source_region=[x,y,w,h],image=tile,width=w,height=h,rgba_sha256=hashlib.sha256(tile.tobytes()).hexdigest()))
  modes.append(dict(name=animation['name'],fps=animation['speed'],first=first,count=len(tiles)-first,front=animation['name']in m['shadows']['front'],loop=animation['loop']))
 require(tiles and len(tiles)<=1024 and maxw*p['atlas_columns']<=1024 and maxh*math.ceil(len(tiles)/p['atlas_columns'])<=1024,'Shadow frame atlas capacity');atlas=Image.new('RGBA',(maxw*p['atlas_columns'],maxh*math.ceil(len(tiles)/p['atlas_columns'])));frames=[]
 for i,tile in enumerate(tiles):
  x=(i%p['atlas_columns'])*maxw;y=(i//p['atlas_columns'])*maxh;atlas.paste(tile['image'],(x,y));frames.append(dict(mode=tile['mode'],index=tile['index'],x=x,y=y,width=tile['width'],height=tile['height']));del tile['image']
 default=next(i for i,mode in enumerate(modes)if mode['name']==m['shadows']['default']);require(root.props['show_behind_parent']==(not modes[default]['front']),'Shadow initial layer differs from mode policy')
 model=dict(owner=p['owner_id'],texture=p['texture'],width=atlas.width,height=atlas.height,texture_bytes=0,texture_crc=0,position=[position.x,position.y],offset=offset,flags=flags,default=default,playing=root.props['playing'],modes=modes,frames=frames)
 proof=dict(commit=PIN,scope=p['scope'],source=step_proof,source_review_sha256=hashlib.sha256(Path(path).read_bytes()).hexdigest(),engine_commit=p['engine_commit'],engine_sources=p['engine_sources'],sources=dict(sorted(ex.sources.items())),packing=dict(columns=p['atlas_columns'],width=atlas.width,height=atlas.height,cell=[maxw,maxh],rgba_sha256=hashlib.sha256(atlas.tobytes()).hexdigest()),source_frames=tiles,permission=p['license_review'],native_defaults_unverified=True,not_implemented=['Formal scene/actor binding','Complete player/lighting/save admission','Actual GPU/NDSP/hardware verification'])
 return p,model,atlas,proof

def validate(m):
 fields(m,'owner texture width height texture_bytes texture_crc position offset flags default playing modes frames','Shadow graphics metadata')
 require(type(m['owner'])is int and 0<m['owner']<2**32-1 and type(m['texture'])is str and len(m['texture'])<=512 and all(32<=ord(c)<127 for c in m['texture'])and safe(m['texture'])and m['texture'].endswith('.t3x'),'Shadow graphics owner/texture path')
 require(all(type(m[k])is int and 1<=m[k]<=1024 for k in['width','height'])and type(m['texture_bytes'])is int and 1<=m['texture_bytes']<=8*1024*1024 and type(m['texture_crc'])is int and 0<=m['texture_crc']<2**32,'Shadow image bounds/identity')
 for key in['position','offset']:require(type(m[key])is list and len(m[key])==2 and all(type(n)in(int,float)and math.isfinite(n)and abs(n)<=1e6 and container.f32(n)==n for n in m[key]),'Shadow local geometry')
 require(type(m['flags'])is int and 0<=m['flags']<=31 and type(m['playing'])is bool and type(m['modes'])is list and 1<=len(m['modes'])<=16 and type(m['frames'])is list and 1<=len(m['frames'])<=1024 and type(m['default'])is int and 0<=m['default']<len(m['modes']),'Shadow mode/flags capacity');names=set();cursor=0
 for index,mode in enumerate(m['modes']):
  fields(mode,'name fps first count front loop','Shadow graphics mode');require(type(mode['name'])is str and re.fullmatch('[a-z][a-z0-9_]{0,31}',mode['name'])and mode['name']not in names and type(mode['fps'])in(int,float)and math.isfinite(mode['fps'])and 0<mode['fps']<=60 and container.f32(mode['fps'])==mode['fps']and 1.0/mode['fps']<=3.4028234663852886e38 and type(mode['first'])is int and mode['first']==cursor and type(mode['count'])is int and 1<=mode['count']<=64 and mode['count']<=len(m['frames'])-cursor and type(mode['front'])is bool and type(mode['loop'])is bool,'Shadow graphics mode/span');names.add(mode['name'])
  for local,frame in enumerate(m['frames'][cursor:cursor+mode['count']]):
   fields(frame,'mode index x y width height','Shadow graphics frame');require(all(type(frame[k])is int for k in frame)and frame['mode']==index and frame['index']==local and 0<=frame['x']<m['width']and 0<=frame['y']<m['height']and 0<frame['width']<=m['width']-frame['x']and 0<frame['height']<=m['height']-frame['y'],'Shadow frame owner/rectangle')
  cursor+=mode['count']
 require(cursor==len(m['frames']),'Orphan Shadow animation frames');return m

def encode(m):
 validate(m);pool=bytearray();cache={}
 def S(value):
  if value not in cache:raw=value.encode();cache[value]=(len(pool),len(raw));pool.extend(raw)
  return cache[value]
 config=struct.pack('<6I4f6I',*S(m['texture']),m['width'],m['height'],m['texture_bytes'],m['texture_crc'],*m['position'],*m['offset'],m['flags'],m['default'],int(m['playing']),0,0,0);modes=bytearray();frames=bytearray()
 for mode in m['modes']:modes.extend(struct.pack('<2If5I',*S(mode['name']),mode['fps'],mode['first'],mode['count'],int(mode['front'])|int(mode['loop'])<<1,0,0))
 for frame in m['frames']:frames.extend(struct.pack('<8I',frame['mode'],frame['index'],frame['x'],frame['y'],frame['width'],frame['height'],0,0))
 return container.pack_container(MAGIC,m['owner'],[pool,config,modes,frames],STRIDES,LIMIT)

def parse(blob):
 owner,sections=container.unpack_container(blob,MAGIC,STRIDES,LIMIT,capability=1);pool,config,modes,frames=sections
 require(0<len(pool)<=32768 and len(config)==64 and 1<=len(modes)//32<=16 and 1<=len(frames)//32<=1024,'Shadow packed capacities');owned=bytearray(len(pool))
 def text(first,count):
  require(count>0 and first<=len(pool)and count<=len(pool)-first,'Shadow packed text reference');owned[first:first+count]=b'\1'*count;return pool[first:first+count].decode('utf8')
 c=struct.unpack('<6I4f6I',config);require(c[12]<=1 and not any(c[13:]),'Shadow packed config flags/reserved');mode_rows=[];frame_rows=[]
 for at in range(0,len(modes),32):
  first,count,fps,start,size,flags,r0,r1=struct.unpack_from('<2If5I',modes,at);require(flags<=3 and not r0 and not r1,'Shadow packed mode flags/reserved');mode_rows.append(dict(name=text(first,count),fps=fps,first=start,count=size,front=bool(flags&1),loop=bool(flags&2)))
 for at in range(0,len(frames),32):
  mode,index,x,y,width,height,r0,r1=struct.unpack_from('<8I',frames,at);require(not r0 and not r1,'Shadow packed frame reserved');frame_rows.append(dict(mode=mode,index=index,x=x,y=y,width=width,height=height))
 model=dict(owner=owner,texture=text(c[0],c[1]),width=c[2],height=c[3],texture_bytes=c[4],texture_crc=c[5],position=list(c[6:8]),offset=list(c[8:10]),flags=c[10],default=c[11],playing=bool(c[12]),modes=mode_rows,frames=frame_rows);require(all(owned),'Shadow unreferenced packed text');return validate(model)

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);a.add_argument('--tex3ds',type=Path,required=True);a.add_argument('--tex3ds-command');o=a.parse_args();out=o.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists()and o.tex3ds.is_file(),'Fresh owned Shadow output under build and actual tex3ds required');p,m,atlas,proof=build();out.mkdir(parents=True);(out/'.owned-shadow-graphics-candidate').write_bytes(b'shadow-graphics-candidate-v1');png=out/'source-atlas.png';atlas.save(png);target=out/p['texture'];target.parent.mkdir(parents=True,exist_ok=True);cmd=shlex.split(o.tex3ds_command)if o.tex3ds_command else[str(o.tex3ds.resolve())];relative=lambda path:path.resolve().relative_to(ROOT).as_posix();args=[relative(target),relative(png)]if o.tex3ds_command else[str(target),str(png)];subprocess.run(cmd+['-f','rgba8','-z','none','-o',args[0],args[1]],cwd=ROOT,check=True);raw=target.read_bytes();m['texture_bytes']=len(raw);m['texture_crc']=zlib.crc32(raw);blob=encode(m);require(parse(blob)==m,'Shadow candidate roundtrip');output=out/p['output'];output.parent.mkdir(parents=True,exist_ok=True);output.write_bytes(blob);proof.update(metadata=m,bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),texture_sha256=hashlib.sha256(raw).hexdigest(),tex3ds_sha256=hashlib.sha256(o.tex3ds.read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),scope='Actual generated original-pixel atlas/metadata;native defaults/clock/GPU/formal scene remain unverified by conversion');(out/'source.json').write_bytes((json.dumps(proof,indent=2)+'\n').encode());print('Shadow candidate:',len(blob),'metadata bytes;',len(raw),'actual T3X bytes;',len(m['frames']),'untrimmed source frames;no scene/GPU pass')
if __name__=='__main__':main()
