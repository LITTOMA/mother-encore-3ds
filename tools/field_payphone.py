#!/usr/bin/env python3
"""Actual Podunk street payphones: source payment/order/timers -> ENCFPH01."""
from __future__ import annotations
import argparse,hashlib,json,math,re,struct,subprocess,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import read,write,decode,stable,SCENE,PIN,require,sha
from tools.extract_battle_entry import Extractor,node,animation
SCRIPT='Maps/Testing/phone.gd';BASE='Scripts/Main/Interact Dialog.gd'
IR=ROOT/'content/native-field-payphone.json';REVIEW=ROOT/'reports/field-payphone/source-review.json';PACK=ROOT/'romfs/data/podunk.encpayphone';RECEIPT=ROOT/'content/asset-receipts/graphics/objects/payphone/source.json'
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
  out.append(dict(stable_id=stable(path),node=path,ready_ordinal=ordinal[path],script=bindings[path],ancestor_bindings=[dict(node=p,script=bindings[p])for p in bindings if path.startswith(p+'/')],overrides=overrides[path],native=names[path],children=[names[p] for p in children[path]]))
 require(len(out)==4,'Podunk Payphone binding loss');return out,names,resources,dict(native_export_sha256=sha(native),source_receipt_sha256=sha(receipt),source_files={name:r['sha256'] for name,r in s['files'].items()})

