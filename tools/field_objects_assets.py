"""Checked DroppedItem ready/draw/delete resource producer; no pickup VM."""
import argparse,hashlib,json,math,re,shlex,struct,subprocess,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import podunk_field as f,field_ready_rng,mick_treats,field_area
from tools.godot_text import owner_of,Vec2,Rect2
RECIPE=ROOT/'content/field-objects.json'
PRODUCERS=('tools/field_objects_assets.py','tools/field_ready_rng.py','tools/podunk_field.py','tools/godot_text.py','tools/field_area.py')
def sha(raw):return hashlib.sha256(raw).hexdigest()
def require(v,s):f.require(v,'Field objects '+s)
def unique(pairs):
 out={}
 for k,v in pairs:require(k not in out,'duplicate field');out[k]=v
 return out
def safe(s):return type(s)is str and 0<len(s)<=512 and not any(c in s for c in ('\\',':','\0'))and all(32<=ord(c)<127 for c in s)and all(p not in('','.','..')for p in s.split('/'))
def read(file=RECIPE):
 def nonfinite(_):raise ValueError('Field objects nonfinite JSON')
 p=json.loads(Path(file).read_text(encoding='utf8'),object_pairs_hook=unique,parse_constant=nonfinite)
 require(type(p)is dict and set(p)==set('schema kind commit field_recipe area_recipe object_scene scripts objects atlas_size texture output receipt sources scope'.split()),'recipe fields')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.field-objects.reviewed-source'and p['commit']==f.PIN,'schema/pin')
 require(all(safe(p[k])for k in('field_recipe','area_recipe','object_scene','texture','output','receipt')),'resource path')
 require(p['texture'].startswith('graphics/')and p['texture'].endswith('.t3x')and p['output'].startswith('world/')and p['output'].endswith('.encobjects')and p['receipt'].startswith('content/asset-receipts/')and p['receipt'].endswith('.json'),'resource path kinds')
 require(type(p['scope'])is str and 0<len(p['scope'])<=2048 and '\0'not in p['scope'],'scope')
 require(type(p['scripts'])is dict and set(p['scripts'])=={'object','holder','flag','effect'}and all(safe(v)and v.endswith('.gd')for v in p['scripts'].values()),'script identities')
 require(type(p['sources'])is dict and all(safe(k)and type(v)is str and re.fullmatch('[0-9a-f]{64}',v)for k,v in p['sources'].items())and set(p['scripts'].values())|{p['object_scene'],'LICENSE'}<=set(p['sources']),'reviewed source hashes')
 require(type(p['atlas_size'])is list and len(p['atlas_size'])==2 and all(type(n)is int and 8<=n<=1024 and n&(n-1)==0 for n in p['atlas_size']),'atlas dimensions')
 require(type(p['objects'])is list and 0<len(p['objects'])<=128,'object capacity');ids=set();paths=set()
 for r in p['objects']:
  require(type(r)is dict and set(r)=={'id','path','sprite','effect','body_shape'}and type(r['id'])is int and 0<r['id']<f.NONE and all(safe(r[k])for k in('path','sprite','effect','body_shape'))and r['id']not in ids and r['path']not in paths,'object identity/bindings');ids.add(r['id']);paths.add(r['path'])
 return p
