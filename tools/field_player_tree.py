"""Compile the audited eight-state player's complete travel and pause graph.

ENCSWE01 capability 2 retains capability 1 frame clips and incoming Bat paths.
It adds the incapacitated Idle role, per-state automatic/freeze policy and a
complete bounded route matrix. No general AnimationTree interpreter is used.
"""
import argparse,hashlib,json,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_swing_entry as entry,field_swing,podunk_field as f
from tools.godot_text import Document
from tools.extract_battle_entry import require
RECIPE=ROOT/'content/field-player-tree.json';STRIDES=(*entry.STRIDES,16)

def read(path=RECIPE):
 p=json.loads(Path(path).read_text(encoding='utf8'),object_pairs_hook=entry.unique)
 require(type(p)is dict and set(p)==set('schema kind entry_recipe incapacitated_idle path_reference pause_reference'.split()),'Player tree binding fields')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.player-tree.reviewed-bindings'and p['entry_recipe']=='content/field-swing-entry.json','Player tree binding schema')
 require(type(p['incapacitated_idle'])is int and 0<p['incapacitated_idle']<=65535,'Player tree incapacitated role')
 for k in('path_reference','pause_reference'):require(type(p[k])is str and re.fullmatch('reports/iggy-tutorial/[a-z0-9-]+',p[k]),'Player tree reference path')
 return p

