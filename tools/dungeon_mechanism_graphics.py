"""Original untrimmed mechanism frames into checked candidate texture atlases.

No painting, resampling, trimming, live draw, shader, contact or scene admission.
"""
import argparse,hashlib,io,json,math,re,shlex,struct,subprocess,sys
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_mechanism_bindings as bindings
from tools.extract_battle_entry import Extractor,PIN,require
from tools.godot_text import Document,Vec2,node_path,parse as parse_godot
from tools.podunk_field import unpack_container
from tools import prop_clips
RECIPE=ROOT/'content/catacombs-mechanism-graphics.json'

def fields(p,names):require(type(p)is dict and set(p)==set(names.split()),'Mechanism graphics fields')
def texture_import(raw,source):
 sections=parse_godot('[gd_resource format=2]\n'+raw.decode('utf8'))[1:]
 require([s.tag for s in sections]==['remap','deps','params']and all(not s.attrs for s in sections),'Mechanism texture importer sections')
 remap,deps,params=[s.props for s in sections]
 fields(remap,'importer type path metadata');fields(deps,'source_file dest_files')
 require(remap['importer']=='texture'and remap['type']=='StreamTexture'and remap['metadata']=={'vram_texture':False}and type(remap['path'])is str and re.fullmatch(r'res://\.import/[^/]+-[0-9a-f]{32}\.stex',remap['path'])and deps=={'source_file':'res://'+source,'dest_files':[remap['path']]},'Mechanism texture importer binding')
 expected={'compress/mode':0,'compress/lossy_quality':0.7,'compress/hdr_mode':0,'compress/bptc_ldr':0,'compress/normal_map':0,'flags/repeat':0,'flags/filter':False,'flags/mipmaps':False,'flags/anisotropic':False,'flags/srgb':2,'process/fix_alpha_border':True,'process/premult_alpha':False,'process/HDR_as_SRGB':False,'process/invert_color':False,'process/normal_map_invert_y':False,'stream':False,'size_limit':0,'detect_3d':False,'svg/scale':1.0}
 require(params==expected and all(type(params[k])is type(v)for k,v in expected.items()),'Unreviewed mechanism texture importer operation')
 return params
def read(file=RECIPE):
 def unique(rows):
  out={}
  for k,v in rows:require(k not in out,'Duplicate mechanism graphics review field');out[k]=v
  return out
 def invalid(v):raise ValueError('Nonfinite mechanism graphics review')
 p=json.loads(Path(file).read_text(encoding='utf8'),object_pairs_hook=unique,parse_constant=invalid);fields(p,'schema kind commit scope bindings_recipe bindings_review_sha256 profiles sources permission')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.dungeon-mechanism-graphics.reviewed-source'and p['commit']==PIN,'Mechanism graphics schema/pin')
 require(all(type(p[k])is str and 0<len(p[k])<=2048 for k in('scope','permission')),'Mechanism graphics scope/permission')
 require(p['bindings_recipe']=='content/catacombs-mechanism-bindings.json'and p['bindings_review_sha256']==hashlib.sha256(bindings.RECIPE.read_bytes()).hexdigest(),'Mechanism graphics binding dependency changed')
 require(type(p['profiles'])is list and 0<len(p['profiles'])<=64 and type(p['sources'])is dict,'Mechanism graphics profile/source types')
 ids=set();names=set();outputs=set();paths={'LICENSE'}
 for row in p['profiles']:
  fields(row,'id name scene sprite texture atlas_columns output');bindings.integer(row['id'],1,65535);bindings.integer(row['atlas_columns'],1,16)
  require(type(row['name'])is str and re.fullmatch('[a-z][a-z0-9_]{0,63}',row['name'])and row['id']not in ids and row['name']not in names,'Mechanism graphics profile identity');ids.add(row['id']);names.add(row['name'])
  for key in('scene','sprite','texture','output'):bindings.wall.path(row[key])
  require(row['output'].startswith('graphics/world/catacombs/mechanisms/')and row['output'].endswith('.t3x')and row['output']not in outputs,'Mechanism graphics output ownership');outputs.add(row['output']);paths|={row['scene'],row['texture'],row['texture']+'.import'}
 require(set(p['sources'])==paths,'Mechanism graphics source coverage');ex=Extractor(ROOT)
 for path,digest in p['sources'].items():require(type(digest)is str and hashlib.sha256(ex.data(path)).hexdigest()==digest,'Mechanism reviewed graphics source changed: '+path)
 require("purpose you're using them is related to the game"in ex.text('LICENSE'),'Mechanism graphics source permission changed')
 for row in p['profiles']:texture_import(ex.data(row['texture']+'.import'),row['texture'])
 return p,ex

