#!/usr/bin/env python3
"""Actual Podunk ButtonPrompt authoring -> checked binary; no runtime JSON."""
from pathlib import Path
import argparse,hashlib,json,math,re,struct,sys,zlib
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,SCENE,read,write,sha,require,decode,stable
from tools.extract_battle_entry import Extractor,node,animation,properties
SCRIPT='Scripts/UI/Button Prompt.gd';SOURCE='Nodes/Ui/ButtonPrompt.tscn'
IR=ROOT/'content/native-field-prompts.json';PACK=ROOT/'romfs/data/podunk-prompts.encfieldprompt'
REVIEW=ROOT/'compatibility/reviews/podunk-prompts-v0410.json'
LAYOUT=ROOT/'content/asset-receipts/graphics/ui/podunk-prompts-layout.json'
PROPERTIES={'Arrow:rect_position':1,'HBoxContainer:rect_position':2,'HBoxContainer/Label:rect_position':3,'HBoxContainer/Label:modulate':4,'.:modulate':5,'Arrow:modulate':6,'.:visible':7,'.:material:shader_param/glow_modifier':8,'.:material:shader_param/flash_color':9,'.:material:shader_param/flash_modifier':10}
def source_instances(native,receipt,upstream):
 d=read(native);s=read(receipt);require(d['source']=='res://'+SCENE and d['schema']==1 and d['native_compatible'] is False,'Incomplete NPC native scene')
 require(s['commit']==PIN and s['scene']==SCENE,'Unreviewed NPC native receipt')
 inventory=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for name,record in s['files'].items():require(sha(upstream/name)==record['sha256']==inventory[name]['sha256'],'Changed NPC closure '+name)
 names={n['path']:n for n in d['nodes']};require(len(names)==8686,'Incomplete Podunk enemy scene');require(decode(names['Objects']['world_transform'])==[[1,0],[0,1],[0,0]],'NPC Objects transform requires a source adapter')
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
   require(path in names,'Unresolved NPC binding '+path);bindings[path]=a['script']
 children={p:[] for p in names}
 for path in names:
  if path!='.':children[path.rsplit('/',1)[0] if '/' in path else '.'].append(path)
 ready=[]
 def visit(path):
  for child in children[path]:visit(child)
  ready.append(path)
 visit('.');ordinal={p:i for i,p in enumerate(ready)};out=[]
 for path in sorted(bindings,key=lambda p:ordinal[p]):
  if bindings[path]!=SCRIPT:continue
  out.append(dict(stable_id=stable(path),node=path,ready_ordinal=ordinal[path],overrides=overrides[path],native=names[path],children=[names[p] for p in children[path]]))
 require(len(out)==146,'Podunk ButtonPrompt binding loss');return out,names,resources,dict(native_export_sha256=sha(native),source_receipt_sha256=sha(receipt),source_files={name:r['sha256'] for name,r in s['files'].items()})

