#!/usr/bin/env python3
"""Source InteractDialog instances -> checked ENCFDLG1; no runtime JSON."""
from __future__ import annotations
import argparse,json,math,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import decode,stable,SCENE,PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor
SCRIPT='Scripts/Main/Interact Dialog.gd'
IR=ROOT/'content/native-field-interact-dialog.json'
REVIEW=ROOT/'reports/field-interact-dialog/source-review.json'
PACK=ROOT/'romfs/data/podunk-interact.encdialog'
def source_instances(native,receipt,upstream):
 d=read(native);s=read(receipt);require(d['source']=='res://'+SCENE and d['schema']==1 and d['native_compatible'] is False,'Incomplete InteractDialog native scene')
 require(s['commit']==PIN and s['scene']==SCENE,'Unreviewed InteractDialog native receipt')
 inventory=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for name,record in s['files'].items():require(sha(upstream/name)==record['sha256']==inventory[name]['sha256'],'Changed InteractDialog closure '+name)
 names={n['path']:n for n in d['nodes']};require(len(names)==8686,'Incomplete Podunk InteractDialog scene');require(decode(names['Objects']['world_transform'])==[[1,0],[0,1],[0,0]],'InteractDialog Objects transform requires a source adapter')
 resources={r['id']:r for r in d['resources']};states={s['source'][6:]:s for s in d['scene_states']};roots=[]
 def visit_instances(path,filename):
  roots.append((path,filename))
  for n in states[filename]['nodes']:
   if n['instance'] is not None:
    local=n['path'][2:] if n['path'].startswith('./') else n['path'];target=local if path=='.' else path+'/'+local
    visit_instances(target,resources[n['instance']['id']]['path'][6:])
 visit_instances('.',SCENE);bindings={};overrides={}
 for root,filename in sorted(roots,key=lambda r:r[0].count('/') if r[0]!='.' else -1,reverse=True):
  for n in states[filename]['nodes']:
   local=n['path'][2:] if n['path'].startswith('./') else n['path'];path=root if local=='.' else local if root=='.' else root+'/'+local
   if path in names:overrides.setdefault(path,{}).update(decode(n['properties']))
  for a in s['script_attachments']:
   if a['source']!=filename:continue
   name=re.search(r'\bname="([^"]+)"',a['node_declaration'])[1];parent=re.search(r'\bparent="([^"]+)"',a['node_declaration'])
   local='.' if parent is None else name if parent[1]=='.' else parent[1]+'/'+name;path=root if local=='.' else local if root=='.' else root+'/'+local
   require(path in names,'Unresolved InteractDialog binding '+path);bindings[path]=a['script']
 children={p:[] for p in names}
 for path in names:
  if path!='.':children[path.rsplit('/',1)[0] if '/' in path else '.'].append(path)
 ready=[]
 def visit(path):
  for child in children[path]:visit(child)
  ready.append(path)
 visit('.');ordinal={p:i for i,p in enumerate(ready)};out=[]
 for path in sorted(bindings,key=lambda p:ordinal[p]):
  if bindings[path] not in (SCRIPT,):continue
  out.append(dict(stable_id=stable(path),node=path,ready_ordinal=ordinal[path],script=bindings[path],overrides=overrides[path],native=names[path],children=[names[p] for p in children[path]]))
 require(len(out)==33,'Podunk InteractDialog binding loss');return out,names,resources,dict(native_export_sha256=sha(native),source_receipt_sha256=sha(receipt),source_files={name:r['sha256'] for name,r in s['files'].items()})

