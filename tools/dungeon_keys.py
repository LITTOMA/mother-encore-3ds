"""Candidate source-owned dungeon key/lock state metadata; never stages a map.

The compiler checks source counter/flag/call ordering; output describes state
bindings only. The detached shared-core method state machine consumes them;
scene admission, timelines, rendering, audible output and save integration
remain unimplemented. This compiler cannot activate a scene.
"""
from __future__ import annotations
import argparse, hashlib, json, re, struct, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools.godot_text import instantiate
from tools.podunk_field import Sources,pack_container,unpack_container
RECIPE=ROOT/'content/catacombs-keys.json'
MAGIC=b'ENCDKEY1';STRIDES=(1,32,32);LIMIT=65536

def fields(value,names,label):
 require(type(value)is dict and set(value)==set(names.split()),'Dungeon key '+label+' fields')
def path(value):
 require(type(value)is str and 0<len(value)<=512 and not value.startswith('/')and all(32<=ord(c)<127 for c in value)and all(c not in value for c in ('\\',':','\0'))and all(p not in ('','.','..')for p in value.split('/')),'Dungeon key unsafe path')
 return value
def read(file=RECIPE):
 def unique(rows):
  out={}
  for key,value in rows:
   require(key not in out,'Dungeon key duplicate JSON field');out[key]=value
  return out
 def invalid(value):raise ValueError('Dungeon key nonfinite JSON')
 p=json.loads(Path(file).read_text(encoding='utf8'),object_pairs_hook=unique,parse_constant=invalid)
 fields(p,'schema kind commit scope scene_id paths sources nodes reviewed_source_boundary','review')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.dungeon-keys.reviewed-source'and p['commit']==PIN,'Dungeon key schema/pin')
 require(type(p['scene_id'])is int and 0<p['scene_id']<65536 and all(type(p[k])is str and 0<len(p[k])<=2048 for k in ('scope','reviewed_source_boundary')),'Dungeon key scene/scope')
 fields(p['paths'],'scene key_scene key_script door_scene door_script flag_script ui_script globaldata_script save locked_dialogue','selectors')
 selected=[path(v)for v in p['paths'].values()]
 require(len(set(selected))==len(selected)and type(p['sources'])is dict and set(p['sources'])==set(selected),'Dungeon key source coverage')
 ex=Extractor(ROOT)
 for name,digest in p['sources'].items():
  require(type(digest)is str and re.fullmatch('[0-9a-f]{64}',digest)and hashlib.sha256(ex.data(name)).hexdigest()==digest,'Dungeon key source changed: '+name)
 require(type(p['nodes'])is list and 0<len(p['nodes'])<=128,'Dungeon key node count')
 ids=set();paths=set()
 for row in p['nodes']:
  fields(row,'id path kind','node');path(row['path'])
  require(type(row['id'])is int and 0<row['id']<2**32 and row['id']not in ids and row['path']not in paths and row['kind']in ('key','lock'),'Dungeon key node identity')
  ids.add(row['id']);paths.add(row['path'])
 return p,ex

def function(text,name):
 matches=list(re.finditer(r'^func '+re.escape(name)+r'\([^\n]*\)[^\n]*\n(.*?)(?=^func |\Z)',text,re.M|re.S))
 require(len(matches)==1,'Dungeon key function changed: '+name)
 return matches[0][1]
def ordered(text,tokens,label):
 at=-1
 for token in tokens:
  found=text.find(token,at+1);require(found>at,'Dungeon key source order: '+label);at=found