def build(records,nodes,resources,provenance):
 ex=Extractor(ROOT);script=ex.text(SCRIPT);base=ex.text(BASE);inventory=ex.text('Scripts/global/Inventory.gd');gd=ex.text('Scripts/global/globalData.gd');ex.text('Scripts/global/global.gd');ui=ex.text('Scripts/global/uiManager.gd');ex.text('Scripts/UI/CashBox.gd');ex.text('Nodes/Ui/PhoneUnitsBox.tscn');ex.text('Nodes/Ui/CashBox.tscn');ex.text('LICENSE')
 require(re.findall(r'^func ([A-Za-z_]+)\(',script,re.M)==['_ring','interact','interact_item','_use_phone','_show_amount_box'],'Changed phone methods')
 require(all(v in inventory for v in ('get_item_owner(item).inv.reduce_or_drop_item(item)','if item.doses > 1:','item.doses -= 1','return drop_item(item)','_items.erase(item)','item.equipped = false')),'Unknown actual PhoneCard owner/reduction/drop scope')
 required=['_audio.stream = ResourceLoader.load("res://Audio/Sound effects/phonehangup.wav")','_anim_player.play("Idle")','global.set_phone_location(_save_location)','_show_amount_box(true, true)','_show_amount_box(false, true)','_audio.playing = true','uiManager.open_dialogue_box(_get_right_dialog())','Inventory.reduce_or_drop_item_for_all(phone_card)','uiManager.open_dialogue_box("Reusable/payphonenomoney")'];require(all(v in script for v in required),'Unknown source payphone effect')
 use=script[script.index('func _use_phone('):script.index('func _show_amount_box(')];order=[use.index(v)for v in required[:4]+required[5:8]];require(order==sorted(order),'Payphone source operation order changed')
 cost=int(re.search(r'if globaldata.cash >= (\d+):',script)[1]);require(int(re.search(r'globaldata.cash -= (\d+)',script)[1])==cost,'Payphone source threshold/spend mismatch');timers=[float(v)for v in re.findall(r'create_timer\(([^)]+)\)',script)];require(len(timers)==2,'Phone timer source surface')
 card=re.search(r'find_item_for_all\("([^"]+)"\)',script)[1];item=ex.yaml('Data/Items/'+card+'.yaml');require(item['keyitem']is False and item['transform']=='' and item['slot']=='','Unsupported card transform/key/equipment');step=int(re.search(r'item.doses -= (\d+)',inventory)[1]);require(step==int(re.search(r'if item.doses > (\d+):',inventory)[1]),'Changed source reduction boundary')
 require('global.connect("flags_updated", self, "_check_flags")'in base and 'visible = globaldata.check_appear_disappear_flags(appear_flag, disappear_flag)'in base and 'if !visible:\n\t\tqueue_free()'in base and 'var ret := dialog'in base and 'ret = flags[1]'in base and 'flag_on = flag_on and !flags.get(disappear_flag, false)'in gd,'Unknown InteractDialog source Ready/dispatch')
 source='Nodes/Reusables/exterior payphone.tscn';scene=ex.text(source);ex.text('Nodes/Reusables/payphone.tscn');idle=animation(scene,node(scene,'AnimationPlayer')['anims/Idle']['SubResource'],source,'Idle');require(not idle['loop']and len(idle['tracks'])==1 and idle['tracks'][0]['path']=='main:frame'and idle['tracks'][0]['keys']['times']==[0.0]and idle['tracks'][0]['keys']['update']==1,'Unknown phone Idle animation');frame=idle['tracks'][0]['keys']['values'][0]
 sound=re.search(r'func _use_phone[^\n]*\n\s*_audio.stream = ResourceLoader.load\("res://([^\"]+)"\)',script)[1];ex.data(sound);ex.data(sound+'.import');banks=[a for a in read(ROOT/'content/native-audio.json')['assets']if a['source_path']=='res://'+sound];require(len(banks)==1 and banks[0]['source_sha256']==ex.sources[sound]and banks[0]['import_sha256']==ex.sources[sound+'.import'],'Phone actual hangup bank source binding rejected');audio=banks[0]
 rows=[];texture=None;bus=None;volume=None
 for rec in records:
  p=rec['node'];v=rec['overrides'];sp=decode(nodes[p+'/main']['properties']);a=decode(nodes[p+'/AudioStreamPlayer2D']['properties']);require(v.get('_is_payphone')is True and v['_key_item']==card,'Unknown street free/noncard phone');require(sp['hframes']==sp['vframes']==1 and sp['frame']==frame and sp['rotation']==0 and sp['centered']and not sp['flip_h']and not sp['flip_v']and not sp['region_enabled']and sp['material']is None,'Unreviewed actual phone Sprite layout');tex=resources[sp['texture']['id']]['path'][6:]
  if texture is None:texture=tex;bus=a['bus'];volume=a['volume_db']
  require(tex==texture and a['bus']==bus and a['volume_db']==volume,'Unsupported per-phone texture/audio variation');dispatch=v.get('_all_dialog',[]);require(type(dispatch)is list and all(type(b)is list and len(b)==2 and all(type(s)is str for s in b)for b in dispatch),'Unknown phone flag dispatch')
  rows.append(dict(id=rec['stable_id'],parent_id=stable(p.rsplit('/',1)[0]),ready_ordinal=rec['ready_ordinal'],sprite_id=stable(p+'/main'),prompt_id=stable(p+'/interact/ButtonPrompt'),audio_id=stable(p+'/AudioStreamPlayer2D'),sprite_flags=int(sp['centered'])|int(sp['flip_h'])<<1|int(sp['flip_v'])<<2,sprite_offset=sp['offset'],node=p,dialogue=v['dialog'],item=card,location=v.get('_save_location',''),appear=v.get('appear_flag',''),disappear=v.get('disappear_flag',''),dispatch=[dict(flag=b[0],program=b[1])for b in dispatch]))
 no_money=re.search(r'open_dialogue_box\("(Reusable/payphonenomoney)"\)',script)[1]
 for program in {no_money}|{r['dialogue']for r in rows}|{b['program']for r in rows for b in r['dispatch']}:ex.yaml('Data/Dialogue/'+program+'.yaml')
 size=ex.png_size(texture);import_text=ex.text(texture+'.import');require(size[0]<=1024 and size[1]<=1024 and 'flags/filter=false'in import_text,'Phone atlas extent/filter');require('return _phone_units if is_phone_units else _cash'in ui,'Unknown amount box routing')
 for p,h in provenance['source_files'].items():ex.data(p);require(ex.sources[p]==h,'Changed complete phone source closure')
 return dict(schema=1,kind='encore.field-payphone.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),scene_sha256=ex.sources[SCENE],script_sha256=ex.sources[SCRIPT],cash_cost=cost,card_name=card,card_max_doses=item['doses'],card_step=step,idle_frame=frame,update_seconds=timers[0],close_seconds=timers[1],no_money_program=no_money,sound=dict(id=audio['stable_id'],source=sound,pcm=audio['pcm_path'],bus=bus,volume=volume),texture=dict(source=texture,output='graphics/objects/payphone.t3x',size=size,columns=sp['hframes'],rows=sp['vframes']),records=rows,sources=dict(sorted(ex.sources.items())),provenance=provenance,semantics=['Four actual exterior payphones; base Ready flags check before synchronous flags_updated registration','PhoneCard party/key inventory search and actual saved UID consumption; storage excluded','Source hangup stream -> Idle -> save location -> box initial update -> audio -> dialogue -> card reduction/cash subtraction','Insufficient cash still schedules amount box but plays no sound and spends no cash','Independent box coroutines update after source 0.5s, then newly created source 1s timer closes while tree paused','Actual exterior phone has no Ring clip; Ring remains unsupported'],unverified=['Tests not run','Main/SceneHost and amount box renderer integration pending','Emulator and hardware unverified'])
def validate(d):
 require(type(d)is dict and set(d)==set('schema kind commit scene scene_id scene_sha256 script_sha256 cash_cost card_name card_max_doses card_step idle_frame update_seconds close_seconds no_money_program sound texture records sources provenance semantics unverified'.split()),'Unknown/missing phone IR field')
 require(type(d['sound'])is dict and set(d['sound'])==set('id source pcm bus volume'.split())and type(d['texture'])is dict and set(d['texture'])==set('source output size columns rows'.split()),'Unknown phone audio/atlas field')
 require(type(d['sound']['id'])is int and d['sound']['id']>0 and math.isfinite(d['sound']['volume'])and -120<=d['sound']['volume']<=24 and len(d['texture']['size'])==2 and all(type(v)is int and 0<v<=1024 for v in d['texture']['size'])and 0<d['texture']['columns']<=1024 and 0<d['texture']['rows']<=1024,'Phone bounded source audio/texture')
 require(d['schema']==1 and d['kind']=='encore.field-payphone.source-ir'and d['commit']==PIN and d['scene']==SCENE and d['scene_id']==stable('.')and len(d['records'])==4,'Phone exact source identity/coverage');require(d['scene_sha256']==d['sources'][SCENE]and d['script_sha256']==d['sources'][SCRIPT]and type(d['cash_cost'])is int and 0<d['cash_cost']<=1000000 and type(d['card_max_doses'])is int and 0<d['card_step']<=d['card_max_doses']<=1000000 and d['idle_frame']==0 and all(math.isfinite(d[k])and 0<d[k]<=60 for k in ('update_seconds','close_seconds')),'Phone source policy/timers');seen=set();last=-1
 for r in d['records']:
  require(type(r)is dict and set(r)==set('id parent_id ready_ordinal sprite_id prompt_id audio_id sprite_flags sprite_offset node dialogue item location appear disappear dispatch'.split()),'Unknown phone descriptor field')
  require(r['id']==stable(r['node'])and r['id']not in seen and r['ready_ordinal']>last and r['item']==d['card_name']and 0<=r['sprite_flags']<8 and len(r['sprite_offset'])==2 and all(math.isfinite(v)and abs(v)<=1000000 for v in r['sprite_offset'])and all(type(r[k])is str for k in ('node','dialogue','item','location','appear','disappear')),'Phone source node/Ready/item');seen.add(r['id']);last=r['ready_ordinal'];require(all(set(b)=={'flag','program'}and all(type(v)is str for v in b.values())for b in r['dispatch']),'Phone flag dispatch')
 return d
def load():
 d=validate(read(IR));r=read(REVIEW);require(r['commit']==PIN and r['ir_sha256']==sha(IR)and r['sources']==d['sources'],'Phone source review stale');ex=Extractor(ROOT)
 for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Changed phone source '+p)
 return d
def receipt(d):
 r=read(RECEIPT);p=ROOT/'romfs'/d['texture']['output'];require(r['schema']==1 and r['commit']==PIN and r['ir_sha256']==sha(IR)and r['producer_sha256']==sha(ROOT/'tools/field_payphone.py')and r['output']==d['texture']['output']and r['bytes']==p.stat().st_size and r['sha256']==sha(p),'Phone genuine texture receipt stale');return r
def encode(d,a):
 validate(d);out=bytearray(64)
 def u(*v):out.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):out.extend(struct.pack('<'+'f'*len(v),*v))
 def s(v):b=v.encode();u(len(b));out.extend(b)
 out.extend(bytes.fromhex(d['scene_sha256']));out.extend(bytes.fromhex(d['script_sha256']));u(d['cash_cost'],d['card_max_doses'],d['card_step'],d['idle_frame'],d['sound']['id']);f(d['update_seconds'],d['close_seconds'],d['sound']['volume']);u(*d['texture']['size'],d['texture']['columns'],d['texture']['rows']);s(d['texture']['source']);s(d['texture']['output']);out.extend(bytes.fromhex(a['sha256']));s(d['card_name']);s(d['no_money_program']);s(d['sound']['source']);s(d['sound']['pcm']);s(d['sound']['bus'])
 for r in d['records']:
  u(*(r[k]for k in ('id','parent_id','ready_ordinal','sprite_id','prompt_id','audio_id','sprite_flags')));f(*r['sprite_offset'])
  for k in ('node','dialogue','item','location','appear','disappear'):s(r[k])
  u(len(r['dispatch']))
  for b in r['dispatch']:s(b['flag']);s(b['program'])
 u(len(d['sources']))
 for p,h in d['sources'].items():s(p);out.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20sII4x',out,0,b'ENCFPH01',1,len(out),0,1,1,len(d['records']),bytes.fromhex(PIN),d['scene_id'],len(d['sources']));struct.pack_into('<I',out,16,zlib.crc32(out)&0xffffffff);return bytes(out)