def build(records,nodes,resources,provenance):
 ex=Extractor(ROOT);ex.sources.update(provenance['source_files']);text=ex.text(SCRIPT)
 for token in ['_check_flags()','global.connect("flags_updated", self, "_check_flags")','visible = globaldata.check_appear_disappear_flags(appear_flag, disappear_flag)','queue_free()','item.item_name == _key_item','uiManager.set_telepathy_effect(true)','uiManager.open_dialogue_box(_thoughts)','flags.get(flag, false)','ret = flags[1]','$ButtonPrompt.offset = button_offset']:
  require(token in text,'Changed InteractDialog behavior: '+token)
 ex.text('Scripts/global/globalData.gd');ex.text('Scripts/global/uiManager.gd');out=[]
 for a in records:
  p=a['node'];o=a['overrides'];np=decode(a['native']['properties']);program=o.get('dialog')or'';thoughts=o.get('_thoughts')or'';choices=o.get('_all_dialog')or[]
  require(type(choices)is list and all(type(c)is list and len(c)==2 and all(type(x)is str for x in c)for c in choices),'Unknown InteractDialog override tuple')
  paths=[program,thoughts]+[v for _,v in choices]
  for v in paths:
   if v:ex.text('Data/Dialogue/'+v+'.yaml')
  key=o.get('_key_item')or''
  if key:ex.yaml('Data/Items/'+key+'.yaml')
  turn=o.get('player_turn',{'x':True,'y':True});turn=dict(turn['pairs'])if type(turn)is dict and set(turn)=={'type','pairs'}and turn['type']=='Dictionary'else turn;require(type(turn)is dict and set(turn)=={'x','y'}and all(type(v)is bool for v in turn.values()),'Unknown source player turn fields')
  offset=o.get('button_offset',[0,0]);require(len(offset)==2 and all(type(v)in(int,float)and math.isfinite(v)for v in offset),'Invalid source button offset')
  prompt=p+'/ButtonPrompt';require(prompt in nodes,'Missing InteractDialog source prompt')
  out.append(dict(id=a['stable_id'],node=p,ready=a['ready_ordinal'],prompt_id=stable(prompt),dialogue=program,thoughts=thoughts,key_item=key,appear=o.get('appear_flag',''),disappear=o.get('disappear_flag',''),no_problem=o.get('no_problem_thoughts',True),turn=turn,button_offset=offset,initial_visible=np['visible'],offset_assigned='button_offset'in o,choices=choices))
 return dict(schema=1,kind='encore.field-interact-dialog.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),script=SCRIPT,records=out,sources=dict(sorted(ex.sources.items())),provenance=provenance,producer_sha256=sha(Path(__file__)),semantics=['Complete 33 source instances with inner-to-outer overrides and exact postorder Ready','Ready checks appear/disappear, queues hidden objects for deletion then connects flags_updated synchronously','Dialogue starts from base; every matching nonempty normal flag overrides in source order; last match wins','Matching actual item_name opens dialogue; unmatched item has no effect','Telepathy turns on effect before opening exact thoughts; has_thoughts is source nonempty test','Button offset source setter forwarded to actual shared Prompt; player axis turn and no_problem flags remain descriptor data'],pending=['Actual programme/ray/prompt/flags/SceneTree endpoints must be admitted before scene activation'])
def validate(d):
 require(d['schema']==1 and d['kind']=='encore.field-interact-dialog.source-ir'and d['commit']==PIN and d['scene']==SCENE and d['script']==SCRIPT and len(d['records'])==33,'Unknown InteractDialog source schema')
 require(d['producer_sha256']==sha(Path(__file__)),'Changed InteractDialog producer')
 require(set(d)==set('schema kind commit scene scene_id script records sources provenance producer_sha256 semantics pending'.split()),'Unknown InteractDialog IR fields')
 require(d['scene_id']==stable('.')and set(d['provenance'])=={'native_export_sha256','source_receipt_sha256','source_files'},'InteractDialog provenance identity')
 for v in d['records']:
  require(set(v)==set('id node ready prompt_id dialogue thoughts key_item appear disappear no_problem turn button_offset initial_visible offset_assigned choices'.split()),'Unknown InteractDialog record fields')
  require(all(type(v[k])is bool for k in ['no_problem','initial_visible','offset_assigned'])and set(v['turn'])=={'x','y'}and all(type(x)is bool for x in v['turn'].values()),'InteractDialog source boolean fields')
  require(type(v['ready'])is int and 0<=v['ready']<8686 and len(v['button_offset'])==2 and all(type(x)in(int,float)and math.isfinite(x)for x in v['button_offset']),'InteractDialog source Ready/offset')
  require(all(type(v[k])is str for k in ['node','dialogue','thoughts','key_item','appear','disappear'])and all(type(c)is list and len(c)==2 and all(type(x)is str for x in c)for c in v['choices']),'InteractDialog source selector')
 inv=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for p,h in d['sources'].items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Changed source '+p)
 require(d['scene']in d['sources']and SCRIPT in d['sources'],'Missing InteractDialog source closure')
 ids=set();ordinals=set()
 for v in d['records']:
  require(v['id']==stable(v['node'])and v['prompt_id']==stable(v['node']+'/ButtonPrompt')and v['id']not in ids and v['ready']not in ordinals,'InteractDialog source identity')
  ids.add(v['id']);ordinals.add(v['ready'])
def encode(d):
 validate(d);b=bytearray()
 def put(fmt,*v):b.extend(struct.pack('<'+fmt,*v))
 def text(v):raw=v.encode('utf-8');require(len(raw)<=8192 and '\0'not in v,'Invalid InteractDialog text');put('I',len(raw));b.extend(raw)
 text(d['scene']);text(d['script']);put('I',len(d['sources']))
 for p,h in sorted(d['sources'].items()):text(p);b.extend(bytes.fromhex(h))
 put('I',len(d['records']))
 for v in d['records']:
  bits=int(v['no_problem'])|int(v['turn']['x'])<<1|int(v['turn']['y'])<<2|int(v['initial_visible'])<<3|int(v['offset_assigned'])<<4
  put('4I2f',v['id'],v['ready'],v['prompt_id'],bits,*v['button_offset'])
  for k in ['node','dialogue','thoughts','key_item','appear','disappear']:text(v[k])
  put('I',len(v['choices']))
  for f,p in v['choices']:text(f);text(p)
 header=bytearray(struct.pack('<8s6I20s12x',b'ENCFDLG1',1,64+len(b),0,1,1,d['scene_id'],bytes.fromhex(PIN)));header.extend(b);struct.pack_into('<I',header,16,zlib.crc32(header));return bytes(header)
def compile_pack():
 d=read(IR);review=read(REVIEW);require(review['commit']==PIN and review['ir_sha256']==sha(IR)and review['semantics']==d['semantics'],'InteractDialog semantic review changed');raw=encode(d);PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw);print('InteractDialog binary:',len(raw),'bytes; 33 source objects')
def stage_files(source):
 b=(Path(source)/'data/podunk-interact.encdialog').read_bytes();require(b==encode(read(IR)),'Stale InteractDialog pack');return{Path('data/podunk-interact.encdialog'):b}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);a=p.parse_args()
 if a.action=='extract':
  records,nodes,res,provenance=source_instances(a.native,a.source,ROOT/'upstream/MOTHER-Encore');d=build(records,nodes,res,provenance);write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],semantics=d['semantics'],pending=d['pending'],verification='Source extraction only; no behavior tests or device validation'))
 else:compile_pack()
if __name__=='__main__':main()