def source(path=RECIPE):
 p=read(path);base,proof=entry.source(ROOT/p['entry_recipe']);by_name={s['name']:s['id']for s in base['states']};lookup={s['id']:s for s in base['states']}
 require(p['incapacitated_idle']in lookup and lookup[p['incapacitated_idle']]['name']=='FaintedIdle','Player tree incapacitated Idle mapping')
 dependencies={};sha=lambda file:hashlib.sha256(file.read_bytes()).hexdigest()
 def checked_reference(folder):
  folder=ROOT/folder;metadata=json.loads((folder/'source.json').read_bytes());actual=json.loads((folder/'reference.json').read_bytes())
  require(metadata['engine']['string']=='3.6.2-stable (official)'and actual['engine']==metadata['engine'],'Player tree reference engine')
  for relative,digest in metadata['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/relative)==digest,'Player tree reference source changed')
  for relative,digest in metadata['outputs'].items():require(sha(folder/relative)==digest,'Player tree reference output changed')
  for name in('source.json','reference.json'):dependencies[(folder/name).relative_to(ROOT).as_posix()]=sha(folder/name)
  return metadata,actual
 paths_proof,actual=checked_reference(p['path_reference']);pause_proof,_=checked_reference(p['pause_reference'])
 require(paths_proof['closed']is True and paths_proof['routes_each']==len(by_name)**2 and actual['paths'][0]==actual['paths'][1],'Player tree native route closure')
 source,ex=field_swing.read();doc=Document(source['scene'],ex.text(source['scene']));tree=next(n for n in doc.nodes if n.attrs['name']=='AnimationTree');machine=doc.sub[tree.props['tree_root'].id][1];edges={};automatic={}
 for a,b,ref in zip(machine['transitions'][::3],machine['transitions'][1::3],machine['transitions'][2::3]):
  if a not in by_name or b not in by_name:continue
  kind,policy=doc.sub[ref.id];require(kind=='AnimationNodeStateMachineTransition'and set(policy)<={'switch_mode','auto_advance'}and type(policy.get('switch_mode',0))is int and policy.get('switch_mode',0)in(0,2)and type(policy.get('auto_advance',False))is bool,'Player tree unknown edge policy')
  require((a,b)not in edges,'Player tree duplicate original edge');edges[a,b]=policy.get('switch_mode',0)
  if policy.get('auto_advance',False):require(a not in automatic,'Player tree ambiguous automatic edge');automatic[a]=b
 party_path='Scripts/Main/party/party_object.gd';party=ex.text(party_path);looped=re.search(r'^const LOOPED_ANIMS := (.+)$',party,re.M);require(looped is not None,'Player tree missing pause list');looped=json.loads(looped[1]);require(looped==pause_proof['looped_animations']and len(set(looped))==len(looped)and all(n in by_name for n in looped),'Player tree unreviewed pause list')
 for symbol,digest in pause_proof['pause_methods'].items():
  relative,name=symbol.split('::');text=ex.text(relative);method=re.search(r'^func '+re.escape(name)+r'\(.*?(?=^func |^static func |\Z)',text,re.M|re.S);require(method is not None and hashlib.sha256(method[0].encode()).hexdigest()==digest,'Player tree pause method changed')
 routes=[]
 require(set(actual['paths'][0])=={a+'->'+b for a in by_name for b in by_name},'Player tree route domain')
 for a in by_name:
  for b in by_name:
   route=actual['paths'][0][a+'->'+b];require(type(route)is list and (not route if a==b else bool(route)and route[-1]==b),'Player tree route endpoint')
   for origin,target in zip([a,*route],route):require((origin,target)in edges,'Player tree route source edge missing')
   routes.append(dict(from_state=by_name[a],to_state=by_name[b],next=by_name[route[0]]if route else 0,switch=edges[a,route[0]]if route else 0))
 flags=[dict(state=s['id'],automatic=by_name[automatic[s['name']]]if s['name']in automatic else 0,pausable=s['name']in looped)for s in base['states']]
 dependencies.update({Path(path).resolve().relative_to(ROOT).as_posix():sha(Path(path)),'tools/field_player_tree.py':sha(Path(__file__))})
 model=dict(entry=base,incapacitated_idle=p['incapacitated_idle'],flags=flags,routes=routes)
 proof.update(scope=__doc__,sources={**proof['sources'],**ex.sources},dependencies={**proof['dependencies'],**dependencies},automatic_edges=automatic,looped_animations=looped,route_reference=p['path_reference'],pause_reference=p['pause_reference'])
 return model,proof

def encode(model):
 require(type(model)is dict and set(model)=={'entry','incapacitated_idle','flags','routes'},'Player tree model fields')
 base=model['entry'];_,parts=f.unpack_container(entry.encode(base),entry.MAGIC,entry.STRIDES,entry.LIMIT);ids={s['id']for s in base['states']};attack=base['policy'][2];incap=model['incapacitated_idle']
 require(type(incap)is int and incap in ids and incap not in(base['policy'][1],attack),'Player tree incapacitated role')
 require(type(model['flags'])is list and len(model['flags'])==len(ids)and type(model['routes'])is list and len(model['routes'])==len(ids)**2,'Player tree model coverage')
 routes={};rows=[]
 for r in model['routes']:
  require(type(r)is dict and set(r)=={'from_state','to_state','next','switch'}and all(type(v)is int for v in r.values())and r['from_state']in ids and r['to_state']in ids and (r['from_state'],r['to_state'])not in routes and r['switch']in(0,2),'Player tree route identity')
  require((r['next']==0 and r['switch']==0)if r['from_state']==r['to_state']else(r['next']in ids and r['next']!=r['from_state']),'Player tree route target');routes[r['from_state'],r['to_state']]=r;rows.append(struct.pack('<4I',r['from_state'],r['to_state'],r['next'],r['switch']))
 for r in routes.values():
  seen=set();at=r['from_state'];target=r['to_state']
  while at!=target:
   require(at not in seen,'Player tree route cycle');seen.add(at);step=routes[at,target];direct=routes[at,step['next']];require(direct['next']==step['next']and direct['switch']==step['switch'],'Player tree inconsistent edge');at=step['next']
 state_rows=[]
 for index,(state,flags)in enumerate(zip(base['states'],model['flags'])):
  require(type(flags)is dict and set(flags)=={'state','automatic','pausable'}and type(flags['state'])is int and flags['state']==state['id']and type(flags['automatic'])is int and type(flags['pausable'])is bool,'Player tree state policy')
  auto=flags['automatic'];require(auto==0 or(auto in ids and auto!=state['id']and routes[state['id'],auto]['next']==auto),'Player tree automatic edge')
  if flags['pausable']:require(state['count']and all(c['loop']for c in base['clips'][state['first']:state['first']+state['count']]),'Player tree pausable nonloop')
  if state['id']!=attack:require((routes[state['id'],attack]['next'],routes[state['id'],attack]['switch'])==(state['next'],state['switch']),'Player tree legacy route differs')
  words=list(struct.unpack_from('<8I',parts[1],index*32));words[7]=auto|int(flags['pausable'])<<16;state_rows.append(struct.pack('<8I',*words))
 meta=list(struct.unpack('<12I',parts[0]));meta[7]=incap
 return f.pack_container(entry.MAGIC,0,[struct.pack('<12I',*meta),b''.join(state_rows),*parts[2:],b''.join(rows)],STRIDES,entry.LIMIT,capability=2)

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);o=p.parse_args();out=o.out.resolve();require(out.is_relative_to(ROOT/'build')and not out.exists(),'Fresh owned player tree candidate required');model,proof=source();blob=encode(model);out.mkdir(parents=True);(out/'swing.encentry').write_bytes(blob);proof.update(model=model,bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest());(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Player tree:',len(blob),'bytes; 64 routes; capability 2 candidate only')
