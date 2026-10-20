"""Checked source-owner/profile/animation-role bundle candidates.

Independent ENCDMA01 data only. No live scene, media, collision or save admission.
"""
import argparse,hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import PIN,require
from tools import dungeon_switches as graph,dungeon_switch_clips as clips,dungeon_wall_timeline as wall,prop_clips
from tools.podunk_field import pack_container,unpack_container

RECIPE=ROOT/'content/catacombs-mechanism-bindings.json'
MAGIC=b'ENCDMA01';STRIDES=(40,16,1,1);LIMIT=256*1024
ROLES=('None','TurnOff','TurnOn','Off','On','StayOff','StayOn')
FAMILIES={'lever':1,'plate':2,'gate':3,'wall':4}
DEPS={'graph':'content/catacombs-switches.json','property_clips':'content/catacombs-switch-clips.json','wall_timeline':'content/catacombs-wall-timeline.json'}

def fields(p,names):require(type(p)is dict and set(p)==set(names.split()),'Mechanism binding fields')
def integer(v,lo,hi):require(type(v)is int and lo<=v<=hi,'Mechanism binding integer');return v

def read(file=RECIPE):
 def unique(rows):
  out={}
  for k,v in rows:require(k not in out,'Duplicate mechanism review field');out[k]=v
  return out
 def invalid(v):raise ValueError('Nonfinite mechanism review')
 p=json.loads(Path(file).read_text(encoding='utf8'),object_pairs_hook=unique,parse_constant=invalid)
 fields(p,'schema kind commit scope dependencies profiles bindings')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.dungeon-mechanism-bindings.reviewed-source'and p['commit']==PIN,'Mechanism review schema/pin')
 require(type(p['scope'])is str and 0<len(p['scope'])<=2048,'Mechanism review scope');fields(p['dependencies'],'graph property_clips wall_timeline')
 for name,path in DEPS.items():
  row=p['dependencies'][name];fields(row,'path sha256');require(row['path']==path and row['sha256']==hashlib.sha256((ROOT/path).read_bytes()).hexdigest(),'Mechanism reviewed dependency changed')
 require(type(p['profiles'])is list and 0<len(p['profiles'])<=64 and type(p['bindings'])is list and 0<len(p['bindings'])<=256,'Mechanism review counts')
 ids=set();names=set()
 for row in p['profiles']:
  fields(row,'id kind source_profile name roles');integer(row['id'],1,65535);integer(row['source_profile'],0,65535)
  require(row['id']not in ids and type(row['name'])is str and row['name']in FAMILIES and row['name']not in names and row['kind']==('wall'if row['name']=='wall'else'properties'),'Mechanism profile identity/kind');ids.add(row['id']);names.add(row['name'])
  require(type(row['roles'])is dict and row['roles']and all(name in ROLES[1:]for name in row['roles']),'Mechanism profile role names')
  for value in row['roles'].values():integer(value,1,65535)
 for row in p['bindings']:
  fields(row,'owner path kind profile');integer(row['owner'],1,2**32-1);integer(row['profile'],1,65535);wall.path(row['path']);require(row['kind']in('switch','controlled'),'Mechanism binding owner kind')
 return p

