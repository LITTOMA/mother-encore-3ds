"""Prepare source key/door sprites, rectangles and node-owned PCM bindings.

Candidates stay under build. ENCKMP01 is independent of clock and save
versions; it binds the exact clock resource rather than admitting a full map.
"""
import argparse,hashlib,io,json,math,re,shlex,shutil,struct,subprocess,sys,tempfile,zlib
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_key_clocks as clocks,audio_asset as audio
from tools.dungeon_mechanism_graphics import texture_import
from tools.godot_text import Vec2,instantiate,parse as parse_godot
from tools.podunk_field import Sources,pack_container,unpack_container
from tools.extract_battle_entry import PIN,require
RECIPE=ROOT/'content/catacombs-key-media.json';MAGIC=b'ENCKMP01';STRIDES=(1,1,112,64,64,48,1);LIMIT=512*1024
SHAPES={'pickup':1,'body':2,'interaction':3}
def fields(v,n):require(type(v)is dict and set(v)==set(n.split()),'Key media fields: '+n)
def integer(v,a=0,b=0xffffffff):require(type(v)is int and a<=v<=b,'Key media integer');return v
def real(v):return clocks.real(v)
def vector(v):require(type(v)is Vec2,'Key media source Vector2');return[real(v.x),real(v.y)]
def flag(v):require(type(v)is bool,'Key media boolean');return v
def path(v):return clocks.path(v)
def read(file=RECIPE):
 p=json.loads(Path(file).read_text('utf8'),object_pairs_hook=clocks.unique)
 fields(p,'schema kind commit scope clock_recipe clock_review_sha256 graphics shapes audio_recipe sources permission')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.dungeon-key-media.reviewed-source'and p['commit']==PIN,'Key media schema/pin')
 require(type(p['scope'])is str and 0<len(p['scope'])<=2048 and type(p['permission'])is str and 0<len(p['permission'])<=2048,'Key media scope/permission')
 require(p['clock_recipe']=='content/catacombs-key-clocks.json'and p['clock_review_sha256']==hashlib.sha256((ROOT/p['clock_recipe']).read_bytes()).hexdigest(),'Key media clock review changed')
 source=Sources();require(type(p['sources'])is dict and p['sources'],'Key media sources')
 for name,h in p['sources'].items():path(name);require(type(h)is str and len(h)==64 and hashlib.sha256(source.data(name)).hexdigest()==h,'Key media reviewed source changed: '+name)
 require('LICENSE'in p['sources']and "purpose you're using them is related to the game"in source.text('LICENSE'),'Key media permission changed')
 require(type(p['graphics'])is list and 0<len(p['graphics'])<=16 and type(p['shapes'])is list and 0<len(p['shapes'])<=16,'Key media profile rows')
 ids=set();paths=set()
 for r in p['graphics']:
  fields(r,'profile atlas_columns output');integer(r['profile'],1,65535);integer(r['atlas_columns'],1,16);path(r['output']);require(r['profile']not in ids and r['output']not in paths and r['output'].startswith('graphics/world/catacombs/keys/')and r['output'].endswith('.t3x'),'Key media graphics identity');ids.add(r['profile']);paths.add(r['output'])
 for r in p['shapes']:
  fields(r,'profile path role');integer(r['profile'],1,65535);path(r['path']);require(r['role']in SHAPES,'Key media shape role')
 root=json.loads((ROOT/'content/native-audio.json').read_bytes());recipe=p['audio_recipe'];fields(recipe,'schema upstream_commit bus_source bus_sha256 manager_source manager_sha256 assets')
 require(recipe['schema']==1 and type(recipe['schema'])is int and recipe['upstream_commit']==PIN and all(recipe[k]==root[k]for k in['bus_source','bus_sha256','manager_source','manager_sha256']),'Key media root audio bus binding')
 require(type(recipe['assets'])is list and 0<len(recipe['assets'])<=16,'Key media audio assets');ids=set();uris=set()
 for a in recipe['assets']:
  fields(a,'stable_id source_path source_sha256 import_sha256 pcm_path gain_db');integer(a['stable_id'],1);relative=audio.source_path(a['source_path']);path(a['pcm_path']);require(a['stable_id']not in ids and a['source_path']not in uris and a['pcm_path'].startswith('sound/effects/catacombs-keys-')and a['pcm_path'].endswith('.pcm')and type(a['gain_db'])in(int,float)and a['gain_db']==0,'Key media audio identity/output')
  require(relative in p['sources']and relative+'.import'in p['sources']and a['source_sha256']==p['sources'][relative]and a['import_sha256']==p['sources'][relative+'.import'],'Key media audio source binding');loop,offset=audio.import_settings(source.data(relative+'.import'),a['source_path']);require(not loop and offset==0,'Key media source audio loop unsupported');ids.add(a['stable_id']);uris.add(a['source_path'])
  for old in root['assets']:require(old['stable_id']!=a['stable_id']and old['source_path']!=a['source_path'],'Key media identity collides with existing root bank')
 return p,source

