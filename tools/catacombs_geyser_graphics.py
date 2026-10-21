"""Compile original untrimmed Geyser Sprite frames into independent resources."""
import argparse,hashlib,io,json,math,shlex,struct,subprocess,sys,zlib
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_geyser as timeline,podunk_field as container
from tools.extract_battle_entry import Extractor,PIN,require
RECIPE=ROOT/'content/catacombs-geyser-graphics.json'
MAGIC=b'ENCGFX01';STRIDES=(1,32,1,32);LIMIT=262144

def read(path=RECIPE):
 p=json.loads(Path(path).read_bytes(),object_pairs_hook=timeline.unique);timeline.fields(p,'schema kind commit timeline_recipe timeline_sha256 output texture scope license_review','Geyser graphics recipe');require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.geyser.reviewed-graphics'and p['commit']==PIN,'Geyser graphics recipe identity');require(p['timeline_recipe']=='content/catacombs-geyser.json'and hashlib.sha256((ROOT/p['timeline_recipe']).read_bytes()).hexdigest()==p['timeline_sha256'],'Geyser graphics timeline review changed')
 for key,suffix,prefix in [('output','.encgeysergfx','data/world/'),('texture','.t3x','graphics/world/')]:timeline.safe(p[key]);require(p[key].startswith(prefix)and p[key].endswith(suffix),'Geyser graphics output path')
 require(all(type(p[k])is str and 0<len(p[k])<=4096 for k in ['scope','license_review']),'Geyser graphics scope/permission');return p