def compile_pack():
 d=load();raw=encode(d,receipt(d));PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw);return raw
def compile_assets(tool):
 d=load();require(tool and Path(tool).is_file(),'Actual tex3ds required');p=ROOT/'romfs'/d['texture']['output'];p.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(tool),'-f','rgba8','-z','none','-o',str(p),str(ROOT/'upstream/MOTHER-Encore'/d['texture']['source'])],check=True);write(RECEIPT,dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(ROOT/'tools/field_payphone.py'),tex3ds_sha256=sha(tool),output=d['texture']['output'],bytes=p.stat().st_size,sha256=sha(p)));return compile_pack()
def stage_files(source):
 d=load();a=receipt(d);raw=encode(d,a);root=Path(source);p=root/d['texture']['output'];require((root/'data/podunk.encpayphone').read_bytes()==raw and p.stat().st_size==a['bytes']and sha(p)==a['sha256'],'Stale staged payphone resources');return{Path('data/podunk.encpayphone'):raw,Path(d['texture']['output']):p.read_bytes()}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','pack']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
 if a.action=='extract':
  require(a.native and a.source,'Actual native/source exports required');records,nodes,resources,provenance=source_instances(a.native,a.source,ROOT/'upstream/MOTHER-Encore');d=validate(build(records,nodes,resources,provenance));write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],semantics=d['semantics'],unverified=d['unverified']));print('Four actual payphones; source payment and amount-box timers');return
 raw=compile_assets(a.tex3ds)if a.action=='compile'else compile_pack();print('Payphone resource:',len(raw),'bytes')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,OSError,TypeError,struct.error,subprocess.SubprocessError)as e:sys.exit('FIELD PAYPHONE ERROR: '+str(e))