def art_binding(ex):
 layout=read(LAYOUT);require(layout['engine']['hash']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8','Unreviewed prompt layout engine')
 atlas=read(ROOT/'content/native-house-button-prompts.json');a=atlas['resources'][0];require(atlas['commit']==PIN and atlas['prompt_rect'][2:]==[a['width'],a['height']],'Prompt converted atlas binding')
 for path in ['Fonts/BottleRocket.tres','Fonts/BottleRocket.ttf','Graphics/UI/select_arrow.png']:ex.data(path);require(ex.sources[path]==atlas['sources'][path],'Prompt art source mismatch')
 require(sha(ROOT/'romfs'/a['path'])==a['sha256'],'Prompt actual atlas bytes changed')
 text=ex.text(SOURCE);box=node(text,'HBoxContainer');arrow=node(text,'Arrow');label=node(text,'HBoxContainer/Label');require(label['text']=='A' and arrow['rect_rotation']==90,'Prompt glyph/arrow adapter required')
 material=properties(ex.text('Shaders/Flash.tres').split('[resource]\n')[1]);require(material['shader_param/glow_color'][:3]==[0,0,0] and arrow['use_parent_material'] is True and not label.get('use_parent_material',False) and not box.get('use_parent_material',False),'Prompt source material inheritance changed')
 left,top,w,h=atlas['prompt_rect'];lx,ly,lw,lh=layout['label'];aw=arrow['margin_bottom']-arrow['margin_top'];ah=arrow['margin_right']-arrow['margin_left'];ax=arrow['margin_left']-aw;ay=arrow['margin_top'];split=int(ay-top)
 require(0<split<h and ax>=left and ax+aw<=left+w and ay+ah<=top+h,'Prompt atlas crop bounds')
 return dict(path=a['path'],width=w,height=h,sha256=a['sha256'],bytes=(ROOT/'romfs'/a['path']).stat().st_size,crc32=zlib.crc32((ROOT/'romfs'/a['path']).read_bytes()),label='A',label_crop=[0,0,w,split],arrow_crop=[int(ax-left),split,int(aw),int(ah)],label_origin=[left-lx,top-ly],arrow_origin=[-aw,0],label_local=[lx-box['margin_left'],ly-box['margin_top']],layout_sha256=sha(LAYOUT),engine_hash=layout['engine']['hash'])

def source_clips(text):
 clips=[];roles={'Float':1,'Hide':2,'Press':3,'RESET':4,'Show':5}
 for key,ref in node(text,'AnimationPlayer').items():
  if not key.startswith('anims/'):continue
  name=key.split('/',1)[1];require(name in roles and set(ref)=={'SubResource'},'Prompt animation reference');a=animation(text,ref['SubResource'],SOURCE,name);tracks=[]
  for t in a['tracks']:
   require(t['interp']==1 and t['loop_wrap'] and all(math.isfinite(v) for v in t['keys']['transitions']),'Prompt interpolation')
   if t['type']=='method':
    require(t['path']=='.' and all(v=={'args':[],'method':'hide'}for v in t['keys']['values']),'Unknown prompt method');prop=11;update=1;values=[[1,0,0,0]for v in t['keys']['values']]
   else:
    require(t['path']in PROPERTIES,'Unknown prompt animation property');prop=PROPERTIES[t['path']];update=t['keys']['update'];require(update in (0,1),'Prompt animation update');values=[]
    for value in t['keys']['values']:
     v=[int(value)]if type(value)is bool else value if type(value)is list else [value];require(len(v) in (1,2,4) and all(type(n)in(int,float)and math.isfinite(n)for n in v),'Prompt animation value');values.append(v+[0]*(4-len(v)))
   tracks.append(dict(property=prop,update=update,keys=[dict(time=t,ease=e,value=v)for t,e,v in zip(t['keys']['times'],t['keys']['transitions'],values)]))
  clips.append(dict(role=roles[name],name=name,length=a['length'],loop=a['loop'],tracks=tracks))
 return sorted(clips,key=lambda c:c['role'])

def build(records,nodes,proof):
 ex=Extractor(ROOT);text=ex.text(SOURCE);script=ex.text(SCRIPT);ex.text('Shaders/Flash.tres');ex.text('Shaders/Flash.shader');ex.text('LICENSE')
 require('set_process(false)' in script and '_player_nearby and !global.get_player().is_paused()'in script and '(should_show or _force_show) and !_force_hide'in script and 'if _pressing_button:'in script,'Changed ButtonPrompt source visibility')
 require('position.x = position.x / get_parent().scale.x'in script and 'scale.x = 1.0 / get_parent().scale.x'in script and 'emit_signal("hide")'in script,'Changed ButtonPrompt scale/Press source')
 root=node(text,'.');box=node(text,'HBoxContainer');label=node(text,'HBoxContainer/Label');arrow=node(text,'Arrow');material=properties(ex.text('Shaders/Flash.tres').split('[resource]\n')[1])
 art=art_binding(ex)
 initial=[[arrow.get('margin_left',0),arrow.get('margin_top',0),0,0],[box.get('margin_left',0),box.get('margin_top',0),0,0],[label.get('margin_left',0),label.get('margin_top',0),0,0],label.get('modulate',[1,1,1,1]),root.get('modulate',[1,1,1,1]),arrow.get('modulate',[1,1,1,1]),[int(root.get('visible',True)),0,0,0],[material['shader_param/glow_modifier'],0,0,0],material['shader_param/flash_color'],[material['shader_param/flash_modifier'],0,0,0]]
 initial[2]=art['label_local']+[0,0]
 choices=json.loads(re.search(r'const BUTTON_PROMPTS := (\[[^\n]+\])',ex.text('Scripts/global/globalData.gd'))[1]);require(set(choices)=={'Objects','NPCs','Both','None'},'Prompt source settings')
 exported=dict(category=re.search(r'var type := "([^"]+)"',script)[1],key=re.search(r'var key := "([^"]+)"',script)[1],enabled=re.search(r'var enabled := (true|false)',script)[1]=='true',offset=[0,0]);require('var offset := Vector2.ZERO'in script,'Prompt source default offset')
 out=[]
 for rec in records:
  p=rec['overrides'];category=p.get('type',exported['category']);key=p.get('key',exported['key']);enabled=p.get('enabled',exported['enabled']);offset=p.get('offset',exported['offset']);parent=rec['node'].rsplit('/',1)[0]
  require(category in ('Objects','NPCs') and key in ('ui_accept','ui_select','ui_toggle'),'Unreviewed prompt category/action')
  require(parent in nodes and type(enabled)is bool and len(offset)==2 and all(type(v)in(int,float)and math.isfinite(v)for v in offset),'Prompt parent/offset')
  out.append(dict(id=rec['stable_id'],parent_id=stable(parent),node=rec['node'],ready=rec['ready_ordinal'],category=choices.index(category),key=key,enabled=enabled,offset=offset))
 clips=source_clips(text)
 for path,h in proof['source_files'].items():ex.data(path);require(ex.sources[path]==h,'Changed prompt native closure')
 return dict(schema=1,kind='encore.field-prompts.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),script_sha256=ex.sources[SCRIPT],source_sha256=ex.sources[SCENE],sources=dict(sorted(ex.sources.items())),proof=proof,art=art,initial=initial,choices=choices,records=out,clips=sorted(clips,key=lambda c:c['role']),semantics=['146 exact postorder source Ready entries; hide and set_process(false) are retained','Authoritative paused/settings/input/locale/event detector signals; full force/press state machine','AnimationPlayer value/method tracks retain source times, ease and idle clock; no synthetic nearby distance','Current parent scale is read only when source _reset_scale executes; hidden animation remains active'])

def validate(d):
 require(set(d)=={'schema','kind','commit','scene','scene_id','script_sha256','source_sha256','sources','proof','art','initial','choices','records','clips','semantics'},'Prompt IR fields rejected')
 require(d['schema']==1 and d['kind']=='encore.field-prompts.source-ir' and d['commit']==PIN and d['scene']==SCENE and d['scene_id']==stable('.'),'Prompt source identity')
 ex=Extractor(ROOT)
 for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Prompt source fingerprint '+p)
 require(d['art']==art_binding(ex),'Prompt source GPU/layout binding changed')
 require(d['clips']==source_clips(ex.text(SOURCE)),'Prompt source animation semantics changed')
 require(d['initial'][2]==d['art']['label_local']+[0,0],'Prompt Container actual layout changed')
 require(d['script_sha256']==d['sources'][SCRIPT] and d['source_sha256']==d['sources'][SCENE],'Prompt embedded proof')
 require(len(d['initial'])==10 and all(len(v)==4 and all(math.isfinite(n)for n in v)for v in d['initial']),'Prompt initial Canvas values')
 require(len(d['choices'])==4 and set(d['choices'])=={'Objects','NPCs','Both','None'},'Prompt settings')
 roster={p['stable_id']:p for p in read(ROOT/'content/podunk-scene.json')['pending']if p['script']==SCRIPT};seen=set();last=-1
 for r in d['records']:
  require(set(r)=={'id','parent_id','node','ready','category','key','enabled','offset'},'Prompt record fields rejected')
  require(r['id']in roster and r['id']not in seen and r['id']==stable(r['node']) and r['parent_id']==stable(r['node'].rsplit('/',1)[0]) and r['node']==roster[r['id']]['node'] and r['ready']==roster[r['id']]['ready_ordinal'] and last<r['ready'],'Prompt Ready binding');seen.add(r['id']);last=r['ready']
  require(r['category']<4 and d['choices'][r['category']]in ('Objects','NPCs') and r['key']in ('ui_accept','ui_select','ui_toggle')and type(r['enabled'])is bool and len(r['offset'])==2 and all(math.isfinite(v)and abs(v)<=100000 for v in r['offset']),'Prompt exported values')
 require(len(seen)==len(roster)==146 and {c['role']for c in d['clips']}=={1,2,3,4,5},'Prompt coverage')
 for c in d['clips']:
  require(set(c)=={'role','name','length','loop','tracks'} and c['name']=={1:'Float',2:'Hide',3:'Press',4:'RESET',5:'Show'}.get(c['role']) and c['loop']==(c['role']==1),'Prompt clip fields/role rejected')
  require(0<c['length']<=10 and type(c['loop'])is bool and 0<len(c['tracks'])<=16,'Prompt clip bounds');roles=set()
  for t in c['tracks']:
   require(set(t)=={'property','update','keys'} and (t['property'] not in (7,11) or t['update']==1),'Prompt track fields rejected')
   require(t['property']in range(1,12) and t['property']not in roles and t['update']in (0,1) and 0<len(t['keys'])<=64,'Prompt track bounds');roles.add(t['property']);last=-1
   for k in t['keys']:
    require(set(k)=={'time','ease','value'},'Prompt key fields rejected')
    require(last<=k['time']<=c['length'] and math.isfinite(k['ease'])and len(k['value'])==4 and all(math.isfinite(v)and abs(v)<=100000 for v in k['value']),'Prompt key');last=k['time']
 return d

def encode(d):
 validate(d);out=bytearray(80)
 def u(*v):out.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):out.extend(struct.pack('<'+'f'*len(v),*v))
 def s(v):b=v.encode();u(len(b));out.extend(b)
 out.extend(bytes.fromhex(d['source_sha256']));u(d['scene_id'])
 for v in d['initial']:f(*v)
 a=d['art'];s(a['path']);out.extend(bytes.fromhex(a['sha256']));u(a['bytes'],a['crc32'],a['width'],a['height'],*a['label_crop'],*a['arrow_crop']);f(*a['label_origin'],*a['arrow_origin']);s(a['label']);out.extend(bytes.fromhex(a['layout_sha256']));out.extend(bytes.fromhex(a['engine_hash']))
 for name in d['choices']:s(name)
 u(len(d['records']))
 for r in d['records']:u(r['id'],r['parent_id'],r['ready'],r['category'],int(r['enabled']));s(r['node']);s(r['key']);f(*r['offset'])
 u(len(d['clips']))
 for c in d['clips']:
  u(c['role']);s(c['name']);f(c['length']);u(int(c['loop']),len(c['tracks']))
  for t in c['tracks']:
   u(t['property'],t['update'],len(t['keys']))
   for k in t['keys']:f(k['time'],k['ease'],*k['value'])
 struct.pack_into('<8s4I20s32sI',out,0,b'ENCFPR01',1,len(out),zlib.crc32(out[80:]),1,bytes.fromhex(PIN),bytes.fromhex(d['script_sha256']),0)
 return bytes(out)

def stage_files(source_root):
 d=read(IR);raw=encode(d);root=Path(source_root);require((root/'data/podunk-prompts.encfieldprompt').read_bytes()==raw,'Stale prompt pack');a=d['art'];texture=(root/a['path']).read_bytes();require(hashlib.sha256(texture).hexdigest()==a['sha256'],'Staged prompt texture changed');return {Path('data/podunk-prompts.encfieldprompt'):raw,Path(a['path']):texture}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);a=p.parse_args()
 if a.action=='extract':
  require(a.native and a.source,'Explicit source exports required');records,nodes,_,proof=source_instances(a.native,a.source,ROOT/'upstream/MOTHER-Encore');d=build(records,nodes,proof);validate(d);write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,script=SCRIPT,sources=d['sources'],ir_sha256=sha(IR),scope=d['semantics'],scene_admitted=False));return
 d=read(IR);review=read(REVIEW);require(review['ir_sha256']==sha(IR)and review['sources']==d['sources']and review['commit']==PIN,'Prompt review changed');raw=encode(d)
 if a.action=='verify':require(PACK.read_bytes()==raw,'Stale prompt binary')
 else:PACK.write_bytes(raw)
 print('Field ButtonPrompt binary:',len(raw),'bytes; 146 actual Ready records')
if __name__=='__main__':
 try:main()
 except (ValueError,KeyError,OSError,TypeError)as e:print('FIELD PROMPT ERROR:',e,file=sys.stderr);raise SystemExit(1)