def source(path=RECIPE):
 p=read(path);original,model,proof=timeline.source(ROOT/p['timeline_recipe']);ex=Extractor(ROOT);paths=original['paths'];project=ex.text(paths['project']);imports=ex.text(paths['texture_import'])
 require('\n2d/snapping/use_gpu_pixel_snap=true\n'in project,'Geyser source GPU pixel snap changed')
 for row in ['flags/repeat=0','flags/filter=false','flags/mipmaps=false','process/premult_alpha=false']:require('\n'+row+'\n'in imports,'Geyser source texture import profile changed')
 engine=ROOT/'reports/geyser-timeline/official-engine-source'
 for name in ['scene/2d/sprite.cpp','scene/2d/sprite.h']:
  require(hashlib.sha256((engine/name).read_bytes()).hexdigest()==original['engine_sources'][name],'Geyser audited Sprite source bytes changed')
 require(hashlib.sha256((engine/'LICENSE.txt').read_bytes()).hexdigest()==original['engine_license_sha256'],'Geyser Sprite source license changed')
 with Image.open(io.BytesIO(ex.data(paths['texture'])))as opened:image=opened.convert('RGBA')
 width,height=image.size;fw,fh=model['size'];require(fw==int(fw)and fh==int(fh)and width==model['hframes']*fw and height==model['vframes']*fh,'Geyser original Sprite grid changed');frames=[];pixels=[]
 for index in range(model['frames']):
  x=int(index%model['hframes']*fw);y=int(index//model['hframes']*fh);frames.append(dict(index=index,x=x,y=y,width=int(fw),height=int(fh)));pixels.append(hashlib.sha256(image.crop((x,y,x+int(fw),y+int(fh))).tobytes()).hexdigest())
 result=dict(texture=p['texture'],width=width,height=height,texture_bytes=0,texture_crc=0,flags=1,timeline=timeline.encode(model),frames=frames)
 provenance=dict(commit=PIN,source=proof,recipe_sha256=hashlib.sha256(Path(path).read_bytes()).hexdigest(),sources=dict(sorted(ex.sources.items())),engine_sources={n:original['engine_sources'][n]for n in ['scene/2d/sprite.cpp','scene/2d/sprite.h']},engine_commit=original['engine_commit'],engine_license_sha256=original['engine_license_sha256'],source_frame_rgba_sha256=pixels,source_rgba_sha256=hashlib.sha256(image.tobytes()).hexdigest(),permission=p['license_review'],scope=p['scope'])
 return p,result,image,provenance

def validate(m):
 timeline.fields(m,'texture width height texture_bytes texture_crc flags timeline frames','Geyser graphics model');timeline.safe(m['texture']);require(m['texture'].startswith('graphics/')and m['texture'].endswith('.t3x'),'Geyser graphics texture identity')
 for key in ['width','height']:timeline.integer(m[key],1,1024)
 timeline.integer(m['texture_bytes'],1,8*1024*1024);timeline.integer(m['texture_crc']);timeline.integer(m['flags'],0,7);require(type(m['timeline'])in[bytes,bytearray]and type(m['frames'])is list,'Geyser graphics nested sections');source=timeline.parse(m['timeline']);require(len(m['frames'])==source['frames'],'Geyser graphics frame coverage')
 for index,row in enumerate(m['frames']):
  timeline.fields(row,'index x y width height','Geyser Sprite frame');require(all(type(v)is int for v in row.values())and row['index']==index and 0<=row['x']<m['width']and 0<=row['y']<m['height']and 0<row['width']<=m['width']-row['x']and 0<row['height']<=m['height']-row['y']and row['width']==source['size'][0]and row['height']==source['size'][1],'Geyser graphics frame identity/bounds')
 return m

def encode(m):
 validate(m);pool=m['texture'].encode();policy=struct.pack('<8I',0,len(pool),m['width'],m['height'],m['texture_bytes'],m['texture_crc'],m['flags'],0);frames=b''.join(struct.pack('<8I',*[v[k]for k in ['index','x','y','width','height']],0,0,0)for v in m['frames']);return container.pack_container(MAGIC,0,[pool,policy,m['timeline'],frames],STRIDES,LIMIT)

def parse(blob):
 owner,sections=container.unpack_container(blob,MAGIC,STRIDES,LIMIT,capability=1);pool,policy,source,frames=sections;require(owner==0 and 0<len(pool)<=512 and len(policy)==32 and 160<=len(source)<=timeline.LIMIT and 1<=len(frames)//32<=256,'Geyser graphics section bounds');v=struct.unpack('<8I',policy);require(v[0]==0 and v[1]==len(pool)and v[7]==0,'Geyser graphics text/reserved coverage')
 try:text=pool.decode('ascii')
 except UnicodeError as error:raise ValueError('Geyser graphics ASCII texture path')from error
 rows=[]
 for at in range(0,len(frames),32):
  f=struct.unpack_from('<8I',frames,at);require(not any(f[5:]),'Geyser graphics frame reserved');rows.append(dict(zip(['index','x','y','width','height'],f[:5])))
 return validate(dict(texture=text,width=v[2],height=v[3],texture_bytes=v[4],texture_crc=v[5],flags=v[6],timeline=bytes(source),frames=rows))

def compile(out,tex3ds,command=None):
 out=Path(out).resolve();tool=Path(tex3ds).resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists()and tool.is_file(),'Fresh owned Geyser graphics output/real converter required');p,m,image,proof=source();out.mkdir(parents=True);(out/'.owned-geyser-graphics-candidate').write_bytes(b'geyser-graphics-candidate-v1');png=out/'source-atlas.png';image.save(png);target=out/p['texture'];target.parent.mkdir(parents=True,exist_ok=True);cmd=shlex.split(command)if command else[str(tool)];relative=lambda value:value.resolve().relative_to(ROOT).as_posix();args=[relative(target),relative(png)]if command else[str(target),str(png)];subprocess.run(cmd+['-f','rgba8','-z','none','-o',args[0],args[1]],cwd=ROOT,check=True);raw=target.read_bytes();m['texture_bytes']=len(raw);m['texture_crc']=zlib.crc32(raw);blob=encode(m);require(parse(blob)==m,'Geyser graphics roundtrip');meta=out/p['output'];meta.parent.mkdir(parents=True,exist_ok=True);meta.write_bytes(blob);proof.update(bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),texture_bytes=len(raw),texture_sha256=hashlib.sha256(raw).hexdigest(),tex3ds_sha256=hashlib.sha256(tool.read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),scope='Actual original untrimmed PNG frames/real T3X;no native imported-pixel/alpha-edge/GPU/full scene claim');(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n');print('Geyser graphics candidate:',len(m['frames']),'frames;',len(blob),'metadata,',len(raw),'real T3X bytes;not GPU/full scene');return m

if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);a.add_argument('--tex3ds',type=Path,required=True);a.add_argument('--tex3ds-command');o=a.parse_args();compile(o.out,o.tex3ds,o.tex3ds_command)