def bindings(p):return {r['path']:r['id']for r in p['objects']}
def source(file=RECIPE):
 from PIL import Image
 p=read(file);recipe=f.load_recipe(ROOT/p['field_recipe']);m=f.Model(recipe,object_bindings=bindings(p));ex=m.sources.extractor
 for name,digest in p['sources'].items():require(sha(ex.data(name))==digest,'reviewed source changed: '+name)
 require(recipe['scene']['source']in p['sources'],'reviewed map is missing');require(ex.data('LICENSE')==(ROOT/'docs/licenses/MOTHER-Encore-0.4.1.0-LICENSE.txt').read_bytes(),'permission identity')
 code={k:ex.text(v)for k,v in p['scripts'].items()};require('yield(get_tree(), "idle_frame")'in code['object']and 'globaldata.flags.get(flag, false)'in code['flag']and 'reset_when_leaving_region = false'in code['holder'],'ready/flag source contract')
 require('"res://Graphics/Objects/Items/%s.png" % item'in code['object'],'live texture source contract')
 def default_bool(which,key):
  match=re.search(r'^export \(bool\) var '+key+r'\s*(?::=|=)\s*(true|false)\s*$',code[which],re.M);require(match,'exported bool '+key);return match[1]=='true'
 _,rng=field_ready_rng.replay(ex,m.scene.nodes,mick_treats.READY_SCRIPTS);images=[];image_keys=[];rows=[]
 def image(texture,rect=None):
  require(texture in p['sources'],'unreviewed image '+texture);raw=ex.data(texture)
  with Image.open(__import__('io').BytesIO(raw))as im:
   im=im.convert('RGBA');r=(0,0,*im.size)if rect is None else rect;require(all(type(n)is int for n in r)and r[0]>=0 and r[1]>=0 and r[2]>0 and r[3]>0 and r[0]+r[2]<=im.width and r[1]+r[3]<=im.height,'image crop bounds');key=(texture,*r)
   if key not in image_keys:image_keys.append(key);images.append(im.crop((r[0],r[1],r[0]+r[2],r[1]+r[3])))
  return image_keys.index(key)
 def point(path):
  x=m.scene.global_xf[path];require((x.xx,x.xy,x.yx,x.yy)==(1,0,0,1),'transformed object child');return [f.f32(x.ox),f.f32(x.oy)]
 def text(span):return m.strings[span[0]:span[0]+span[1]].decode('utf8')
 for binding in p['objects']:
  path=binding['path'];node=m.scene.by_path[path];require(node.scene==p['object_scene']and node.script==p['scripts']['object'],'source object class')
  require(set(node.props)<={'script','show_behind_parent','button_prompt','position','flag','item','is_object_flag','emit_flag_updated_signal','reset_when_leaving_region','reset_when_leaving_area','reset_when_consumed','can_pickup','dialog','dialog_full','dialog_empty','__meta__'},'unmapped object override')
  require(all(path+'/'+binding[k]in m.scene.by_path for k in('sprite','effect','body_shape')),'missing bound child')
  for key,which in [('is_object_flag','flag'),('reset_when_leaving_region','flag'),('reset_when_leaving_area','flag'),('reset_when_consumed','object')]:require(node.props.get(key,default_bool(which,key))is False,'unsupported flag/reset policy '+key)
  flag=node.props.get('flag');require(type(flag)is str and re.fullmatch('[A-Za-z0-9_]+',flag),'global ready flag')
  sprite=m.scene.by_path[path+'/'+binding['sprite']];effect=m.scene.by_path[path+'/'+binding['effect']];require(sprite.type=='Sprite'and effect.type=='AnimatedSprite'and effect.script==p['scripts']['effect'],'visual child classes')
  require(set(sprite.props)<={'texture','position','offset','centered','visible'}and sprite.props.get('visible',True)is True,'main sprite policy')
  require(set(effect.props)<={'frames','animation','frame','playing','script','position','offset','centered','visible'}and effect.props.get('visible',True)is True,'effect sprite policy')
  document=owner_of(effect,'frames');require(document.sub_type(effect.props['frames'])=='SpriteFrames','effect frames type');frames=document.sub_resource(effect.props['frames']);require(set(frames)=={'animations'}and len(frames['animations'])==1,'effect animation coverage');animation=frames['animations'][0]
  require(set(animation)=={'frames','loop','name','speed'}and animation['name']==effect.props.get('animation')and type(animation['loop'])is bool and type(effect.props.get('playing',False))is bool,'effect animation policy')
  fps=animation['speed'];require(type(fps)in(int,float)and math.isfinite(fps)and 0<fps<=240,'effect fps');sequence=[]
  for ref in animation['frames']:
   require(document.sub_type(ref)=='AtlasTexture','effect atlas frame type');atlas=document.sub_resource(ref);require(set(atlas)=={'atlas','region'}and isinstance(atlas['region'],Rect2),'effect atlas fields');region=atlas['region'];rect=[region.x,region.y,region.w,region.h];require(all(int(n)==n for n in rect),'fractional atlas region');sequence.append(image(document.ext_path(atlas['atlas'],'Texture'),tuple(int(n)for n in rect)))
  item=node.props.get('item');require(type(item)is str and re.fullmatch('[A-Za-z0-9_]+',item),'item identity');main=image('Graphics/Objects/Items/'+item+'.png');position=point(path)
  offsets=[]
  for child in(sprite,effect):
   offset=child.props.get('offset',Vec2(0,0));require(isinstance(offset,Vec2)and type(child.props.get('centered',True))is bool,'sprite offset/center');at=point(child.path);offsets.append([f.f32(at[0]-position[0]+offset.x),f.f32(at[1]-position[1]+offset.y)])
  initial=effect.props.get('frame',0);require(type(initial)is int and 0<=initial<len(sequence),'serialized animation frame');sample=rng['samples'].get(effect.path);require(sample and sample['seeded'],'unowned incoming ready RNG');interval=re.search(r'int\(rand_range\(([0-9.]+), ([0-9.]+)\)\)',code['effect']);require(interval,'effect ready frame assignment')
  shape=m.scene.by_path[path+'/'+binding['body_shape']];shape_doc=owner_of(shape,'shape');require(shape.type=='CollisionShape2D'and not shape.props.get('disabled',False)and shape_doc.sub_type(shape.props['shape'])=='RectangleShape2D','static shape policy');extent=shape_doc.sub_resource(shape.props['shape'])['extents'];body_path=shape.path.rsplit('/',1)[0];body_node=m.scene.by_path[body_path];layer=body_node.props.get('collision_layer',1);mask=body_node.props.get('collision_mask',1);body=m.body_by_path.get(body_path,f.NONE);require(m.collides(layer,mask)==(body!=f.NONE),'source collision filtering')
  draw=[i for i,r in enumerate(m.items)if r['kind']==7 and r['first']==binding['id']];notice=[i for i,r in enumerate(m.notices)if text(r['path'])==path];require(len(draw)==len(notice)==1,'map owner join')
  flags=1|2|128|256|int(animation['loop'])*4|int(effect.props.get('playing',False))*8|int(sprite.props.get('centered',True))*16|int(effect.props.get('centered',True))*32|int(node.props.get('show_behind_parent',False))*64
  rows.append(dict(id=binding['id'],flags=flags,main=main,sequence=sequence,initial=initial,resolved=min(int(sample['value']),len(sequence)-1),path=path,flag=flag,position=position,main_offset=offsets[0],effect_offset=offsets[1],fps=fps,range=[float(v)for v in interval.groups()],body=body,draw=draw[0],notice=notice[0],body_center=point(shape.path),body_extents=[extent.x,extent.y],body_layer=layer,body_mask=mask))
 canvas=Image.new('RGBA',tuple(p['atlas_size']),(0,0,0,0));x=y=h=0;rectangles=[]
 for im in images:
  if x+im.width>canvas.width:x=0;y+=h+1;h=0
  require(x+im.width<=canvas.width and y+im.height<=canvas.height,'atlas capacity');canvas.paste(im,(x,y));require(canvas.crop((x,y,x+im.width,y+im.height)).tobytes()==im.tobytes(),'RGBA crop changed');rectangles.append(dict(x=x,y=y,width=im.width,height=im.height));x+=im.width+1;h=max(h,im.height)
 meta=dict(scene=recipe['scene']['id'],width=canvas.width,height=canvas.height,texture=p['texture'],ready_state=rng['state'],player_layer=m.player_layer,player_mask=m.player_mask,objects=rows,frames=rectangles)
 proof=dict(commit=f.PIN,sources=dict(sorted(ex.sources.items())),recipe_sha256=sha(Path(file).read_bytes()),producers={n:sha((ROOT/n).read_bytes())for n in PRODUCERS},rgba_sha256=sha(canvas.tobytes()),images=[list(v)for v in image_keys],permission='Pinned upstream LICENSE game-related port; original notices retained; no tool binary distributed',scope=p['scope'])
 return p,m,canvas,meta,proof