def transform(nodes,path):
 chain=['.']+['/'.join(path.split('/')[:i])for i in range(1,len(path.split('/'))+1)]if path!='.'else['.'];pos=[0.,0.];scale=[1.,1.];visible=True;z=0
 for name in chain:
  n=nodes[name];props=n.props;require(not any(k in props for k in['rotation','rotation_degrees','transform','skew','material','modulate','self_modulate','show_behind_parent']),'Key media unreviewed transform/material')
  local=vector(props.get('position',Vec2(0,0)));s=vector(props.get('scale',Vec2(1,1)));require(all(v!=0 for v in s),'Key media singular transform')
  pos=[real(pos[a]+real(local[a]*scale[a]))for a in range(2)];scale=[real(scale[a]*s[a])for a in range(2)];visible&=flag(props.get('visible',True));local_z=integer(props.get('z_index',0),-4096,4096);z=z+local_z if flag(props.get('z_as_relative',True))else local_z
 return pos,scale,visible,z

def source_model(file=RECIPE):
 p,source=read(file);review,_=clocks.read(ROOT/p['clock_recipe']);clock,blob,cproof=clocks.extract(ROOT/p['clock_recipe']);scene=next(v for v in cproof['sources']if v=='Maps/podunk/Catacombs.tscn');nodes={n.path:n for n in instantiate(source.doc,scene)};profiles=[];images={};by={r['id']:r for r in review['profiles']};clock_profiles={r['id']:r for r in clock['profiles']}
 require({r['profile']for r in p['graphics']}==set(by),'Key media graphics profile coverage')
 for r in p['graphics']:
  profile=by[r['profile']];texture=profile['texture'];require(texture in p['sources']and texture+'.import'in p['sources'],'Key media reviewed texture importer');texture_import(source.data(texture+'.import'),texture)
  doc=source.doc(profile['scene']);sprite=next(n for n in doc.nodes if n.attrs.get('parent')=='.'and n.attrs['name']=='Sprite');require(set(sprite.props)<=set('texture hframes vframes position offset visible centered flip_h flip_v z_index z_as_relative frame'.split()),'Key media sprite property unsupported')
  cols=integer(sprite.props.get('hframes',1),1,65535);rows=integer(sprite.props.get('vframes',1),1,65535);count=clock_profiles[r['profile']]['frames'];require(cols*rows==count,'Key media clock frame grid mismatch');off=vector(sprite.props.get('offset',Vec2(0,0)));bits=sum(int(flag(sprite.props.get(k,v)))<<i for i,(k,v)in enumerate([('centered',True),('visible',True),('flip_h',False),('flip_v',False),('z_as_relative',True)]));z=integer(sprite.props.get('z_index',0),-4096,4096)
  with Image.open(io.BytesIO(source.data(texture)))as raw:
   raw=raw.convert('RGBA');require(raw.width%cols==raw.height%rows==0,'Key media nonintegral frames');cw,ch=raw.width//cols,raw.height//rows;ac=r['atlas_columns'];ar=(count+ac-1)//ac;require(0<cw*ac<=1024 and 0<ch*ar<=1024,'Key media atlas domain');image=Image.new('RGBA',(cw*ac,ch*ar));frames=[]
   for frame in range(count):
    tile=raw.crop((frame%cols*cw,frame//cols*ch,(frame%cols+1)*cw,(frame//cols+1)*ch));image.paste(tile,(frame%ac*cw,frame//ac*ch));frames.append(hashlib.sha256(tile.tobytes()).hexdigest())
  images[r['profile']]=image;profiles.append(dict(id=r['profile'],texture=r['output'],source=texture,width=image.width,height=image.height,cell_width=cw,cell_height=ch,columns=ac,rows=ar,frames=count,offset=off,bits=bits,z_index=z,frame_sha256=frames))
 owners=[];shapes=[];sounds=[];selected_shapes={(r['profile'],r['path']):r for r in p['shapes']};used=set();assets={a['source_path']:a for a in p['audio_recipe']['assets']};audio_profiles={a['profile']:a for a in clock['audios']}
 for node in clock['metadata']['nodes']:
  n=nodes[node['path']];parent=n.path.rsplit('/',1)[0];profile=next(v for v in clock['profiles']if v['kind']==node['kind']);pos,scale,visible,z=transform(nodes,n.path);require(scale==[1,1]and nodes[parent].type=='TileMap','Key media owner scale/parent unsupported');_,_,parent_visible,_=transform(nodes,parent);child=[0.,0.];bits=int(flag(nodes[parent].props.get('cell_y_sort',False)))|int(flag(n.props.get('visible',True)))<<1|int(parent_visible)<<2
  if profile['child']:
   c=nodes[n.path+'/Lock'];require(set(c.props)<=set('hframes script anim_name anim_player position'.split())and c.props.get('position')is not None,'Key media child override unsupported');child=vector(c.props['position'])
   script=source.text(by[profile['id']]['script']);match=re.search(r'export var player_turn := (\{.*?\})',script,re.S);require(match is not None,'Key media player turn source');literal=re.sub(r'#[^\n]*','',match[1]);turn=parse_godot('[gd_resource format=2]\n[resource]\nvalue='+literal)[1].props['value'];fields(turn,'x y');bits|=int(flag(turn['y']))<<3|int(flag(turn['x']))<<4
  order=list(nodes).index(n.path);owners.append(dict(owner=node['id'],profile=profile['id'],path=n.path,parent=parent,position=pos,child_position=child,bits=bits,order=order,z_base=z,parent_order=list(nodes).index(parent)))
  all_shapes=[s for s in nodes.values()if s.path.startswith(n.path+'/')and s.type in('CollisionShape2D','CollisionPolygon2D')and'/ButtonPrompt/'not in s.path]
  for shape in all_shapes:
   relative=shape.path[len(n.path)+1:];key=(profile['id'],relative);require(key in selected_shapes and shape.type=='CollisionShape2D','Key media unknown owner shape');used.add(key);selection=selected_shapes[key];body=nodes[shape.path.rsplit('/',1)[0]];role=SHAPES[selection['role']];require(body.type==('StaticBody2D'if role==2 else'Area2D'),'Key media source body class');require(set(shape.props)<=set('position scale shape disabled one_way_collision one_way_collision_margin'.split())and set(body.props)<=set('position collision_layer collision_mask monitoring monitorable'.split()),'Key media unreviewed body/shape property')
   rect=shape.resolve.sub_resource(shape.props['shape'],'RectangleShape2D');fields(rect,'extents');ext=vector(rect['extents']);center,s,_,_=transform(nodes,shape.path);ext=[real(abs(s[a])*ext[a])for a in range(2)];require(min(ext)>0 and shape.props.get('one_way_collision',False)is False,'Key media one-way/empty rectangle unsupported');flags=int(flag(shape.props.get('disabled',False)))
   if role!=2:flags|=int(flag(body.props.get('monitoring',True)))<<1|int(flag(body.props.get('monitorable',True)))<<2
   shapes.append(dict(owner=node['id'],role=role,path=relative,center=center,extents=ext,layer=integer(body.props.get('collision_layer',1),0,0xfffff),mask=integer(body.props.get('collision_mask',1),0,0xfffff),bits=flags,one_way_margin=real(shape.props.get('one_way_collision_margin',1))))
  ap=profile['child']or profile['id'];a=audio_profiles[ap];audio_node=n.path+('/Lock'if profile['child']else'')+'/AudioStreamPlayer';actual=nodes[audio_node];require(actual.type=='AudioStreamPlayer'and set(actual.props)<=set('stream bus volume_db pitch_scale'.split())and actual.resolve.ext_path(actual.props['stream'],'AudioStream')==a['source'][6:],'Key media source sound node');require(a['source']in assets and assets[a['source']]['source_sha256']==a['sha'],'Key media clock/audio asset mismatch')
  connections=[c for c in source.doc(by[ap]['scene']).connections if c.get('from')=='AudioStreamPlayer']
  if connections:require(connections==[{'signal':'finished','from':'AudioStreamPlayer','to':'.','method':'_on_AudioStreamPlayer_finished'}]and node['kind']==1,'Key media unknown audio completion binding')
  sounds.append(dict(owner=node['id'],profile=ap,asset=assets[a['source']]['stable_id'],path=audio_node,gain=a['gain'],pitch=a['pitch'],bus=a['bus'],finish=int(bool(connections))))
 require(used==set(selected_shapes)and set(assets)=={a['source']for a in clock['audios']},'Key media unused shape/audio definition')
 model=dict(scene=clock['scene'],clocks=blob,profiles=profiles,owners=owners,shapes=shapes,sounds=sounds)
 proof=dict(commit=PIN,scope=p['scope'],review_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),clock_sha256=hashlib.sha256(blob).hexdigest(),sources={**cproof['sources'],**source.digests()},whole_scene_approved=False)
 return model,images,proof

def graphics(out,tool,command=None,file=RECIPE):
 out=Path(out).resolve();tool=Path(tool).resolve();require(out.is_relative_to(ROOT/'build')and not out.exists()and tool.is_file(),'Fresh key graphics and actual tex3ds required');model,images,proof=source_model(file);out.mkdir(parents=True);args=shlex.split(command)if command else[str(tool)]
 for p in model['profiles']:
  dest=out/p['texture'];dest.parent.mkdir(parents=True,exist_ok=True);png=dest.with_suffix('.png');images[p['id']].save(png);paths=[v.relative_to(ROOT).as_posix()for v in[dest,png]]if command else[str(dest),str(png)]
  with dest.with_suffix('.log').open('wb')as log:subprocess.run(args+['-f','rgba8','-z','none','-o',*paths],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
  p.update(texture_bytes=dest.stat().st_size,texture_crc=zlib.crc32(dest.read_bytes()),texture_sha256=hashlib.sha256(dest.read_bytes()).hexdigest())
 proof.update(kind='encore.key-media.graphics',profiles=model['profiles'],tex3ds_sha256=hashlib.sha256(tool.read_bytes()).hexdigest());(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Actual key media graphics:',len(model['profiles']),'T3X assets')
def sound_assets(out,file=RECIPE):
 out=Path(out).resolve();require(out.is_relative_to(ROOT/'build')and not out.exists(),'Fresh key audio output required');p,_=read(file);_,_,proof=source_model(file);out.mkdir(parents=True)
 with tempfile.TemporaryDirectory(prefix='decoded-',dir=out)as directory:
  temp=Path(directory);receipt=audio.compile_assets(p['audio_recipe'],ROOT/'upstream/MOTHER-Encore',temp);bank=(temp/'sound/banks/opening.encaudio').read_bytes()
  for asset in receipt['assets']:
   dest=out/asset['pcm_path'];dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(temp/asset['pcm_path'],dest)
  dest=out/'sound/banks/catacombs-keys.encaudio';dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(bank)
 proof.update(kind='encore.key-media.audio',decoder=receipt,bank=audio.parse_bank(bank),bank_path='sound/banks/catacombs-keys.encaudio',bank_sha256=hashlib.sha256(bank).hexdigest());(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Actual key media audio:',len(receipt['assets']),'PCM assets,',sum(a['pcm_bytes']for a in receipt['assets']),'bytes')

def encode(model,bank):
 strings=bytearray();spans={}
 def text(v):
  if v not in spans:spans[v]=(len(strings),len(v.encode('ascii')));strings.extend(v.encode('ascii'))
  return spans[v]
 profiles=bytearray();owners=bytearray();shapes=bytearray();sounds=bytearray()
 for p in model['profiles']:profiles.extend(struct.pack('<7I32s7I2fIi2I',p['id'],*text(p['texture']),*text(p['source']),p['texture_bytes'],p['texture_crc'],bytes.fromhex(p['texture_sha256']),*[p[k]for k in['width','height','cell_width','cell_height','columns','rows','frames']],*p['offset'],p['bits'],p['z_index'],0,0))
 for o in model['owners']:owners.extend(struct.pack('<6I4f2Ii3I',o['owner'],o['profile'],*text(o['path']),*text(o['parent']),*o['position'],*o['child_position'],o['bits'],o['order'],o['z_base'],o['parent_order'],0,0))
 for s in model['shapes']:shapes.extend(struct.pack('<4I4f3If4I',s['owner'],s['role'],*text(s['path']),*s['center'],*s['extents'],s['layer'],s['mask'],s['bits'],s['one_way_margin'],0,0,0,0))
 for s in model['sounds']:sounds.extend(struct.pack('<5I2f5I',s['owner'],s['profile'],s['asset'],*text(s['path']),s['gain'],s['pitch'],s['bus'],s['finish'],0,0,0))
 blob=pack_container(MAGIC,model['scene'],[strings,model['clocks'],profiles,owners,shapes,sounds,bank],STRIDES,LIMIT);parse(blob);return blob

def parse(blob):
 scene,parts=unpack_container(blob,MAGIC,STRIDES,LIMIT);clock=clocks.parse(parts[1]);bank=audio.parse_bank(parts[6]);require(scene==clock['scene'],'Key media nested scene');cursor=64+len(STRIDES)*16
 for i,part in enumerate(parts):at=struct.unpack_from('<I',blob,64+i*16+4)[0];require(at==(cursor+3)//4*4,'Key media canonical section offset');cursor=at+len(part)
 used={};owned=0
 def text(at,n):
  nonlocal owned
  require(0<n<=512 and at<=len(parts[0])and n<=len(parts[0])-at,'Key media text bounds');v=parts[0][at:at+n].decode('ascii');path(v)
  if v in used:require(used[v]==(at,n),'Key media text alias')
  else:require(at==owned,'Key media canonical text order');used[v]=(at,n);owned+=n
  return v
 require(0<len(parts[2])//112<=16 and 0<len(parts[3])//64<=128 and 0<len(parts[4])//64<=384 and 0<len(parts[5])//48<=128,'Key media section counts');cp={p['id']:p for p in clock['profiles']};profiles=[];ids=set();textures=set()
 for at in range(0,len(parts[2]),112):
  r=struct.unpack_from('<7I32s7I2fIi2I',parts[2],at);ident=r[0];w,h,cw,ch,cols,rows,frames=r[8:15];require(ident in cp and ident not in ids and 0<r[5]<=8*1024*1024 and any(r[7])and 0<w<=1024 and 0<h<=1024 and 0<cw<=w and 0<ch<=h and 0<cols<=16 and rows>0 and w==cw*cols and h==ch*rows and frames==cp[ident]['frames']and(rows-1)*cols<frames<=rows*cols and r[17]<=31 and -4096<=r[18]<=4096 and not(r[19]or r[20]),'Key media atlas identity/grid');ids.add(ident);texture=text(*r[1:3]);source=text(*r[3:5]);require(texture.startswith('graphics/')and texture.endswith('.t3x')and texture not in textures and source.startswith('Graphics/')and source.endswith('.png'),'Key media atlas path');textures.add(texture);real(r[15]);real(r[16]);require(bool(r[17]&2)==bool(cp[ident]['mask']&1),'Key media initial sprite visibility mismatch');profiles.append(dict(id=ident,texture=texture,source=source,texture_bytes=r[5],texture_crc=r[6],texture_sha256=r[7].hex(),width=w,height=h,cell_width=cw,cell_height=ch,columns=cols,rows=rows,frames=frames,offset=list(r[15:17]),bits=r[17],z_index=r[18]))
 require(ids==set(cp),'Key media profile coverage');owners=[];ids=set();orders=set();metadata={n['id']:n for n in clock['metadata']['nodes']};require(len(parts[3])//64==len(metadata),'Key media owner count')
 for at in range(0,len(parts[3]),64):
  owner,profile,pa,pn,la,ln,x,y,cx,cy,bits,order,z,parent_order,r1,r2=struct.unpack_from('<6I4f2Ii3I',parts[3],at);require(owner in metadata and owner not in ids and owner==clock['metadata']['nodes'][at//64]['id']and profile in cp and cp[profile]['kind']==metadata[owner]['kind']and bits<=31 and order<=65535 and order not in orders and -4096<=z<=4096 and not(r1 or r2),'Key media owner identity/profile');ids.add(owner);orders.add(order);name=text(pa,pn);parent=text(la,ln);require(name==metadata[owner]['path']and parent==name.rsplit('/',1)[0]and parent_order<order,'Key media owner path/order');[real(v)for v in[x,y,cx,cy]];require(cp[profile]['child']or cx==cy==0,'Key media stray child origin');owners.append(dict(owner=owner,profile=profile,path=name,parent=parent,position=[x,y],child_position=[cx,cy],bits=bits,order=order,z_base=z,parent_order=parent_order))
 require(ids==set(metadata),'Key media owner coverage');shapes=[];seen=set();counts={k:set()for k in ids}
 for at in range(0,len(parts[4]),64):
  owner,role,a,n,x,y,ex,ey,layer,mask,bits,margin,r1,r2,r3,r4=struct.unpack_from('<4I4f3If4I',parts[4],at);require(owner in ids and role in(1,2,3)and(role==1)==(metadata[owner]['kind']==1)and role not in counts[owner]and 0<ex<=1000000 and 0<ey<=1000000 and layer<=0xfffff and mask<=0xfffff and bits<=7 and(role!=2 or not(bits&6))and math.isfinite(margin)and 0<=margin<=1000 and not(r1 or r2 or r3 or r4),'Key media shape role/filter/flags');counts[owner].add(role);name=text(a,n);require((owner,name)not in seen,'Key media duplicate shape');seen.add((owner,name));[real(v)for v in[x,y,ex,ey]];shapes.append(dict(owner=owner,role=role,path=name,center=[x,y],extents=[ex,ey],layer=layer,mask=mask,bits=bits,one_way_margin=margin))
 require(all(v==({1}if metadata[k]['kind']==1 else{2,3})for k,v in counts.items()),'Key media shape coverage');sounds=[];seen=set();assets={a['stable_id']:a for a in bank['assets']};used_assets=set();ca={a['profile']:a for a in clock['audios']};by={o['owner']:o for o in owners}
 for at in range(0,len(parts[5]),48):
  owner,profile,asset,a,n,gain,pitch,bus,finish,r1,r2,r3=struct.unpack_from('<5I2f5I',parts[5],at);require(owner in by and owner not in seen and profile in ca and asset in assets and bus==1 and finish<=1 and not(r1 or r2 or r3),'Key media sound identity');seen.add(owner);o=by[owner];p=cp[o['profile']];require(profile==(p['child']or p['id'])and not(finish and metadata[owner]['kind']!=1),'Key media sound owner/completion mapping');c=ca[profile];b=assets[asset];require(gain==c['gain']and pitch==c['pitch']and b['source_path']==c['source']and b['source_sha256']==c['sha']and not b['loop'],'Key media clock/PCM source mismatch');name=text(a,n);require(name==o['path']+('/Lock'if p['child']else'')+'/AudioStreamPlayer','Key media audio node path');used_assets.add(asset);sounds.append(dict(owner=owner,profile=profile,asset=asset,path=name,gain=gain,pitch=pitch,bus=bus,finish=finish))
 require(seen==ids and used_assets==set(assets)and owned==len(parts[0]),'Key media unowned sound/asset/text');return dict(scene=scene,clocks=parts[1],profiles=profiles,owners=owners,shapes=shapes,sounds=sounds,bank=parts[6])

def validate_native(observed,file=RECIPE):
 """Compare unmodified native Sprite/shape numerics, not a raster golden."""
 m,_,_=source_model(file);cp={p['id']:p for p in clocks.parse(m['clocks'])['profiles']};profiles={p['id']:p for p in m['profiles']};owners={o['owner']:o for o in m['owners']};seen=set()
 require(observed['engine']['string']=='3.6.2-stable (official)','Key media original engine')
 for row in observed['sprites']:
  o=owners[row['owner']];p=profiles[row['profile']];child=row['profile']!=o['profile'];require(row['profile']==(cp[o['profile']]['child']if child else o['profile']),'Native key sprite owner/profile');key=(row['owner'],row['profile'],row['frame']);require(key not in seen and 0<=row['frame']<p['frames'],'Native key frame coverage');seen.add(key)
  local=list(map(float,row['local_position']));global_pos=list(map(float,row['global_position']));expected=[real(o['position'][a]+(o['child_position'][a]if child else 0)+local[a])for a in range(2)];require(global_pos==expected and local==cp[p['id']]['position'],'Native key Sprite position differs')
  bits=p['bits'];size=[p['cell_width'],p['cell_height']];rect=[real(p['offset'][a]-(size[a]*.5 if bits&1 else 0))for a in range(2)]
  require(list(map(float,row['rect']['position']))==rect and list(map(float,row['rect']['size']))==size and row['centered']==bool(bits&1)and row['tree_visible']==bool(bits&2 and o['bits']&2 and o['bits']&4)and list(map(float,row['offset']))==p['offset']and row['flip_h']==bool(bits&4)and row['flip_v']==bool(bits&8)and row['z_relative']==bool(bits&16)and row['z']==p['z_index']and row['parent_type']=='TileMap'and row['parent_sort']==bool(o['bits']&1),'Native key Sprite rectangle/visibility/order differs')
 require(len(seen)==sum(profiles[o['profile']]['frames']+(profiles[cp[o['profile']]['child']]['frames']if cp[o['profile']]['child']else 0)for o in m['owners']),'Native key sprite total differs')
 shapes={(s['owner'],s['path']):s for s in m['shapes']};seen=set()
 for row in observed['shapes']:
  key=row['owner'],row['path'];require(key in shapes and key not in seen,'Native key shape owner');seen.add(key);s=shapes[key];center=list(map(float,row['center']));extent=[real(float(a)*abs(float(b)))for a,b in zip(row['extents'],row['scale'])];corners=[[real(center[a]+extent[a]*sign[a])for a in range(2)]for sign in[(-1,-1),(1,-1),(1,1),(-1,1)]]
  require(center==s['center']and extent==s['extents']and [list(map(float,v))for v in row['corners']]==corners and row['role']==s['role']and row['layer']==s['layer']and row['mask']==s['mask']and row['disabled']==bool(s['bits']&1)and row['monitoring']==bool(s['bits']&2)and row['monitorable']==bool(s['bits']&4)and row['one_way']is False and row['margin']==s['one_way_margin'],'Native key shape bounds/filter differs')
 require(seen==set(shapes),'Native key shape coverage');seen=set();sounds={s['owner']:s for s in m['sounds']}
 for row in observed['sounds']:
  require(row['owner']in sounds and row['owner']not in seen,'Native key sound owner');seen.add(row['owner']);s=sounds[row['owner']];o=owners[s['owner']];require(row['path']==s['path'][len(o['path'])+1:]and row['bus']=='SFX'and float(row['gain'])==s['gain']and float(row['pitch'])==s['pitch']and row['pause']==0 and row['stream_type']in('AudioStreamMP3','AudioStreamSample')and float(row['length'])>0,'Native key sound properties')
 require(seen==set(sounds),'Native key sound coverage')
 review,source=read(file);layout=parse_godot(source.text(review['audio_recipe']['bus_source']))[-1].props;indices={int(k.split('/')[1])for k in layout if k.startswith('bus/')};require(indices==set(range(len(indices)))and type(observed.get('buses'))is list and len(observed['buses'])==len(indices),'Native key original audio bus coverage')
 for index,row in enumerate(observed['buses']):
  prefix='bus/'+str(index)+'/';name=layout.get(prefix+'name','Master'if index==0 else None);send=layout.get(prefix+'send',''if index==0 else'Master');effects=sum(k.startswith(prefix+'effect/')and k.endswith('/effect')for k in layout)
  require(row['name']==name and row['send']==send and float(row['gain'])==real(layout.get(prefix+'volume_db',0))and row['mute']==layout.get(prefix+'mute',False)and row['solo']==layout.get(prefix+'solo',False)and row['bypass']==layout.get(prefix+'bypass_fx',False)and row['effects']==effects,'Native key audio bus source properties differ')
 for o in m['owners']:
  if cp[o['profile']]['child']:require(bool(o['bits']&8)==observed['player_turn']['y']and bool(o['bits']&16)==observed['player_turn']['x'],'Native key turn defaults differ')
 return m

def build(out,graphics_dir,audio_dir,file=RECIPE):
 out=Path(out).resolve();require(out.is_relative_to(ROOT/'build')and not out.exists(),'Fresh key media candidate required');model,_,proof=source_model(file);g=json.loads((Path(graphics_dir)/'source.json').read_text('utf8'));a=json.loads((Path(audio_dir)/'source.json').read_text('utf8'));require(g['review_sha256']==a['review_sha256']==proof['review_sha256']and g['clock_sha256']==a['clock_sha256']==proof['clock_sha256'],'Key media prepared receipt mismatch');atlas={p['id']:p for p in g['profiles']}
 for p in model['profiles']:
  r=atlas[p['id']];require(all(r[k]==p[k]for k in p),'Key media prepared atlas source mismatch');raw=(Path(graphics_dir)/p['texture']).read_bytes();require(len(raw)==r['texture_bytes']and zlib.crc32(raw)==r['texture_crc']and hashlib.sha256(raw).hexdigest()==r['texture_sha256'],'Key media prepared atlas changed');p.update({k:r[k]for k in['texture_bytes','texture_crc','texture_sha256']})
 bank=(Path(audio_dir)/a['bank_path']).read_bytes();require(hashlib.sha256(bank).hexdigest()==a['bank_sha256'],'Key media prepared bank changed');blob=encode(model,bank);out.mkdir(parents=True);(out/'source.enckmp').write_bytes(blob)
 for p in model['profiles']:
  dest=out/p['texture'];dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(Path(graphics_dir)/p['texture'],dest)
 for p in a['decoder']['assets']:
  raw=(Path(audio_dir)/p['pcm_path']).read_bytes();require(hashlib.sha256(raw).hexdigest()==p['pcm_sha256'],'Key media prepared PCM changed');dest=out/p['pcm_path'];dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(raw)
 proof.update(model={k:v for k,v in model.items()if k!='clocks'},graphics=g,audio=a,bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest());(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Prepared key media:',len(blob),'bytes,',len(model['owners']),'owners,',len(model['shapes']),'rectangles; no full map admission')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=['source','graphics','audio','compile']);p.add_argument('--out',type=Path,required=True);p.add_argument('--recipe',type=Path,default=RECIPE);p.add_argument('--tex3ds',type=Path);p.add_argument('--tex3ds-command');p.add_argument('--graphics',type=Path);p.add_argument('--audio',type=Path);a=p.parse_args()
 if a.action=='graphics':graphics(a.out,a.tex3ds,a.tex3ds_command,a.recipe)
 elif a.action=='audio':sound_assets(a.out,a.recipe)
 elif a.action=='compile':build(a.out,a.graphics,a.audio,a.recipe)
 else:
  out=a.out.resolve();require(out.is_relative_to(ROOT/'build')and not out.exists(),'Fresh key source snapshot under build required');model,_,proof=source_model(a.recipe);out.mkdir(parents=True);(out/'source.json').write_text(json.dumps(dict(model={k:v for k,v in model.items()if k!='clocks'},source=proof),indent=2)+'\n',encoding='utf8');print('Key media source:',len(model['owners']),'owners,',len(model['shapes']),'rectangles')