def extract(file=RECIPE):
 p,ex=read(file);s=p['paths'];sources=Sources(ROOT);nodes=instantiate(sources.doc,s['scene']);by_path={n.path:n for n in nodes};root=by_path['.'];doc=sources.doc(s['scene'])
 require(root.script is not None and type(root.props.get('_region_name'))is str,'Dungeon key region missing')
 region=root.props['_region_name'];path(region);scene_name=doc.nodes[0].attrs['name']
 ui=ex.text(s['ui_script']);alter=function(ui,'try_alter_key_count');get=function(ui,'get_key_count')
 bounds=re.findall(r'key_count(?: \+ delta)? >= (-?[0-9]+)',alter)
 require(len(bounds)==2 and bounds[0]==bounds[1],'Dungeon key counter gate changed');minimum=int(bounds[0])
 require('globaldata.keys.get(global.currentScene.get_region_name(), 0)'in get and 'globaldata.keys[global.currentScene.get_region_name()] = globaldata.keys.get(global.currentScene.get_region_name(), 0) + delta'in alter,'Dungeon key region counter selector changed')
 key=ex.text(s['key_script']);door=ex.text(s['door_script']);flag=ex.text(s['flag_script']);globaldata=ex.text(s['globaldata_script'])
 require('global.currentScene.name + "/" + name'in function(flag,'_get_flag_status')and'global.currentScene.name + "/" + name'in function(flag,'_set_flag_status'),'Dungeon key fallback flag identity changed')
 require('object_flags[flag_name] = value'in function(globaldata,'set_object_flag')and'if emit_signal: global.emit_signal("flags_updated")'in function(globaldata,'set_object_flag'),'Dungeon key flag notification changed')
 contact=function(key,'_on_Area2D_body_entered');interact=function(door,'interact')
 require('body != global.get_player() or $AudioStreamPlayer.playing or global.get_player().is_paused()'in contact,'Dungeon key pickup gate changed')
 ordered(contact,['uiManager.try_alter_key_count(','_set_flag_status()','uiManager.update_key_indicator()','$AudioStreamPlayer.playing = true','self.visible = false','$Area2D/CollisionShape2D.set_deferred("disabled", true)'],'pickup')
 ordered(interact,['if opened:','uiManager.try_alter_key_count(','uiManager.open_dialogue_box(','$AnimationPlayer.play("Open")','opened = true','_set_flag_status()','uiManager.update_key_indicator()'],'lock')
 require('if _get_flag_status():\n\t\tqueue_free()'in function(key,'_ready')and function(key,'_on_AudioStreamPlayer_finished').strip()=='queue_free()'and function(key,'_on_AnimationPlayer_animation_finished').strip()=='queue_free()','Dungeon key deferred deletion changed')
 ordered(function(door,'_ready'),['if !_get_flag_status():','opened = true','$AnimationPlayer.play("Opened")','open()'],'saved lock ready')
 ordered(function(door,'open'),['$StaticBody2D/CollisionShape2D.disabled = true','$interact/ButtonPrompt.enabled = false'],'open callback')
 deltas=[]
 for body in (contact,interact):
  found=re.findall(r'uiManager.try_alter_key_count\(([+-]?[0-9]+)\)',body);require(len(found)==1,'Dungeon key counter delta selector');deltas.append(int(found[0]))
 require(deltas[0]>0 and deltas[1]<0 and minimum>=0,'Dungeon key counter domain')
 locked=re.findall(r'uiManager.open_dialogue_box\("([^"\n]+)"\)',interact);require(len(locked)==1 and 'Data/Dialogue/'+locked[0]+'.yaml'==s['locked_dialogue'],'Dungeon key locked message binding')
 actual=[n for n in nodes if n.scene in (s['key_scene'],s['door_scene'])];require([n.path for n in actual]==[r['path']for r in p['nodes']],'Dungeon key source node order/coverage')
 rows=[]
 for row,n in zip(p['nodes'],actual):
  kind=1 if n.scene==s['key_scene']else 2
  require(row['kind']==('key'if kind==1 else'lock')and n.script==s['key_script'if kind==1 else'door_script'],'Dungeon key node script/kind')
  require(set(n.props)<=set('position script hframes flag is_object_flag emit_flag_updated_signal reset_when_leaving_region reset_when_leaving_area'.split()),'Dungeon key unreviewed node override')
  require(n.props.get('is_object_flag',False)is True and n.props.get('emit_flag_updated_signal',False)is True and n.props.get('reset_when_leaving_region',False)is False and n.props.get('reset_when_leaving_area',False)is False,'Dungeon key source flag lifetime/notification')
  flag_value=n.props.get('flag','');require(flag_value is None or type(flag_value)is str,'Dungeon key flag value type')
  identity=flag_value or scene_name+'/'+n.path.rsplit('/',1)[-1];path(identity)
  rows.append(dict(id=row['id'],kind=kind,path=n.path,flag=identity,delta=deltas[kind-1],flags=3))
 require(len({r['flag']for r in rows})==len(rows),'Dungeon key saved identity collision')
 save=ex.yaml(s['save']);require(type(save.get('keys'))is dict and type(save['keys'].get(region))is int,'Dungeon key source initial counter')
 initial=save['keys'][region];maximum=initial+sum(r['delta']for r in rows if r['kind']==1)
 require(minimum<=initial<=maximum<=65535,'Dungeon key initial/reachable counter bound')
 return dict(scene=p['scene_id'],region=region,initial=initial,minimum=minimum,maximum=maximum,locked=locked[0],nodes=rows),dict(commit=PIN,whole_scripts_approved=False,sources=dict(sorted({**ex.sources,**sources.digests()}.items())),scope=p['scope'])