def encode(meta,texture):
 require(type(texture)is bytes and 0<len(texture)<=1024*1024,'encoded texture payload')
 pool=bytearray();strings={}
 def text(s):
  if s not in strings:strings[s]=(len(pool),len(s.encode('utf8')));pool.extend(s.encode('utf8'))
  return strings[s]
 state=meta['ready_state'];policy=struct.pack('<6I',meta['width'],meta['height'],state&0xffffffff,state>>32,*text(meta['texture']))+hashlib.sha256(texture).digest()+struct.pack('<2I',meta['player_layer'],meta['player_mask']);rows=bytearray();sequence=[]
 for r in meta['objects']:
  first=len(sequence);sequence.extend(r['sequence']);rows+=struct.pack('<12I9f3I4I4f2I',r['id'],1,r['flags'],r['main'],first,len(r['sequence']),r['initial'],r['resolved']+1,*text(r['path']),*text(r['flag']),*r['position'],*r['main_offset'],*r['effect_offset'],r['fps'],*r['range'],1,0,0,r['body'],r['draw'],r['notice'],0,*r['body_center'],*r['body_extents'],r['body_layer'],r['body_mask'])
 rectangles=b''.join(struct.pack('<4I',*(r[k]for k in('x','y','width','height')))for r in meta['frames']);indices=struct.pack('<'+'I'*len(sequence),*sequence);blob=bytearray(f.pack_container(b'ENCFOB01',meta['scene'],[bytes(pool),policy,bytes(rows),rectangles,indices],(1,64,136,16,4),1024*1024,capability=3));struct.pack_into('<2I',blob,56,len(texture),zlib.crc32(texture));struct.pack_into('<I',blob,16,zlib.crc32(blob[32:]));return bytes(blob)