def build(file=RECIPE):
 p,ex=read(file);review=bindings.read();data,_,_=bindings.extract();source_profiles={v['id']:v for v in review['profiles']};checked={v['id']:v for v in data['profiles']};require({v['id']for v in p['profiles']}==set(source_profiles),'Mechanism graphics profile coverage')
 property_review,_=bindings.clips.read();property_profiles={v['id']:v for v in property_review['profiles']};wall_review,_=bindings.wall.read()
 images={};rows=[]
 for row in p['profiles']:
  binding=source_profiles[row['id']];require(row['name']==binding['name'],'Mechanism graphics profile family differs')
  if binding['kind']=='wall':expected=(wall_review['paths']['scene'],'main',wall_review['paths']['texture'])
  else:
   original=property_profiles[binding['source_profile']];expected=(original['scene'],original['frame_owner'],original['texture'])
  require((row['scene'],row['sprite'],row['texture'])==expected,'Mechanism graphics reviewed owner/profile differs')
  doc=Document(row['scene'],ex.text(row['scene']));nodes={node_path(n.attrs):n for n in doc.nodes};require(row['sprite']in nodes,'Mechanism sprite owner missing');sprite=nodes[row['sprite']]
  require(sprite.attrs.get('type')=='Sprite'and doc.ext_path(sprite.props.get('texture'),'Texture')==row['texture'],'Mechanism source sprite/texture binding differs')
  require(not(set(sprite.props)-set('texture hframes vframes frame visible position offset centered flip_h flip_v z_index z_as_relative'.split())),'Mechanism source sprite has unreviewed transform/material/region')
  columns=sprite.props.get('hframes',1);lines=sprite.props.get('vframes',1);bindings.integer(columns,1,65535);bindings.integer(lines,1,65535)
  profile=checked[row['id']]
  if profile['family']==4:frame_count=profile['wall']['properties']['frames']
  else:
   _,parts=unpack_container(profile['bytes'],prop_clips.MAGIC,prop_clips.STRIDES,prop_clips.LIMIT);frame_count=struct.unpack_from('<I',parts[0])[0]
  require(columns*lines==frame_count,'Mechanism sprite grid differs from checked clock')
  flags={}
  for name,default in [('centered',True),('visible',True),('flip_h',False),('flip_v',False),('z_as_relative',True)]:
   value=sprite.props.get(name,default);require(type(value)is bool,'Mechanism sprite flag type');flags[name]=value
  origins={}
  for name in('position','offset'):
   value=sprite.props.get(name,Vec2(0,0));require(type(value)is Vec2,'Mechanism sprite origin type');origins[name]=[bindings.wall.real(v,-1000000,1000000)for v in(value.x,value.y)]
  z=sprite.props.get('z_index',0);bindings.integer(z,-4096,4096)
  with Image.open(io.BytesIO(ex.data(row['texture'])))as source:
   source=source.convert('RGBA');require(source.width%columns==source.height%lines==0,'Mechanism source frame grid is not integral');width=source.width//columns;height=source.height//lines
   count=columns*lines;ac=row['atlas_columns'];ar=math.ceil(count/ac);require(0<width*ac<=1024 and 0<height*ar<=1024,'Mechanism atlas exceeds actual texture bounds');atlas=Image.new('RGBA',(width*ac,height*ar));frames=[]
   for index in range(count):
    sx=index%columns*width;sy=index//columns*height;tile=source.crop((sx,sy,sx+width,sy+height));x=index%ac*width;y=index//ac*height;atlas.paste(tile,(x,y));frames.append(dict(source_frame=index,x=x,y=y,width=width,height=height,rgba_sha256=hashlib.sha256(tile.tobytes()).hexdigest()))
   images[row['id']]=atlas;rows.append(dict(**row,source_columns=columns,source_rows=lines,source_sha256=p['sources'][row['texture']],atlas_width=atlas.width,atlas_height=atlas.height,frame_width=width,frame_height=height,atlas_rows=ar,frames=frames,origins=origins,flags=flags,z_index=z,rgba_sha256=hashlib.sha256(atlas.tobytes()).hexdigest()))
 return images,dict(commit=PIN,scope=p['scope'],permission=p['permission'],whole_scene_approved=False,review_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),bindings_review_sha256=p['bindings_review_sha256'],sources=ex.sources,profiles=rows,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())

def compile_assets(out,tex3ds,command=None,file=RECIPE):
 out=Path(out).resolve();tool=Path(tex3ds).resolve();require(out.is_relative_to((ROOT/'build').resolve())and out!=ROOT/'build'and not out.exists()and tool.is_file(),'Fresh owned graphics candidate and actual tex3ds required')
 images,proof=build(file);out.mkdir(parents=True);args=shlex.split(command)if command else[str(tool)];require(args,'Actual tex3ds command required')
 for row in proof['profiles']:
  target=out/row['output'];target.parent.mkdir(parents=True,exist_ok=True);png=target.with_suffix('.png');images[row['id']].save(png)
  paths=[v.relative_to(ROOT).as_posix()for v in(target,png)]if command else[str(target),str(png)]
  with target.with_suffix('.log').open('wb')as log:subprocess.run(args+['-f','rgba8','-z','none','-o',paths[0],paths[1]],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
  row['t3x_bytes']=target.stat().st_size;row['t3x_sha256']=hashlib.sha256(target.read_bytes()).hexdigest()
 proof['tex3ds_sha256']=hashlib.sha256(tool.read_bytes()).hexdigest();(out/'source.json').write_bytes((json.dumps(proof,indent=2)+'\n').encode());print('Actual mechanism graphics:',len(proof['profiles']),'source atlases,',sum(p['t3x_bytes']for p in proof['profiles']),'T3X bytes; no scene/draw/contact admission')
 return proof

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--tex3ds',type=Path,required=True);p.add_argument('--tex3ds-command');p.add_argument('--recipe',type=Path,default=RECIPE);a=p.parse_args();compile_assets(a.out,a.tex3ds,a.tex3ds_command,a.recipe)
