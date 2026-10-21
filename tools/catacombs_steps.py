"""Checked Catacombs stepping source model;no scene or renderer admission."""
import argparse,hashlib,json,math,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools.godot_text import Document,node_path,Vec2,Rect2
from tools.dungeon_keys import fields,path as safe
from tools.dungeon_key_indicator import unique
from tools import podunk_field as geometry,audio_asset
from tools.reference_movement import extract
RECIPE=ROOT/'content/catacombs-stepping.json'
MAGIC=b'ENCSTEP1';STRIDES=(1,24,56,72,16,8,32);LIMIT=1048576

def binary_model(m):
 modes=[dict(name=a['name'],fps=geometry.f32(a['speed']),frames=len(a['frames']),front=a['name']in m['shadows']['front'],loop=a['loop'])for a in m['shadows']['animations']]
 require(m['shadows']['default']==m['shadow']and m['shadows']['behind']==(m['shadow']not in m['shadows']['front']),'Source Shadow initial rendering order differs from selected mode')
 areas=[dict(**{k:a[k]for k in['path','id','enter','exit','center','extents','enter_shadow','exit_shadow','layer','mask','enabled']},parts=[part for shape in a['shapes']for part in shape['parts']])for a in m['areas']]
 result=dict(scene=m['scene'],default=m['default'],shadow=m['shadow'],slot=m['slot'],playing=m['shadows']['playing'],sounds=[{k:s[k]for k in['name','id','source','sha256','loop']}for s in m['sounds']],areas=areas,modes=modes);return validate(result)

def convex(points):
 require(type(points)is list and 3<=len(points)<=64 and all(type(v)is list and len(v)==2 and all(type(n)in(int,float)and math.isfinite(n)and abs(n)<=1e6 and geometry.f32(n)==n for n in v)for v in points),'Stepping convex point domain')
 require(len(set(map(tuple,points)))==len(points),'Stepping repeated convex vertex');positive=negative=False;area=0
 for i,p in enumerate(points):
  q=points[(i+1)%len(points)];area+=p[0]*q[1]-q[0]*p[1]
  for r in points:
   cross=(q[0]-p[0])*(r[1]-p[1])-(q[1]-p[1])*(r[0]-p[0]);positive|=cross>0;negative|=cross<0
 require(area!=0 and not(positive and negative),'Stepping nonconvex/degenerate polygon')

