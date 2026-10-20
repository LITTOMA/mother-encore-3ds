"""Checked static sprite/rectangle bindings for the audited source mechanisms.

ENCDMP01 is independent of saves, map tiles and the animation format. Shape
translations remain separate binary32 values, matching native transform order.
Loading this candidate never admits a map, physics world or live renderer.
"""
import argparse,hashlib,json,math,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_mechanism_graphics as graphics,dungeon_mechanism_bindings as bindings
from tools.godot_text import Vec2,node_path
from tools.podunk_field import pack_container,unpack_container
from tools.extract_battle_entry import require,PIN
RECIPE=ROOT/'content/catacombs-mechanism-presentation.json'
MAGIC=b'ENCDMP01';STRIDES=(1,112,40,64,1);LIMIT=512*1024
MEDIA_STRIDES=(*STRIDES,88,1)
ROLES={'body':1,'interaction':2,'pressure':3}

def fields(p,s):require(type(p)is dict and set(p)==set(s.split()),'Mechanism presentation fields')
def f32(v):return struct.unpack('<f',struct.pack('<f',bindings.wall.real(v,-1000000,1000000)))[0]
def vector(v):
 require(type(v)is Vec2,'Mechanism geometry vector type');return [f32(v.x),f32(v.y)]
def flag(v):require(type(v)is bool,'Mechanism geometry flag type');return v
def read(file=RECIPE):
 def unique(rows):
  out={}
  for k,v in rows:require(k not in out,'Duplicate mechanism presentation review field');out[k]=v
  return out
 def invalid(v):raise ValueError('Nonfinite mechanism presentation review')
 p=json.loads(Path(file).read_text(encoding='utf8'),object_pairs_hook=unique,parse_constant=invalid)
 fields(p,'schema kind commit scope graphics_recipe graphics_review_sha256 engine_defaults shape_profiles textures')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.dungeon-mechanism-presentation.reviewed-source'and p['commit']==PIN,'Mechanism presentation schema/pin')
 require(type(p['scope'])is str and 0<len(p['scope'])<=2048 and p['graphics_recipe']=='content/catacombs-mechanism-graphics.json'and p['graphics_review_sha256']==hashlib.sha256(graphics.RECIPE.read_bytes()).hexdigest(),'Mechanism presentation dependency/scope')
 defaults=p['engine_defaults'];fields(defaults,'sprite area body shape tile')
 fields(defaults['sprite'],'centered flip_h flip_v visible offset z_index z_relative');fields(defaults['area'],'layer mask monitoring monitorable');fields(defaults['body'],'layer mask safe_margin');fields(defaults['shape'],'disabled one_way one_way_margin');fields(defaults['tile'],'cell_y_sort')
 require(defaults=={'sprite':{'centered':True,'flip_h':False,'flip_v':False,'visible':True,'offset':['0.00000000000000000','0.00000000000000000'],'z_index':0,'z_relative':True},'area':{'layer':1,'mask':1,'monitoring':True,'monitorable':True},'body':{'layer':1,'mask':1,'safe_margin':0.08},'shape':{'disabled':False,'one_way':False,'one_way_margin':1},'tile':{'cell_y_sort':False}},'Unreviewed source engine constructor defaults')
 for k in('centered','flip_h','flip_v','visible','z_relative'):flag(defaults['sprite'][k])
 require(type(defaults['sprite']['z_index'])is int,'Mechanism sprite constructor z type')
 # Numeric/boolean equality alone is not schema validation.
 for name in('area','body','shape','tile'):
  for k,v in defaults[name].items():
   expected=bool if k in('monitoring','monitorable','disabled','one_way','cell_y_sort')else int if k in('layer','mask','one_way_margin')else float
   require(type(v)is expected,'Mechanism constructor default type')
 require(type(p['shape_profiles'])is list and 0<len(p['shape_profiles'])<=64 and type(p['textures'])is list and len(p['textures'])==len(p['shape_profiles']),'Mechanism presentation review counts')
 profiles=set()
 for r in p['shape_profiles']:
  fields(r,'profile shapes');bindings.integer(r['profile'],1,65535);require(r['profile']not in profiles and type(r['shapes'])is list and 0<len(r['shapes'])<=8,'Mechanism shape profile identity');profiles.add(r['profile']);paths=set()
  for s in r['shapes']:
   fields(s,'path role');bindings.wall.path(s['path']);require(s['path']not in paths and type(s['role'])is str and s['role']in ROLES,'Mechanism shape role/duplicate');paths.add(s['path'])
 seen=set();paths=set()
 for t in p['textures']:
  fields(t,'profile path bytes crc32 sha256');bindings.integer(t['profile'],1,65535);bindings.integer(t['bytes'],1,8*1024*1024);bindings.integer(t['crc32'],0,2**32-1);bindings.wall.path(t['path'])
  require(t['profile']in profiles and t['profile']not in seen and t['path'].endswith('.t3x')and t['path']not in paths and type(t['sha256'])is str and len(t['sha256'])==64 and all(c in'0123456789abcdef'for c in t['sha256'])and int(t['sha256'],16),'Mechanism texture identity');seen.add(t['profile']);paths.add(t['path'])
 return p