def _encode(data):
 strings=bytearray();offsets={}
 def text(value):
  if value not in offsets:offsets[value]=(len(strings),len(value.encode('utf8')));strings.extend(value.encode('utf8'))
  return offsets[value]
 region=text(data['region']);locked=text(data['locked'])
 policy=struct.pack('<2I3iI2I',*region,data['initial'],data['minimum'],data['maximum'],0,*locked)
 rows=bytearray()
 for row in data['nodes']:rows.extend(struct.pack('<6IiI',row['id'],row['kind'],*text(row['path']),*text(row['flag']),row['delta'],row['flags']))
 return pack_container(MAGIC,data['scene'],[strings,policy,rows],STRIDES,LIMIT)

def parse(blob):
 scene,sections=unpack_container(blob,MAGIC,STRIDES,LIMIT)
 require(0<scene<65536 and len(sections[1])==32 and 0<len(sections[2])//32<=128,'Dungeon key policy/node count')
 strings,policy,nodes=sections
 def text(offset,length):
  require(length>0 and offset<=len(strings)and length<=len(strings)-offset,'Dungeon key text span')
  try:value=strings[offset:offset+length].decode('ascii')
  except UnicodeDecodeError as error:raise ValueError('Dungeon key text encoding')from error
  return path(value)
 a=struct.unpack('<2I3iI2I',policy);region=text(*a[:2]);locked=text(*a[6:]);initial,minimum,maximum=a[2:5]
 require(a[5]==0 and 0<=minimum<=initial<=maximum<=65535,'Dungeon key counter policy')
 rows=[];ids=set();paths=set();flags=set()
 for at in range(0,len(nodes),32):
  row=struct.unpack_from('<6IiI',nodes,at);identity,kind=row[:2];node_path=text(*row[2:4]);flag=text(*row[4:6]);delta,bits=row[6:]
  require(identity>0 and identity not in ids and kind in(1,2)and node_path not in paths and flag not in flags and bits==3,'Dungeon key node identity/flags')
  require((0<delta<=65535 if kind==1 else -65535<=delta<0),'Dungeon key counter delta')
  ids.add(identity);paths.add(node_path);flags.add(flag);rows.append(dict(id=identity,kind=kind,path=node_path,flag=flag,delta=delta,flags=bits))
 require(maximum==initial+sum(row['delta']for row in rows if row['kind']==1),'Dungeon key reachable counter bound')
 result=dict(scene=scene,region=region,initial=initial,minimum=minimum,maximum=maximum,locked=locked,nodes=rows)
 require(_encode(result)==blob,'Dungeon key noncanonical/unused data')
 return result

def encode(data):
 fields(data,'scene region initial minimum maximum locked nodes','encoded state')
 require(type(data['scene'])is int and 0<data['scene']<65536 and all(type(data[k])is int and 0<=data[k]<=65535 for k in ('initial','minimum','maximum')),'Dungeon key encoded integer policy')
 path(data['region']);path(data['locked'])
 require(type(data['nodes'])is list and 0<len(data['nodes'])<=128,'Dungeon key encoded nodes')
 for row in data['nodes']:
  fields(row,'id kind path flag delta flags','encoded node')
  require(all(type(row[k])is int for k in ('id','kind','delta','flags'))and 0<row['id']<2**32 and 0<=row['kind']<2**32 and -65535<=row['delta']<=65535 and 0<=row['flags']<2**32,'Dungeon key encoded node integer')
  path(row['path']);path(row['flag'])
 blob=_encode(data);parse(blob);return blob

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--recipe',type=Path,default=RECIPE);parser.add_argument('--out',type=Path,required=True);args=parser.parse_args()
 out=args.out.resolve();require(out.is_relative_to((ROOT/'build').resolve())and out.suffix=='.encdkeys','Dungeon key candidate must stay under build')
 data,proof=extract(args.recipe);blob=encode(data);parse(blob);out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(blob)
 proof.update(bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),data=data,inputs={'converter_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'review_sha256':hashlib.sha256(args.recipe.read_bytes()).hexdigest()});out.with_suffix('.source.json').write_text(json.dumps(proof,ensure_ascii=False,indent=2)+'\n',encoding='utf8');print('Candidate key/lock state metadata:',len(blob),'bytes; no live scene/presentation/save admission')
if __name__=='__main__':main()