def validate(m):
 fields(m,'scene default shadow slot playing sounds areas modes','polygon stepping metadata');identifier=lambda s:type(s)is str and re.fullmatch('[a-z][a-z0-9_]{0,31}',s);ascii_path=lambda s:type(s)is str and all(32<=ord(c)<127 for c in s)and safe(s)
 require(type(m['scene'])is int and 0<m['scene']<2**32 and type(m['playing'])is bool,'Stepping binary scene/state')
 require(type(m['sounds'])is list and 1<=len(m['sounds'])<=16 and type(m['areas'])is list and 1<=len(m['areas'])<=64 and type(m['modes'])is list and 1<=len(m['modes'])<=16,'Stepping binary capacities');names=set();ids=set();paths=set()
 for s in m['sounds']:
  fields(s,'name id source sha256 loop','stepping binary sound');require(identifier(s['name'])and s['name']not in names and type(s['id'])is int and 0<s['id']<2**32 and s['id']not in ids and type(s['source'])is str and s['source'].startswith('res://')and ascii_path(s['source'][6:])and s['source']not in paths and type(s['sha256'])is str and re.fullmatch('[0-9a-f]{64}',s['sha256'])and int(s['sha256'],16)!=0 and s['loop']is True,'Stepping binary sound identity/source');names.add(s['name']);ids.add(s['id']);paths.add(s['source'])
 modes=set()
 for mode in m['modes']:
  fields(mode,'name fps frames front loop','stepping Shadow mode');require(identifier(mode['name'])and mode['name']not in modes and type(mode['fps'])in(int,float)and math.isfinite(mode['fps'])and 0<mode['fps']<=60 and geometry.f32(mode['fps'])==mode['fps']and type(mode['frames'])is int and 1<=mode['frames']<=64 and type(mode['front'])is bool and type(mode['loop'])is bool,'Stepping Shadow mode values');modes.add(mode['name'])
 require(type(m['default'])is str and m['default']in names and type(m['shadow'])is str and m['shadow']in modes and identifier(m['slot']),'Stepping initial binary policy');paths=set();ids=set();part_count=point_count=0
 for a in m['areas']:
  fields(a,'path id enter exit center extents enter_shadow exit_shadow layer mask enabled parts','polygon stepping area');require(ascii_path(a['path'])and a['path']not in paths and type(a['id'])is int and 0<a['id']<geometry.NONE and a['id']not in ids and type(a['enter'])is str and a['enter']in names and type(a['exit'])is str and(not a['exit']or a['exit']in names),'Stepping area binary identity/sound');paths.add(a['path']);ids.add(a['id'])
  require(all(type(a[k])is str and(not a[k]or a[k]in modes)for k in['enter_shadow','exit_shadow'])and all(type(a[k])is int and 0<a[k]<2**32 for k in['layer','mask'])and type(a['enabled'])is bool,'Stepping binary area Shadow/filter')
  require(type(a['parts'])is list and 1<=len(a['parts'])<=64,'Stepping area convex parts');flat=[]
  for part in a['parts']:convex(part);flat.extend(part);part_count+=1;point_count+=len(part)
  minimum=[min(p[k]for p in flat)for k in[0,1]];maximum=[max(p[k]for p in flat)for k in[0,1]]
  for key,value in [('center',[geometry.f32(geometry.f32(minimum[k]+maximum[k])*.5)for k in[0,1]]),('extents',[geometry.f32(geometry.f32(maximum[k]-minimum[k])*.5)for k in[0,1]])]:require(type(a[key])is list and len(a[key])==2 and all(type(v)in(int,float)and math.isfinite(v)and abs(v)<=1e6 for v in a[key])and a[key]==value and(key!='extents'or all(v>0 for v in value)),'Stepping convex bounds mismatch')
 require(part_count<=4096 and point_count<=16384,'Stepping aggregate polygon capacity');return m

