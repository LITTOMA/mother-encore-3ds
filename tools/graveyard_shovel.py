"""Bounded source review for Iggy's ten-label getshovel programme.

This is an offline candidate boundary, not a general dialogue interpreter or
an assertion that the object, root hit and frontend have been connected.
"""
import hashlib,json,re
from pathlib import Path
from tools.extract_battle_entry import Extractor,PIN,require
from tools.programme_lowering_recipe import document
from tools.godot_text import Document
ROOT=Path(__file__).resolve().parents[1]
RECIPE=ROOT/'content/graveyard-shovel.json'

def read(path=RECIPE):
 def unique(pairs):
  out={}
  for k,v in pairs:require(k not in out,'Duplicate shovel review field');out[k]=v
  return out
 def invalid(_):raise ValueError('Nonfinite shovel review value')
 p=json.loads(Path(path).read_text(encoding='utf8'),object_pairs_hook=unique,parse_constant=invalid)
 require(type(p)is dict and set(p)==set('schema kind commit scene source npc boundary object dialogue_first_id documents sources scope'.split()),'Shovel source review fields')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.graveyard-shovel.reviewed-source'and p['commit']==PIN,'Shovel source schema/pin')
 require(type(p['scene'])is int and 0<p['scene']<=65535 and type(p['scope'])is str and 0<len(p['scope'])<=2048,'Shovel source scene/scope')
 require(type(p['dialogue_first_id'])is int and 0<p['dialogue_first_id']<=999995,'Shovel native dialogue identity range')
 def safe(s):return type(s)is str and 0<len(s)<=512 and all(32<=ord(c)<127 and c not in ':\\'for c in s)and '..'not in s and all(v not in('','.','..')for v in s.split('/'))
 require(all(safe(p[k])for k in('source','npc','boundary','object'))and p['source'].startswith('Data/Dialogue/')and p['source'].endswith('.yaml'),'Shovel source paths')
 require(type(p['sources'])is dict and bool(p['sources'])and all(safe(k)and type(v)is str and re.fullmatch('[0-9a-f]{64}',v)for k,v in p['sources'].items()),'Shovel reviewed source identities')
 require(type(p['documents'])is dict and set(p['documents'])=={p['source']}and p['source']in p['sources'],'Shovel document identity')
 ex=Extractor(ROOT)
 for name,digest in p['sources'].items():require(hashlib.sha256(ex.data(name)).hexdigest()==digest,'Shovel source fingerprint changed: '+name)
 doc=document(p,p['source'],ex.yaml(p['source']));require(len(doc)==10,'Shovel reviewed source label count')
 maps=[n for n in p['sources']if n.startswith('Maps/')and n.endswith('.tscn')];require(len(maps)==1,'Shovel source map identity')
 nodes={}
 for n in Document(maps[0],ex.text(maps[0])).nodes:
  parent=n.attrs.get('parent');key='.'if parent is None else n.attrs['name']if parent=='.'else parent+'/'+n.attrs['name'];require(key not in nodes,'Duplicate shovel source node');nodes[key]=n
 require(all(p[k]in nodes for k in('npc','boundary','object')),'Shovel source node binding')
 require(nodes[p['boundary']].props.get('dialog')==p['source'].removeprefix('Data/Dialogue/').removesuffix('.yaml'),'Shovel source boundary dialogue binding')
 require(doc['0'].get('actors')=={'ninten':'leader','iggy':p['npc']}and doc['0'].get('setflags')==nodes[p['object']].props.get('flag')=='got_shovel','Shovel actor/ready flag binding')
 require(doc['3'].get('objectsfunction')=={p['object']:'queue_free'}and doc['11'].get('eraseactors')=={'iggy':True}and doc['11'].get('setflags')=='returned_shovel','Shovel explicit deletion/erasure/return binding')
 require(not any('item'in row or 'learnskills'in row for row in doc.values()),'Unmapped shovel grant/skill mechanism')
 handler=ex.text('Scripts/UI/DialogueBox.gd')
 require('object.call_deferred(objects[i])'in handler and 'if !_curr_phrase.has("text") and !_curr_phrase.has("wait") and !_curr_phrase.has("autowait"):'in handler,'Shovel deferred method/automatic terminal contract')
 require('yield(new_actor, "actor_ready")'in handler and '\t\tyield(get_tree(), "idle_frame")'in handler[handler.index('if _curr_phrase.has("actors"):'):handler.index('# wait: float')],'Shovel initial actor idle handoff changed')
 require('yield(get_tree(), "idle_frame")'in handler[handler.index('if _curr_phrase.has("changecam"):'):handler.index('# movecam:')],'Shovel camera idle handoff changed')
 require(handler.index('if _curr_phrase.has("eraseactors"):')<handler.index('if _curr_phrase.has("setflags"):')and '_actors.erase(i)'in handler,'Shovel erasure/flag/end actor domain order')
 return p,ex