def extract(file=RECIPE):
 p=read(file);images,gproof=graphics.build();gmodel,gblob,bproof=bindings.extract();breview=bindings.read();source=bindings.graph.Sources(ROOT);greview,_=bindings.graph.read();nodes={n.path:n for n in bindings.graph.instantiate(source.doc,greview['paths']['scene'])};byprofile={r['id']:r for r in gproof['profiles']};shapes={r['profile']:r['shapes']for r in p['shape_profiles']};textures={r['profile']:r for r in p['textures']};defaults=p['engine_defaults']
 require(set(byprofile)==set(shapes)==set(textures),'Mechanism presentation profile coverage');profiles=[];owners=[];rects=[]
 for ident,row in byprofile.items():
  t=textures[ident];require(t['path']==row['output'],'Mechanism presentation graphics output mismatch')
  profiles.append(dict(id=ident,texture=t['path'],texture_bytes=t['bytes'],texture_crc=t['crc32'],texture_sha256=t['sha256'],source=row['texture'],width=row['atlas_width'],height=row['atlas_height'],cell_width=row['frame_width'],cell_height=row['frame_height'],columns=row['atlas_columns'],rows=row['atlas_rows'],frames=len(row['frames']),position=[f32(v)for v in row['origins']['position']],offset=[f32(v)for v in row['origins']['offset']],bits=sum(int(row['flags'][key])<<i for i,key in enumerate(('centered','visible','flip_h','flip_v','z_as_relative'))),z_index=row['z_index']))
 for index,binding in enumerate(breview['bindings']):
  owner=nodes[binding['path']];profile=byprofile[binding['profile']];parent=nodes[binding['path'].rsplit('/',1)[0]]
  require(parent.type=='TileMap'and not any(k in parent.props for k in('position','scale','rotation','rotation_degrees','transform','z_index','z_as_relative','visible','modulate','self_modulate')),'Unreviewed mechanism parent transform/visibility/z')
  require(not any(k in owner.props for k in('scale','rotation','rotation_degrees','transform','z_index','z_as_relative','visible','modulate','self_modulate','offset','centered','texture','flip_h','flip_v')),'Unreviewed mechanism root presentation override')
  parent_sort=flag(parent.props.get('cell_y_sort',defaults['tile']['cell_y_sort']));position=vector(owner.props.get('position',Vec2(0,0)));owners.append(dict(owner=binding['owner'],profile=binding['profile'],path=binding['path'],parent=parent.path,position=position,bits=int(parent_sort),order=index))
  doc=source.doc(profile['scene']);local={node_path(n.attrs):n for n in doc.nodes};selected={r['path']for r in shapes[binding['profile']]};all_shapes={path for path,n in local.items()if n.attrs.get('type')in('CollisionShape2D','CollisionPolygon2D')};require(selected==all_shapes,'Unreviewed or missing mechanism shape')
  for path,n in local.items():
   if path!='.':require(not nodes[binding['path']+'/'+path].overrides,'Unreviewed mechanism child override')
  for selection in shapes[binding['profile']]:
   path=selection['path'];n=local[path];body=local[path.rsplit('/',1)[0]];kind=body.attrs.get('type');role=selection['role'];require(n.attrs.get('type')=='CollisionShape2D'and kind==('KinematicBody2D'if role=='body'else'Area2D'),'Mechanism shape/body kind')
   require(set(n.props)<=set('position shape disabled one_way_collision one_way_collision_margin'.split())and set(body.props)<=set('position collision_layer collision_mask monitoring monitorable collision/safe_margin'.split()),'Unreviewed mechanism shape/body property')
   shape=doc.sub_resource(n.props.get('shape'),'RectangleShape2D');fields(shape,'extents');extent=vector(shape['extents']);require(min(extent)>0,'Mechanism rectangle extents')
   policy=defaults['body'if role=='body'else'area'];bits=int(flag(n.props.get('disabled',defaults['shape']['disabled'])))|int(flag(n.props.get('one_way_collision',defaults['shape']['one_way'])))<<3
   if role!='body':bits|=int(flag(body.props.get('monitoring',policy['monitoring'])))<<1|int(flag(body.props.get('monitorable',policy['monitorable'])))<<2
   layer=body.props.get('collision_layer',policy['layer']);mask=body.props.get('collision_mask',policy['mask']);bindings.integer(layer,0,0xfffff);bindings.integer(mask,0,0xfffff)
   signal='area_entered'if role=='interaction'else'body_entered';method='_on_hit'if role=='interaction'else'_on_player_entered'
   if role!='body':require({'signal':signal,'from':path.rsplit('/',1)[0],'to':'.','method':method}in doc.connections,'Mechanism source trigger signal binding')
   rects.append(dict(owner=binding['owner'],role=ROLES[role],path=path,body_position=vector(body.props.get('position',Vec2(0,0))),shape_position=vector(n.props.get('position',Vec2(0,0))),extents=extent,layer=layer,mask=mask,bits=bits,safe_margin=f32(body.props.get('collision/safe_margin',policy.get('safe_margin',0))),one_way_margin=f32(n.props.get('one_way_collision_margin',defaults['shape']['one_way_margin']))))
 model=dict(scene=gmodel['scene'],profiles=profiles,owners=owners,shapes=rects,bindings_bytes=gblob)
 blob=encode(model);require(parse(blob)==model,'Mechanism presentation encoded disagreement')
 proof=dict(commit=PIN,scope=p['scope'],review_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),graphics=gproof,bindings_sha256=hashlib.sha256(gblob).hexdigest(),sources=source.digests(),owners=len(owners),shapes=len(rects),bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),whole_scene_approved=False,physics_admission=False,renderer_admission=False)
 return model,blob,proof

