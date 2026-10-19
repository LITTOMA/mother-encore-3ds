"""Checked source review for Iggy's tutorial; full programme lowering pending."""
import hashlib,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools.programme_lowering_recipe import document
from tools.godot_text import Document
RECIPE=ROOT/'content/graveyard-tutorial.json'

def read(path=RECIPE):
 def unique(pairs):
  result={}
  for k,v in pairs:require(k not in result,'Duplicate tutorial field');result[k]=v
  return result
 p=json.loads(Path(path).read_text(encoding='utf8'),object_pairs_hook=unique)
 require(type(p)is dict and set(p)==set('schema kind commit scene source npc boundary documents item_grants sources scope'.split()),'Tutorial review fields')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.graveyard-tutorial.reviewed-source'and p['commit']==PIN,'Tutorial schema/pin')
 require(type(p['scene'])is int and 0<p['scene']<=65535 and type(p['scope'])is str and 0<len(p['scope'])<=2048,'Tutorial scene/scope')
 def safe(value):return type(value)is str and 0<len(value)<=512 and ':'not in value and '\\'not in value and all(s not in ('','.','..')for s in value.split('/'))
 require(all(safe(p[k])for k in ('source','npc','boundary'))and p['source'].startswith('Data/Dialogue/')and p['source'].endswith('.yaml'),'Tutorial source paths')
 require(type(p['sources'])is dict and p['sources']and all(safe(k)and type(v)is str and re.fullmatch('[0-9a-f]{64}',v)for k,v in p['sources'].items()),'Tutorial source identities')
 require(type(p['documents'])is dict and set(p['documents'])=={p['source']}and p['source']in p['sources'],'Tutorial document identity')
 ex=Extractor(ROOT)
 for name,digest in p['sources'].items():require(hashlib.sha256(ex.data(name)).hexdigest()==digest,'Tutorial reviewed source changed: '+name)
 doc=document(p,p['source'],ex.yaml(p['source']))
 maps=[name for name in p['sources']if name.startswith('Maps/')and name.endswith('.tscn')];require(len(maps)==1,'Tutorial source map identity')
 scene=Document(maps[0],ex.text(maps[0]));nodes={}
 for n in scene.nodes:
  parent=n.attrs.get('parent');name=n.attrs['name'];key='.'if parent is None else name if parent=='.'else parent+'/'+name;require(key not in nodes,'Duplicate tutorial source node');nodes[key]=n
 require(p['npc']in nodes and p['boundary']in nodes and nodes[p['boundary']].props.get('dialog')==p['source'].removeprefix('Data/Dialogue/').removesuffix('.yaml'),'Tutorial NPC/boundary source binding')
 require(type(p['item_grants'])is list and 0<len(p['item_grants'])<=16,'Tutorial key grants');seen=set()
 for g in p['item_grants']:
  require(type(g)is dict and set(g)=={'label','item','doses','completion_label','completion_flag'},'Tutorial grant fields')
  require(all(type(g[k])is str for k in ('label','item','completion_label','completion_flag'))and re.fullmatch('[A-Za-z][A-Za-z0-9_]{0,63}',g['item'])and re.fullmatch('[A-Za-z][A-Za-z0-9_]{0,63}',g['completion_flag']),'Tutorial key/flag identity')
  require(type(g['doses'])is int and g['doses']==1 and g['item']not in seen and g['label']in doc and g['completion_label']in doc,'Tutorial key dose/labels');seen.add(g['item'])
  require(doc[g['label']].get('item')==g['item']and 'setflags'not in doc[g['label']]and doc[g['completion_label']].get('setflags')==g['completion_flag']and list(doc).index(g['label'])<list(doc).index(g['completion_label']),'Tutorial grant must precede its source completion flag')
  source='Data/Items/'+g['item']+'.yaml';require(source in p['sources']and ex.yaml(source).get('keyitem')is True,'Tutorial source item is not KEY')
 return p,ex