def properties(blob):
 scene,parts=unpack_container(blob,prop_clips.MAGIC,prop_clips.STRIDES,prop_clips.LIMIT)
 require(scene==0 and len(parts[0])==16 and 0<len(parts[1])<=32*24 and 0<len(parts[2])<=4096*12,'Mechanism nested property counts')
 frames,initial,mask,reserved=struct.unpack('<4I',parts[0]);require(0<frames<=65535 and initial<frames and mask<=3 and not reserved,'Mechanism nested property initial pose')
 result={};first=0
 for at in range(0,len(parts[1]),24):
  ident,duration,start,count,r1,r2=struct.unpack_from('<If4I',parts[1],at);integer(ident,1,65535);wall.real(duration,0,60)
  require(ident not in result and duration>0 and start==first and 0<count<=len(parts[2])//12-first and not(r1 or r2),'Mechanism nested property clip');previous=(-1,0)
  for i in range(start,start+count):
   time,kind,value=struct.unpack_from('<f2I',parts[2],i*12);wall.real(time,0,duration);require(kind in(1,3)and(value<frames if kind==1 else value<=1)and(time,kind)>previous,'Mechanism nested property track/order/value');previous=(time,kind)
  result[ident]=duration;first+=count
 require(first==len(parts[2])//12,'Mechanism unowned property keys')
 return result

def parse(blob):
 scene,parts=unpack_container(blob,MAGIC,STRIDES,LIMIT);require(0<scene<65536 and 0<len(parts[0])<=256*40 and 0<len(parts[1])<=64*16 and 0<len(parts[2])<=LIMIT,'Mechanism section counts')
 g=graph.parse(parts[3]);require(g['scene']==scene,'Mechanism nested graph scene mismatch');owners={n['id']:(1,n['kind'])for n in g['switches']}
 owners.update({n['id']:(2,n['kind']+2)for n in g['objects']});require(len(owners)==len(g['switches'])+len(g['objects'])and len(parts[0])//40==len(owners),'Mechanism graph owner coverage')
 ownerpaths={n['id']:n['path']for n in g['switches']+g['objects']}
 profiles={};offset=0
 for at in range(0,len(parts[1]),16):
  ident,family,start,size=struct.unpack_from('<4I',parts[1],at);integer(ident,1,65535)
  require(ident not in profiles and family in FAMILIES.values()and start==offset and 0<size<=len(parts[2])-offset,'Mechanism profile payload ownership');raw=parts[2][start:start+size]
  if family==4:
   model=wall.parse(raw);require(model['scene']==scene and model['owner']in owners and owners[model['owner']]==(2,4)and model['path']==ownerpaths[model['owner']],'Mechanism wall owner/scene mismatch');available={c['id']for c in model['properties']['clips']}
  else:model=None;available=set(properties(raw))
  profiles[ident]=dict(id=ident,family=family,bytes=raw,wall=model,available=available);offset+=size
 require(offset==len(parts[2]),'Mechanism unowned profile bytes');bindings=[];seen=set();used=set()
 for at in range(0,len(parts[0]),40):
  owner,kind,profile,*roles=struct.unpack_from('<10I',parts[0],at)
  require(owner in owners and owner not in seen and profile in profiles and kind==owners[owner][0]and profiles[profile]['family']==owners[owner][1],'Mechanism binding graph/profile identity')
  p=profiles[profile];required={1,2}if kind==1 else{4,5,6}if p['family']==4 else{3,4,5,6}
  require({i for i,v in enumerate(roles)if v}==required and len({roles[i]for i in required})==len(required)and all(roles[i]in p['available']for i in required),'Mechanism role coverage/clip identity')
  if p['family']==4:
   w=p['wall'];require(w['owner']==owner and [roles[i]for i in(4,5,6)]==[w['roles'][i]for i in(0,2,3)],'Mechanism wall role/owner mapping')
  seen.add(owner);used.add(profile);bindings.append(dict(owner=owner,kind=kind,profile=profile,roles=roles))
 require(seen==set(owners)and used==set(profiles),'Mechanism missing owner/unused profile')
 return dict(scene=scene,graph=g,graph_bytes=parts[3],profiles=list(profiles.values()),bindings=bindings)

def extract(file=RECIPE):
 p=read(file);g,gproof=graph.extract();greview,_=graph.read();models,cproof=clips.extract();w,wblob,wproof=wall.extract();byid={row['profile']['id']:row for row in models};profiles=[]
 for row in p['profiles']:
  if row['kind']=='properties':
   require(row['source_profile']in byid,'Mechanism source profile missing');source=byid[row['source_profile']];require(row['name']==source['profile']['name'],'Mechanism source family changed');available={c['name']:c['id']for c in source['profile']['clips']};raw=source['bytes']
  else:
   require(row['source_profile']==0 and row['name']=='wall','Mechanism wall profile selector');available={c['name']:c['id']for c in json.loads(wall.RECIPE.read_bytes())['roles']};raw=wblob
  require(row['roles']=={name:ident for name,ident in available.items()if name!='RESET'},'Mechanism source role mapping changed')
  profiles.append(dict(id=row['id'],family=FAMILIES[row['name']],bytes=raw,roles=[row['roles'].get(name,0)for name in ROLES],name=row['name']))
 byprofile={v['id']:v for v in profiles};expected={n['id']:n for n in greview['nodes']};require(len(p['bindings'])==len(expected),'Mechanism source owner count');bindings=[]
 for row in p['bindings']:
  require(row['owner']in expected and row['profile']in byprofile,'Mechanism reviewed owner/profile missing');node=expected[row['owner']];profile=byprofile[row['profile']]
  require(row['path']==node['path']and profile['name']==node['kind']and row['kind']==('switch'if node['kind']in('lever','plate')else'controlled'),'Mechanism reviewed source binding differs')
  bindings.append(dict(owner=row['owner'],kind=1 if row['kind']=='switch'else 2,profile=row['profile'],roles=profile['roles']))
 source=graph.Sources(ROOT);allnodes=graph.instantiate(source.doc,greview['paths']['scene']);paths={row['path']for row in p['bindings']};order=[n.path for n in allnodes if n.path in paths]
 require([row['path']for row in p['bindings']]==order and len(paths)==len(expected),'Mechanism original effective child order changed')
 rows=b''.join(struct.pack('<10I',row['owner'],row['kind'],row['profile'],*row['roles'])for row in bindings);headers=bytearray();payload=bytearray()
 for row in profiles:headers.extend(struct.pack('<4I',row['id'],row['family'],len(payload),len(row['bytes'])));payload.extend(row['bytes'])
 blob=pack_container(MAGIC,g['scene'],[rows,headers,payload,graph.encode(g)],STRIDES,LIMIT);parsed=parse(blob);require(parsed['bindings']==bindings,'Mechanism encoded binding disagreement')
 proof=dict(commit=PIN,scope=p['scope'],whole_scripts_approved=False,execution_admission=False,review_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),dependencies=p['dependencies'],source_proofs=dict(graph=gproof,properties=cproof,wall=wproof),source_node_order=order,bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),mapped_implementation={'checked_bindings':'runtime/dungeon_mechanism_bindings.cpp','graph_methods':'runtime/dungeon_switch_state.cpp','property_clock':'runtime/prop_clips.cpp','wall_clock':'runtime/dungeon_wall_state.cpp','source_order_and_callbacks':'runtime/dungeon_mechanism_state.cpp','admission':'Candidate only; live Room, prepared media, physical contact and game saves remain excluded.'})
 return parsed,blob,proof

def build(out,file=RECIPE):
 out=Path(out).resolve();require(out.is_relative_to((ROOT/'build').resolve())and out!=ROOT/'build','Mechanism candidates stay under build');model,blob,proof=extract(file);out.mkdir(parents=True,exist_ok=True);(out/'source.encdma').write_bytes(blob);proof['producer_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest();(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Mechanism binding candidate:',len(blob),'bytes;',len(model['bindings']),'source-ordered owners;',len(model['profiles']),'profiles; no scene/media/save admission')

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--recipe',type=Path,default=RECIPE);p.add_argument('--out',type=Path,required=True);a=p.parse_args();build(a.out,a.recipe)