def encode(m):
 validate(m);pool=bytearray();cache={}
 def S(s):
  if not s:return 0,0
  if s not in cache:raw=s.encode();cache[s]=(len(pool),len(raw));pool.extend(raw)
  return cache[s]
 names={s['name']:i for i,s in enumerate(m['sounds'])};policy=struct.pack('<6I',names[m['default']],*S(m['shadow']),*S(m['slot']),int(m['playing']));sounds=bytearray();areas=bytearray();parts=bytearray();points=bytearray();modes=bytearray()
 for s in m['sounds']:sounds.extend(struct.pack('<4I32s2I',*S(s['name']),*S(s['source']),bytes.fromhex(s['sha256']),s['id'],1))
 for i,a in enumerate(m['areas']):
  first=len(parts)//16
  for part in a['parts']:
   parts.extend(struct.pack('<4I',i,len(points)//8,len(part),0))
   for point in part:points.extend(struct.pack('<2f',*point))
  areas.extend(struct.pack('<4I4f10I',*S(a['path']),names[a['enter']],names[a['exit']]if a['exit']else geometry.NONE,*a['center'],*a['extents'],*S(a['enter_shadow']),*S(a['exit_shadow']),a['layer'],a['mask'],int(a['enabled']),a['id'],first,len(a['parts'])))
 for mode in m['modes']:modes.extend(struct.pack('<2If5I',*S(mode['name']),mode['fps'],mode['frames'],int(mode['front']),int(mode['loop']),0,0))
 return geometry.pack_container(MAGIC,m['scene'],[pool,policy,sounds,areas,parts,points,modes],STRIDES,LIMIT,capability=2)

def parse(blob):
 scene,sections=geometry.unpack_container(blob,MAGIC,STRIDES,LIMIT,capability=2);pool,policy,sounds,areas,parts,points,modes=sections
 require(0<len(pool)<=32768 and len(policy)==24 and 1<=len(sounds)//56<=16 and 1<=len(areas)//72<=64 and 1<=len(parts)//16<=4096 and 1<=len(points)//8<=16384 and 1<=len(modes)//32<=16,'Stepping packed capacity')
 def text(off,length):
  require(type(off)is int and off<=len(pool)and length<=len(pool)-off and(length>0 or off==0),'Stepping packed text bounds');return pool[off:off+length].decode('utf8')
 default,so,sl,ko,kl,playing=struct.unpack('<6I',policy);require(playing<=1,'Stepping packed initial state');sound_rows=[];mode_rows=[]
 for at in range(0,len(sounds),56):
  no,nl,po,pl,digest,identity,flags=struct.unpack_from('<4I32s2I',sounds,at);require(flags==1,'Stepping packed sound flags');sound_rows.append(dict(name=text(no,nl),source=text(po,pl),sha256=digest.hex(),id=identity,loop=True))
 for at in range(0,len(modes),32):
  no,nl,fps,frames,front,loop,r1,r2=struct.unpack_from('<2If5I',modes,at);require(front<=1 and loop<=1 and not r1 and not r2,'Stepping packed Shadow flags');mode_rows.append(dict(name=text(no,nl),fps=fps,frames=frames,front=bool(front),loop=bool(loop)))
 require(default<len(sound_rows),'Stepping packed initial sound');area_rows=[];part_cursor=point_cursor=0
 for at in range(0,len(areas),72):
  v=struct.unpack_from('<4I4f10I',areas,at);require(v[2]<len(sound_rows)and(v[3]==geometry.NONE or v[3]<len(sound_rows))and v[14]<=1 and v[16]==part_cursor and 1<=v[17]<=64 and v[17]<=len(parts)//16-part_cursor,'Stepping packed area bindings/span');polygons=[]
  for i in range(v[17]):
   owner,first,count,reserved=struct.unpack_from('<4I',parts,part_cursor*16);require(owner==len(area_rows)and first==point_cursor and 3<=count<=64 and count<=len(points)//8-first and not reserved,'Stepping packed convex ownership/span');polygons.append([list(struct.unpack_from('<2f',points,(first+j)*8))for j in range(count)]);part_cursor+=1;point_cursor+=count
  area_rows.append(dict(path=text(*v[:2]),id=v[15],enter=sound_rows[v[2]]['name'],exit=''if v[3]==geometry.NONE else sound_rows[v[3]]['name'],center=list(v[4:6]),extents=list(v[6:8]),enter_shadow=text(*v[8:10]),exit_shadow=text(*v[10:12]),layer=v[12],mask=v[13],enabled=bool(v[14]),parts=polygons))
 require(part_cursor==len(parts)//16 and point_cursor==len(points)//8,'Stepping orphaned convex records');return validate(dict(scene=scene,default=sound_rows[default]['name'],shadow=text(so,sl),slot=text(ko,kl),playing=bool(playing),sounds=sound_rows,areas=area_rows,modes=mode_rows))

def shadow_source(p,ex):
 doc=Document(p['shadow_scene'],ex.text(p['shadow_scene']));require(len(doc.nodes)==1,'Shadow unexpected child nodes');root=doc.nodes[0]
 require(root.attrs.get('type')=='AnimatedSprite'and set(root.props)=={'show_behind_parent','frames','animation','playing','script','start_anim'}and doc.ext_path(root.props['script'],'Script')==p['shadow_script']and root.props['animation']==root.props['start_anim']and type(root.props['show_behind_parent'])is bool and type(root.props['playing'])is bool,'Shadow root schema/initial state')
 frames=root.props['frames'];require(doc.sub_type(frames)=='SpriteFrames'and set(doc.sub_resource(frames))=={'animations'},'Shadow animation resource schema');rows=[];names=set()
 for animation in doc.sub_resource(frames)['animations']:
  fields(animation,'frames loop name speed','Shadow animation');require(type(animation['name'])is str and animation['name']not in names and type(animation['loop'])is bool and type(animation['speed'])in(int,float)and math.isfinite(animation['speed'])and 0<animation['speed']<=60 and type(animation['frames'])is list and 1<=len(animation['frames'])<=64,'Shadow animation values');names.add(animation['name']);images=[]
  for ref in animation['frames']:
   require(doc.sub_type(ref)=='AtlasTexture','Shadow frame texture type');value=doc.sub_resource(ref);fields(value,'atlas region','Shadow atlas frame');region=value['region'];name=doc.ext_path(value['atlas'],'Texture');require(type(region)is Rect2 and all(math.isfinite(v)and 0<=v<=4096 and int(v)==v for v in[region.x,region.y,region.w,region.h])and region.w>0 and region.h>0 and {name,name+'.import'}<=set(p['sources']),'Shadow frame geometry/source coverage');ex.data(name);ex.data(name+'.import');images.append(dict(atlas='res://'+name,region=[region.x,region.y,region.w,region.h]))
  rows.append(dict(name=animation['name'],speed=animation['speed'],loop=animation['loop'],frames=images))
 require(root.props['animation']in names,'Shadow initial animation missing');text=ex.text(p['shadow_script']);front=re.search(r'^var in_front_anims = (\[[^\n]+\])$',text,re.M);require(front,'Shadow front ordering declaration');front=json.loads(front[1]);require(type(front)is list and len(front)==len(set(front))and all(type(v)is str and v in names for v in front),'Shadow front animation values')
 return dict(default=root.props['animation'],behind=root.props['show_behind_parent'],playing=root.props['playing'],front=front,animations=rows)
def read(file=RECIPE):
 def invalid(v):raise ValueError('Nonfinite stepping source JSON')
 p=json.loads(Path(file).read_text('utf-8'),object_pairs_hook=unique,parse_constant=invalid)
 fields(p,'schema kind commit scope scene scene_id area_scene area_script player_script party_script shadow_scene shadow_script areas sounds sources license_review','Catacombs stepping review')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.catacombs-stepping.reviewed-source'and p['commit']==PIN and type(p['scene_id'])is int and 0<p['scene_id']<2**32,'Stepping review identity')
 for key in ['scene','area_scene','area_script','player_script','party_script','shadow_scene','shadow_script']:safe(p[key])
 require(all(type(p[k])is str and 0<len(p[k])<=4096 for k in ['scope','license_review']),'Stepping scope/license')
 require(type(p['areas'])is list and 1<=len(p['areas'])<=64 and type(p['sounds'])is list and 1<=len(p['sounds'])<=16,'Stepping source capacities');ids=set();paths=set()
 for area in p['areas']:
  fields(area,'path id','stepping area');safe(area['path']);require(type(area['id'])is int and 0<area['id']<2**32 and area['id']not in ids and area['path']not in paths and area['id']==zlib.crc32((p['scene']+':'+area['path']).encode())&0xffffffff,'Stepping stable area identity');ids.add(area['id']);paths.add(area['path'])
 ids=set();names=set();outputs=set()
 for s in p['sounds']:
  fields(s,'name id pcm','stepping sound');safe(s['pcm']);require(type(s['name'])is str and re.fullmatch('[a-z][a-z0-9_]{0,31}',s['name'])and s['name']not in names and type(s['id'])is int and 0<s['id']<2**32 and s['id']not in ids and s['pcm'].startswith('sound/effects/')and s['pcm'].endswith('.pcm')and s['pcm']not in outputs,'Stepping sound identity');ids.add(s['id']);names.add(s['name']);outputs.add(s['pcm'])
 require(type(p['sources'])is dict and {p[k]for k in ['scene','area_scene','area_script','player_script','party_script','shadow_scene','shadow_script']}|{'project.godot','LICENSE'}<=set(p['sources']),'Stepping source coverage');ex=Extractor(ROOT)
 for name,digest in p['sources'].items():safe(name);require(type(digest)is str and re.fullmatch('[0-9a-f]{64}',digest)and hashlib.sha256(ex.data(name)).hexdigest()==digest,'Stepping source changed:'+name)
 return p,ex
def source(file=RECIPE):
 p,ex=read(file);doc=Document(p['scene'],ex.text(p['scene']));nodes={node_path(n.attrs):n for n in doc.nodes};root=nodes['.'];parent=nodes.get('Stepping Sounds')
 require(root.attrs.get('type')=='Node2D'and set(root.props)=={'script','_region_name'}and parent is not None and parent.attrs.get('type')=='Node2D'and parent.props=={},'Stepping canvas ancestry changed')
 declared=[n for n in doc.nodes if n.attrs.get('parent')=='Stepping Sounds'];require([node_path(n.attrs)for n in declared]==[v['path']for v in p['areas']],'Stepping original area coverage/order')
 base=Document(p['area_scene'],ex.text(p['area_scene']));area_root=base.nodes[0];require(area_root.attrs.get('type')=='Area2D'and set(area_root.props)=={'collision_layer','monitorable','script'}and base.ext_path(area_root.props['script'],'Script')==p['area_script'],'Stepping source base class/filter')
 script=ex.text(p['area_script']);defaults={};choices={}
 for key in ['entering_sound','exiting_sound','enter_shadow_effect','exit_shadow_effect']:
  m=re.search(r'^export \((String,[^\n]+)\) var '+key+r' = ("[^"]*")$',script,re.M);require(m,'Stepping source export:'+key);defaults[key]=json.loads(m[2]);choices[key]=[json.loads(v)for v in re.findall(r'"[^"]*"',m[1])]
 require(choices['entering_sound']==[s['name']for s in p['sounds']]and 'var enabled = true'in script,'Stepping complete inherited sound choices/state changed')
 player=ex.data(p['player_script']);player_text=player.decode('utf-8');run=re.search(r'^var run_sound := "([^"]+)"$',player_text,re.M);shadow=re.search(r'^\tset_shadow\("([^"]+)"\)$',player_text,re.M);slot=re.search(r'audioManager.add_sfx\(sfx, "([^"]+)"\)',player_text);require(run and shadow and slot,'Stepping player initial sound/shadow/slot')
 sounds=[]
 for row in p['sounds']:
  path='Audio/Sound effects/Footsteps/'+row['name']+'.mp3';require({path,path+'.import'}<=set(p['sources']),'Stepping unreviewed media dependency');raw=ex.data(path);imp=ex.data(path+'.import');loop,start=audio_asset.import_settings(imp,'res://'+path);require(loop and start==0,'Stepping source requires full builtin loop')
  sounds.append(dict(name=row['name'],id=row['id'],pcm=row['pcm'],source='res://'+path,sha256=hashlib.sha256(raw).hexdigest(),import_sha256=hashlib.sha256(imp).hexdigest(),loop=True))
 require(run[1]in choices['entering_sound']and shadow[1]in choices['enter_shadow_effect'],'Stepping initial state outside source choices');areas=[]
 for binding,node in zip(p['areas'],declared):
  path=binding['path'];require(doc.ext_path(node.attrs['instance'],'PackedScene')==p['area_scene']and set(node.props)<=set(defaults)|{'position'},'Stepping source instance/override changed');values={k:node.props.get(k,v)for k,v in defaults.items()}
  for key,value in values.items():require(type(value)is str and value in choices[key],'Stepping unreviewed sound/shadow selection')
  position=node.props.get('position',Vec2(0,0));require(isinstance(position,Vec2),'Stepping source position');shapes=[]
  for child in doc.nodes:
   if child.attrs.get('parent')!=path:continue
   child_path=node_path(child.attrs);kind=child.attrs.get('type');offset=child.props.get('position',Vec2(0,0));require(isinstance(offset,Vec2),'Stepping shape position')
   if kind=='CollisionPolygon2D':
    require(set(child.props)<={'position','polygon'}and 'polygon'in child.props,'Stepping polygon override outside scope');points=[[v.x,v.y]for v in child.props['polygon']]
   elif kind=='CollisionShape2D':
    require(set(child.props)<={'position','shape'}and doc.sub_type(child.props['shape'])=='RectangleShape2D'and set(doc.sub_resource(child.props['shape']))=={'extents'},'Stepping rectangle override outside scope');e=doc.sub_resource(child.props['shape'])['extents'];require(isinstance(e,Vec2)and e.x>0 and e.y>0,'Stepping rectangle extents');points=[[-e.x,-e.y],[e.x,-e.y],[e.x,e.y],[-e.x,e.y]]
   else:raise ValueError('Unsupported stepping child:'+child_path)
   points=[[geometry.f32(v[0]+position.x+offset.x),geometry.f32(v[1]+position.y+offset.y)]for v in points];require(3<=len(points)<=256 and all(math.isfinite(x)and abs(x)<=1e6 for v in points for x in v),'Stepping polygon capacity/coordinate');parts=geometry.decompose_polygon_in_convex(points);require(parts and all(3<=len(part)<=64 for part in parts),'Stepping convex part capacity')
   shapes.append(dict(path=child_path,type=kind,points=points,parts=[[[x,y]for x,y in part]for part in parts]))
  require(shapes,'Stepping area has no source shapes');flat=[v for s in shapes for part in s['parts']for v in part];minimum=[min(v[k]for v in flat)for k in [0,1]];maximum=[max(v[k]for v in flat)for k in [0,1]]
  areas.append(dict(id=binding['id'],path=path,enter=values['entering_sound'],exit=values['exiting_sound'],enter_shadow=values['enter_shadow_effect'],exit_shadow=values['exit_shadow_effect'],layer=area_root.props['collision_layer'],mask=1,enabled=True,center=[geometry.f32(geometry.f32(minimum[k]+maximum[k])*.5)for k in [0,1]],extents=[geometry.f32(geometry.f32(maximum[k]-minimum[k])*.5)for k in [0,1]],shapes=shapes))
 party=ex.data(p['party_script']);area=ex.data(p['area_script']);shadow_raw=ex.data(p['shadow_script']);methods={}
 require(player.startswith(b'class_name PartyMemberPlayer\nextends PartyObject\n')and party.startswith(b'class_name PartyObject\nextends KinematicBody2D\n'),'Stepping original body class ancestry changed')
 for label,raw,names in [('player',player,['_set_running','_move_state']),('area',area,['_on_Stepping_Sounds_body_entered','_on_Stepping_Sounds_body_exited','set_player_run_sound','enable','disable']),('party',party,['set_shadow','get_shadow']),('shadow',shadow_raw,['set_anim','_set_behind_parent'])]:methods[label]=extract(raw,hashlib.sha256(raw).hexdigest(),names)[1]
 require("purpose you're using them is related to the game"in ex.text('LICENSE')and set(ex.sources)<=set(p['sources']),'Stepping unreviewed source/license')
 metadata=dict(scene=p['scene_id'],default=run[1],shadow=shadow[1],slot=slot[1],sounds=sounds,areas=areas,shadows=shadow_source(p,ex))
 proof=dict(commit=PIN,scope=p['scope'],review_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),sources=dict(sorted(ex.sources.items())),methods=methods,source_rules={'sound':'current player AND enabled;exit also requires nonempty sound','shadow':'ANY PartyObject and nonempty effect,independent of enabled','callbacks':'Assignments only;area change does not directly restart running audio'},not_implemented=['Formal contact/Shadow renderer consumer','Elapsed Shadow animation timing','Complete scene or save admission'])
 return p,metadata,proof
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);o=a.parse_args();out=o.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build','Stepping candidates must stay under build');p,model,proof=source();packed=binary_model(model);blob=encode(packed);require(parse(blob)==packed,'Stepping binary roundtrip');out.mkdir(parents=True,exist_ok=True);(out/'catacombs.encsteps').write_bytes(blob);(out/'source.json').write_bytes((json.dumps(dict(metadata=model,binary=packed,proof=proof,bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),scope='Detached polygon/Shadow policy candidate;no full scene/contact/rendering admission'),indent=2)+'\n').encode());print('Stepping candidate:',len(blob),'bytes;',len(model['areas']),'areas;',sum(len(s['parts'])for a in model['areas']for s in a['shapes']),'convex parts;default',model['default'],';no scene admission')