def output_record(raw):return dict(bytes=len(raw),sha256=sha(raw))
def checked_receipt(p,meta,proof,root):
 receipt=json.loads((ROOT/p['receipt']).read_text(encoding='utf8'),object_pairs_hook=unique)
 require(type(receipt)is dict and set(receipt)==set(proof)|{'schema','kind','meta','tex3ds_sha256','outputs'}and receipt['schema']==1 and type(receipt['schema'])is int and receipt['kind']=='encore.field-objects.assets'and all(receipt[k]==v for k,v in proof.items())and receipt['meta']==meta,'asset receipt/source differs')
 require(type(receipt['tex3ds_sha256'])is str and re.fullmatch('[0-9a-f]{64}',receipt['tex3ds_sha256']),'converter receipt')
 texture=(Path(root)/p['texture']).read_bytes();blob=encode(meta,texture);require(receipt['outputs']=={p['texture']:output_record(texture),p['output']:output_record(blob)},'asset output receipt differs')
 return texture,blob
def stage_files(root):
 p,m,canvas,meta,proof=source();texture,blob=checked_receipt(p,meta,proof,root);require((Path(root)/p['output']).read_bytes()==blob,'stale object resource');return {Path(p['texture']):texture,Path(p['output']):blob}
def convert(canvas,out,tex3ds,command):
 require(tex3ds is not None and tex3ds.is_file(),'actual tex3ds required');out.mkdir(parents=True,exist_ok=True);png=out/'atlas-00.png';texture=out/'atlas-00.t3x';canvas.save(png);rel=lambda v:v.resolve().relative_to(ROOT).as_posix();args=shlex.split(command)if command else[str(tex3ds)]
 with(out/'conversion.log').open('wb')as log:subprocess.run(args+['-f','rgba8','-z','none','-o',rel(texture),rel(png)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
 return texture.read_bytes()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('action',nargs='?',default='candidate',choices=('candidate','assets','compile','verify'));ap.add_argument('--recipe',type=Path,default=RECIPE);ap.add_argument('--out',type=Path);ap.add_argument('--tex3ds',type=Path);ap.add_argument('--tex3ds-command');a=ap.parse_args();p,m,canvas,meta,proof=source(a.recipe)
 if a.action=='candidate':
  require(a.out is not None,'candidate directory required');out=a.out.resolve();require(out!=ROOT/'build'and(ROOT/'build').resolve()in out.parents and not out.exists(),'fresh owned candidate directory required');raw=convert(canvas,out,a.tex3ds,a.tex3ds_command);blob=encode(meta,raw);(out/'source.encobjects').write_bytes(blob);proof.update(meta=meta,tex3ds_sha256=sha(a.tex3ds.read_bytes()),outputs={n:output_record((out/n).read_bytes())for n in('atlas-00.png','atlas-00.t3x','source.encobjects')});(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8')
 else:
  require(a.recipe.resolve()==RECIPE.resolve()and a.out is None,'release actions require canonical recipe and paths');root=ROOT/'romfs'
  if a.action=='assets':
   raw=convert(canvas,ROOT/'build/field-objects/assets',a.tex3ds,a.tex3ds_command);blob=encode(meta,raw);texture=root/p['texture'];texture.parent.mkdir(parents=True,exist_ok=True);texture.write_bytes(raw)
   receipt=dict(proof,schema=1,kind='encore.field-objects.assets',meta=meta,tex3ds_sha256=sha(a.tex3ds.read_bytes()),outputs={p['texture']:output_record(raw),p['output']:output_record(blob)});dest=ROOT/p['receipt'];dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8')
  else:raw,blob=checked_receipt(p,meta,proof,root)
  target=root/p['output']
  if a.action=='verify':require(target.read_bytes()==blob,'stale encoded resource')
  else:target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(blob)
 print('Source objects:',len(meta['objects']),'rows;',len(blob),'metadata bytes;',len(raw),'actual texture bytes;',a.action)
if __name__=='__main__':main()