def encode(model):
 strings=bytearray()
 def text(v):
  raw=v.encode('ascii');at=len(strings);strings.extend(raw);return at,len(raw)
 profiles=bytearray();owners=bytearray();shapes=bytearray()
 for r in model['profiles']:
  profiles.extend(struct.pack('<7I32s7I4fIi',r['id'],*text(r['texture']),*text(r['source']),r['texture_bytes'],r['texture_crc'],bytes.fromhex(r['texture_sha256']),*[r[k]for k in('width','height','cell_width','cell_height','columns','rows','frames')],*r['position'],*r['offset'],r['bits'],r['z_index']))
 for r in model['owners']:owners.extend(struct.pack('<6I2f2I',r['owner'],r['profile'],*text(r['path']),*text(r['parent']),*r['position'],r['bits'],r['order']))
 for r in model['shapes']:shapes.extend(struct.pack('<4I6f3I2fI',r['owner'],r['role'],*text(r['path']),*r['body_position'],*r['shape_position'],*r['extents'],r['layer'],r['mask'],r['bits'],r['safe_margin'],r['one_way_margin'],0))
 return pack_container(MAGIC,model['scene'],[strings,profiles,owners,shapes,model['bindings_bytes']],STRIDES,LIMIT)

def parse(blob):
 require(isinstance(blob,(bytes,bytearray))and len(blob)>=64,'Mechanism presentation header size');cap=struct.unpack_from('<I',blob,20)[0];require(cap in(1,2),'Mechanism presentation unknown capability')
 scene,parts=unpack_container(blob,MAGIC,MEDIA_STRIDES if cap==2 else STRIDES,LIMIT,capability=cap);nested=bindings.parse(parts[4]);require(nested['scene']==scene,'Mechanism presentation nested scene');require(0<len(parts[0])<=65536 and 0<len(parts[1])<=64*112 and 0<len(parts[2])<=256*40 and 0<len(parts[3])<=1024*64,'Mechanism presentation section count')
 used=bytearray(len(parts[0]))
 def text(at,size):
  require(size and at<=len(used)and size<=len(used)-at and not any(used[at:at+size]),'Mechanism presentation text span/alias');used[at:at+size]=b'\1'*size
  try:value=parts[0][at:at+size].decode('ascii')
  except UnicodeError:raise ValueError('Mechanism presentation non-ASCII path')from None
  bindings.wall.path(value);return value
 profiles=[];ids=set();textures=set()
 for at in range(0,len(parts[1]),112):
  v=struct.unpack_from('<7I32s7I4fIi',parts[1],at);ident=v[0];bindings.integer(ident,1,65535);texture=text(*v[1:3]);source=text(*v[3:5]);require(ident not in ids and texture not in textures and texture.startswith('graphics/')and texture.endswith('.t3x')and source.startswith('Graphics/')and source.endswith('.png')and 0<v[5]<=8*1024*1024 and any(v[7]),'Mechanism presentation profile/texture');ids.add(ident);textures.add(texture)
  width,height,cw,ch,cols,rows,frames=v[8:15];require(0<width<=1024 and 0<height<=1024 and 0<cw<=width and 0<ch<=height and 0<cols<=16 and 0<rows<=65535 and width==cw*cols and height==ch*rows and 0<frames<=cols*rows and (frames+cols-1)//cols==rows and v[19]<=31 and -4096<=v[20]<=4096,'Mechanism sprite grid/flags/z')
  for f in v[15:19]:f32(f)
  profile=next((p for p in nested['profiles']if p['id']==ident),None);require(profile is not None,'Mechanism sprite profile missing')
  clock=profile['wall']['properties']['frames']if profile['family']==4 else struct.unpack_from('<I',unpack_container(profile['bytes'],bindings.prop_clips.MAGIC,bindings.prop_clips.STRIDES,bindings.prop_clips.LIMIT)[1][0])[0];require(frames==clock,'Mechanism sprite frame count/clock')
  profiles.append(dict(id=ident,texture=texture,source=source,texture_bytes=v[5],texture_crc=v[6],texture_sha256=v[7].hex(),width=width,height=height,cell_width=cw,cell_height=ch,columns=cols,rows=rows,frames=frames,position=list(v[15:17]),offset=list(v[17:19]),bits=v[19],z_index=v[20]))
 require(ids=={p['id']for p in nested['profiles']},'Mechanism presentation unused/missing profile');owners=[];seen=set();paths=set();graphowners={r['id']:r for r in nested['graph']['switches']+nested['graph']['objects']}
 require(len(parts[2])//40==len(nested['bindings']),'Mechanism presentation owner coverage')
 for index,at in enumerate(range(0,len(parts[2]),40)):
  ident,profile,pa,ps,la,ls,x,y,bits,order=struct.unpack_from('<6I2f2I',parts[2],at);path=text(pa,ps);parent=text(la,ls);binding=nested['bindings'][index];require(ident==binding['owner']and profile==binding['profile']and ident not in seen and path not in paths and path==graphowners[ident]['path']and parent==path.rsplit('/',1)[0]and bits<=1 and order==index,'Mechanism presentation owner/profile/path/order');seen.add(ident);paths.add(path)
  f32(x);f32(y);expected=graphowners[ident];require((x,y)==(f32(expected['x']),f32(expected['y'])),'Mechanism source root position differs');owners.append(dict(owner=ident,profile=profile,path=path,parent=parent,position=[x,y],bits=bits,order=order))
 shapes=[];seen=set();coverage={r['owner']:[]for r in owners}
 for at in range(0,len(parts[3]),64):
  v=struct.unpack_from('<4I6f3I2fI',parts[3],at);ident,role=v[:2];path=text(*v[2:4]);require(ident in coverage and role in ROLES.values()and(ident,path)not in seen and not v[15]and not(v[10]|v[11])&~0xfffff and v[12]<=15,'Mechanism shape identity/flags/reserved');seen.add((ident,path));coverage[ident].append(role)
  for f in v[4:10]+v[13:15]:f32(f)
  require(min(v[8:10])>0 and 0<=v[13]<=1000 and 0<=v[14]<=1000 and(role!=1 or not v[12]&6)and(role==1 or v[13]==0),'Mechanism shape extents/margins/body flags')
  shapes.append(dict(owner=ident,role=role,path=path,body_position=list(v[4:6]),shape_position=list(v[6:8]),extents=list(v[8:10]),layer=v[10],mask=v[11],bits=v[12],safe_margin=v[13],one_way_margin=v[14]))
 families={p['id']:p['family']for p in nested['profiles']}
 for o in owners:require(sorted(coverage[o['owner']])==([1,2]if families[o['profile']]==1 else[3]if families[o['profile']]==2 else[1]),'Mechanism shape family coverage')
 model=dict(scene=scene,profiles=profiles,owners=owners,shapes=shapes,bindings_bytes=parts[4])
 if cap==2:
  from tools import dungeon_mechanism_media as media
  model.update(media.parse_sounds(parts[5],parts[6],nested,owners,text))
 require(all(used),'Mechanism presentation unowned strings');return model

def build(out,file=RECIPE):
 out=Path(out).resolve();require(out.is_relative_to((ROOT/'build').resolve())and out!=ROOT/'build','Mechanism presentation candidate ownership');m,b,p=extract(file);out.mkdir(parents=True,exist_ok=True);(out/'source.encdmp').write_bytes(b);p['producer_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest();(out/'source.json').write_text(json.dumps(p,indent=2)+'\n',encoding='utf8');print('Mechanism presentation:',len(b),'bytes,',len(m['owners']),'owners,',len(m['shapes']),'rectangles; no scene/physics/renderer admission')

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args();build(a.out)
