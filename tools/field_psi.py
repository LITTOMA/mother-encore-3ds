#!/usr/bin/env python3
"""Pinned original field PSI menu and Player Telepathy source slice, offline IR."""
from __future__ import annotations
import argparse,csv,io,json,hashlib,re,struct,sys,zlib,math,subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,node,properties,animation,require
from tools.asset_receipts import receipt_path
IR=ROOT/'content/native-field-psi.json';PACK=ROOT/'romfs/data/field.encpsi';REVIEW=ROOT/'reports/field-psi/source-review.json'
UI='Nodes/Ui/PSIMenu/PSIMenuUI.tscn'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,d):p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes((json.dumps(d,ensure_ascii=False,indent=2)+'\n').encode())
def ident(s):return int.from_bytes(hashlib.sha256(('field-psi:'+s).encode()).digest()[:4],'little')
def table(ex,p):return{r['key']:r for r in csv.DictReader(io.StringIO(ex.text(p)))}
def build():
 ex=Extractor(ROOT);source={p:ex.text(p)for p in [UI,'Nodes/Ui/PSISelect.tscn','Nodes/Ui/PSISkillLine.tscn','Nodes/Ui/InventorySelect.tscn','Nodes/Ui/Inventory/InventorySelect.gd','Nodes/Ui/Inventory/portrait.tscn','Nodes/Ui/Inventory/portrait.gd','Nodes/Ui/HighlightLabel.tscn','Nodes/Ui/HighlightLabel.gd','Nodes/Ui/CostLabel.tscn','Scripts/UI/CostLabel.gd','Nodes/Ui/Reusables/Scrollbar.tscn','Scripts/UI/Reusables/Scrollbar.gd','Nodes/Ui/arrow.tscn','Scripts/UI/cursor.gd','Scripts/UI/PSIMenuUI.gd','Scripts/UI/PSISelect.gd','Scripts/UI/PSISkillLine.gd','Scripts/UI/TargetCharacter.gd','Scripts/UI/Pausemenu.gd','Scripts/Main/party/Player.gd','Scripts/Main/npc.gd','Scripts/global/uiManager.gd','Scripts/global/PartyMember.gd','Scripts/global/Character.gd','Scripts/global/global.gd','Scripts/global/text_tools.gd','project.godot','Data/save_new_game.yaml','Fonts/EBMain_la.tres','Fonts/BottleRocket.tres','Scripts/Main/Status.gd','Scripts/global/audioManager.gd','LICENSE']}
 ui=source[UI];ps=source['Nodes/Ui/PSISelect.tscn'];line=source['Nodes/Ui/PSISkillLine.tscn'];tabs=source['Nodes/Ui/InventorySelect.tscn'];cost=source['Nodes/Ui/CostLabel.tscn'];script=source['Scripts/UI/PSIMenuUI.gd'];player=source['Scripts/Main/party/Player.gd'];pm=source['Scripts/global/PartyMember.gd']
 require('set_pp(_current_character.get_pp() - _skill.get("pp_cost", 0))'in script and '_close(true)'in script and 'close_commands_menu(false, false, true)'in script and '_find_telepathy_target(collide)'in player and 'parent.has_method("telepathy")'in player,'Changed source use/target order')
 names=dict(re.findall(r'^const ([A-Z_]+)\s*:?=\s*"([^"]+)"',pm,re.M));raw=re.search(r'POSSIBLE_PLAYABLE_MEMBERS := \[([^\]]+)\]',source['Scripts/global/global.gd'])[1];order=[names[k]for k in re.findall(r'PartyMember\.([A-Z_]+)',raw)];require(len(order)==5,'Source canonical party')
 menus=table(ex,'Translations/TranslatedText/menus - sheet.csv');skills_table=table(ex,'Translations/TranslatedText/battleskills - sheet.csv')
 translations={**menus,**skills_table}
 def tr(key,col):require(key in translations and col in translations[key],'Missing source translation '+key);return translations[key][col]
 entries=[]
 for path in sorted(p for p in ex.inventory['files']if p.startswith('Data/BattleSkills/')and p.endswith('.yaml')):
  d=ex.yaml(path);key=Path(path).stem;visible=d.get('skill_type')=='psi'and d.get('use_cases',1)<=0
  op=1 if key=='telepathy'else 2 if key=='lifeUpA'else 3 if visible else 0
  if op==2:require(d['action_type']==1 and d.get('value_type','normal')=='normal'and d['traits']==['target_incapacitated'],'Changed field Lifeup effect')
  if op==1:require(d['target_type']==5 and d.get('pp_cost',0)==0 and d['use_cases']==-1,'Changed Telepathy PSI dispatch')
  name=d.get('name')or'';desc=d.get('description')or''
  entries.append(dict(id=ident(key),source=path,key=key,name_key=name,name_en=tr(name,'en')if visible else'',name_zh=tr(name,'zh_CN')if visible else'',desc_en=tr(desc,'en')if visible else'',desc_zh=tr(desc,'zh_CN')if visible else'',operation=op,level=d.get('level',-1),target=d.get('target_type',0)if visible else 0,pp=d.get('pp_cost',0)if visible else 0,heal=d.get('damage_or_heal',0)if op==2 else 0,variance=d.get('variance',0)if op==2 else 0,iq_divisor=int(re.search(r'Character.IQ\) / (\d+)',script)[1])if op==2 else 0,target_unconscious='target_unconscious'in d.get('traits',[]),target_incapacitated='target_incapacitated'in d.get('traits',[])))
 items=[]
 item_script=ex.text('Scripts/global/Item.gd');require('return _has_function("equip")'in item_script and '_learned_skills + _get_battle_skills_from_inv()'in pm and 'if item.equipped or !item.is_equippable():'in pm and 'get_id() in item_data.get("can_use", [])'in pm,'Changed source inventory skill grants')
 for path in sorted(p for p in ex.inventory['files']if p.startswith('Data/Items/')and p.endswith('.yaml')):
  item=ex.yaml(path);grant=item.get('enable_skill','');users=item.get('can_use',[]);require(type(grant)is str and type(users)is list and (not grant or any(s['key']==grant for s in entries)),'Unknown source inventory skill')
  items.append(dict(source=path,key=Path(path).stem,skill=grant,equippable=any(a.get('function')=='equip'for a in item.get('actions',[])),users=users))
 statuses=[]
 for p in sorted(p for p in ex.inventory['files']if p.startswith('Data/StatusAilments/')and p.endswith('.yaml')):
  d=ex.yaml(p);key=Path(p).stem;require(type(d)is dict,'Source ailment dictionary')
  statuses.append(dict(id=key,forgetful=key==re.search(r'_current_character.has_status\("([^"]+)"\)',script)[1],unconscious=key=='unconscious',incapacitated=bool(d.get('effects_by_char',{}).get('any',{}).get('incapacitated',False))))
 tele=ex.yaml('Data/FieldSkills/telepathy.yaml');require(not tele.get('flag') and tele['battle_skill']=='telepathy','Telepathy field gating');users=[k for k,v in tele['usable'].items()if v]
 vw=int(re.search(r'window/size/width=(\d+)',source['project.godot'])[1]);vh=int(re.search(r'window/size/height=(\d+)',source['project.godot'])[1]);dx=(400-vw)/2;dy=(240-vh)/2
 def rect(p,w=vw,h=vh):x=p.get('anchor_left',0)*w+p.get('margin_left',0);y=p.get('anchor_top',0)*h+p.get('margin_top',0);return[x+dx,y+dy,p.get('anchor_right',0)*w+p.get('margin_right',0)-x,p.get('anchor_bottom',0)*h+p.get('margin_bottom',0)-y]
 panel=rect(node(ui,'PSIMenu/PSISelect'));margin=node(ps,'MarginContainer');row=node(line,'.');hbox=node(line,'HBox');first=node(line,'HBox/Alpha');second=node(line,'HBox/Beta');desc_parent=rect(node(ui,'PSIMenu/Description'));desc=node(ui,'PSIMenu/Description/Desc');desc_rect=rect(desc,desc_parent[2],desc_parent[3]);desc_rect[0]+=desc_parent[0]-dx;desc_rect[1]+=desc_parent[1]-dy
 target=node(ui,'PSIMenu/TargetCharacterMenu/PanelContainer');title=node(ui,'PSIMenu/TargetCharacterMenu/PanelContainer/VBoxContainer/ToWhomLabel');targetmargin=node(ui,'PSIMenu/TargetCharacterMenu/PanelContainer/VBoxContainer/MarginContainer');targetrow=node(ui,'PSIMenu/TargetCharacterMenu/PanelContainer/VBoxContainer/MarginContainer/VBoxContainer/CharaLabel2');patch=properties(re.sub(r'Rect2\(\s*([^)]*)\)',lambda m:'['+m[1]+']',re.search(r'\[sub_resource type="StyleBoxTexture" id=10\]\n(.*?)(?=^\[)',ui,re.M|re.S)[1]));bar=rect(node(tabs,'.'));bar[2]=panel[2];bar[3]=max(bar[3],node(tabs,'.')['patch_margin_top']);titlebox=node(tabs,'CenterContainer');menuname=node(tabs,'CenterContainer/MenuName');costroot=node(ps,'CostLabel');costbox=node(cost,'CostBox');costlabel=node(cost,'CostBox/HBoxContainer/Label');costvalue=node(cost,'CostBox/HBoxContainer/Label2');cursor=node(source['Nodes/Ui/arrow.tscn'],'.')
 inside_style=properties(re.sub(r'Rect2\(\s*([^)]*)\)',lambda m:'['+m[1]+']',re.search(r'\[sub_resource type="StyleBoxTexture" id=2\]\n(.*?)(?=^\[)',tabs,re.M|re.S)[1]));cost_style=properties(re.sub(r'Rect2\(\s*([^)]*)\)',lambda m:'['+m[1]+']',re.search(r'\[sub_resource type="StyleBoxTexture" id=4\]\n(.*?)(?=^\[)',cost,re.M|re.S)[1]));cursor_size=[float(x)for x in re.search(r'_cursor_size := Vector2\(([^)]+)',source['Scripts/UI/cursor.gd'])[1].split(',')];tab_portraits=node(tabs,'CharacterPortraits');portrait_root=node(source['Nodes/Ui/Inventory/portrait.tscn'],'.');cost_hbox=node(cost,'CostBox/HBoxContainer');divider=node(ui,'PSIMenu/Description/Desc/Divider');target_title_source=source['Nodes/Ui/HighlightLabel.tscn'];target_title_height=node(target_title_source,'.')['rect_min_size'][1]
 layouts=[panel,[margin['custom_constants/margin_left'],margin['custom_constants/margin_top'],panel[2]-margin['custom_constants/margin_left']-margin['custom_constants/margin_right'],row['rect_min_size'][1]],[hbox['margin_left'],hbox['margin_top'],0,0],desc_rect,[desc_rect[0]+(desc_rect[2]-divider['rect_min_size'][0])/2,desc_rect[1]+divider['margin_top'],*divider['rect_min_size']],[target['margin_left']+dx,target['margin_top']+dy,target['margin_right']-target['margin_left'],0],[target['margin_left']+dx,target['margin_top']+dy,title['margin_right'],target_title_height],[targetmargin['custom_constants/margin_left'],target_title_height+targetmargin['custom_constants/margin_top'],targetrow['margin_top'],45],bar,[bar[0]+bar[2]+titlebox['margin_left']+menuname['margin_left'],bar[1]+titlebox['margin_top']+menuname['margin_top'],menuname['margin_right']-menuname['margin_left'],menuname['margin_bottom']-menuname['margin_top']],[tab_portraits['margin_left'],tab_portraits.get('margin_top',0),node(tabs,'CharacterPortraits/Ana')['margin_left'],portrait_root['margin_bottom']],[panel[0]+panel[2]+costroot['margin_left']+costbox['margin_left'],panel[1]+panel[3]+costroot['margin_top']+costbox['margin_top'],costbox['margin_right']-costbox['margin_left'],costbox['margin_bottom']-costbox['margin_top']],[cost_hbox['margin_left']+costlabel.get('margin_left',0),costlabel['margin_top'],costlabel['margin_right']-costlabel.get('margin_left',0),costlabel['margin_bottom']-costlabel['margin_top']],[cost_hbox['margin_left']+costvalue['margin_left'],costvalue['margin_top'],costvalue['margin_right']-costvalue['margin_left'],costvalue['margin_bottom']-costvalue['margin_top']],cursor.get('cursor_offset',[-8,0])+[0,0],[-cursor_size[0]/6,cursor_size[1]/2,*cursor_size],[node(ps,'.')['patch_margin_left'],node(ps,'.')['patch_margin_top'],node(ps,'.')['patch_margin_right'],node(ps,'.')['patch_margin_bottom']],[patch['margin_left'],patch['margin_top'],patch['margin_right'],patch['margin_bottom']],[patch['expand_margin_left'],patch['expand_margin_top'],0,0],[node(tabs,'.')['patch_margin_left'],node(tabs,'.')['patch_margin_top'],node(tabs,'.')['patch_margin_right'],node(tabs,'.').get('patch_margin_bottom',0)],[cost_style[k]for k in ['margin_left','margin_top','margin_right','margin_bottom']],[vw,vh,0,0],[400,240,0,0],[row['rect_min_size'][1]+4,0,0,0],[second['margin_left'],first['rect_min_size'][0],row['rect_min_size'][1],0],[node(ui,'PSIMenu/PSISelect')['linesPerPage'],3,desc['max_lines_visible'],12],[target_title_height,targetmargin['custom_constants/margin_bottom'],node(ui,'PSIMenu/TargetCharacterMenu/PanelContainer/VBoxContainer/MarginContainer/VBoxContainer')['custom_constants/separation'],0],list(node(ui,'PSIMenu/Description/Desc/Divider')['color']),list(title['custom_colors/font_color']),[bar[0]+bar[2]+titlebox['margin_left'],bar[1]+titlebox['margin_top'],titlebox['margin_right']-titlebox['margin_left'],titlebox['margin_bottom']-titlebox['margin_top']],[inside_style[k]for k in ['margin_left','margin_top','margin_right','margin_bottom']],list(properties(re.search(r'\[sub_resource type="StyleBoxFlat" id=1\]\n(.*?)(?=^\[)',ui,re.M|re.S)[1])['bg_color']),[inside_style[k]for k in ['expand_margin_left','expand_margin_top','expand_margin_right','expand_margin_bottom']]]
 # Source row default VBox separation 4 is the Godot 3 default theme constant,
 # recorded explicitly for this source container with no custom override.
 levels=list(re.search(r'LEVEL_NAMES = "([^"]+)"',source['Scripts/UI/PSISkillLine.gd'])[1]);locales=[]
 for code,col in [('en','en'),('zh_Hans_CN','zh_CN')]:locales.append(dict(code=code,title=tr('MENU_TITLE_PSI',col),whom=tr('PSI_WHOM',col),pp=tr('MENU_PP',col),insufficient=tr('PSI_PP_NOTENOUGH',col),forgetful=re.search(r'_update_desc_label\("(Ninten[^\"]+)"\)',script)[1],hp_max=tr('ACTION_RESULT_HP_MAX',col),hp_up=tr('ACTION_RESULT_HP_UP',col)))
 sounds=[re.search(r'"'+key+r'": load\("res://([^"\n]+)"\)',source['Scripts/global/audioManager.gd'])[1]for key in ['menu_open','menu_close','cursor1','cursor2','back','restricted']]+[re.search(r'"lifeup_a": load\("res://([^"]+)"',script)[1]]
 for sound in sounds:ex.data(sound);ex.data(sound+'.import')
 effectsource=source['Scripts/global/uiManager.gd'];body=effectsource.split('func set_telepathy_effect')[1].split('\nfunc ')[0];color=[float(v)for v in re.search(r'set_color\(Color\(([^)]+)',body)[1].split(',')];cut,duration,transition=[float(v)for v in re.search(r'set_cut\(([\d.]+), ([\d.]+), ([\d.]+), Tween.EASE_OUT',body).groups()];spin=float(re.search(r'set_spin\(true, ([\d.]+)',body)[1])
 fallbacks=[re.search(r'if !_can_use_telepathy\(\):\n\s*uiManager.open_dialogue_box\("([^"]+)"',player)[1],re.search(r'func use_telepathy\(\):.*?if collide == null:\n\s*uiManager.open_dialogue_box\("([^"]+)"',player,re.S)[1],'Reusable/nothoughts','Reusable/straythoughts']
 for p in fallbacks:ex.text('Data/Dialogue/'+p+'.yaml')
 assets=[]
 def asset(role,path):size=ex.png_size(path);assets.append(dict(role=role,source=path,path='graphics/menus/psi/'+str(role)+'.t3x',width=size[0],height=size[1]));return role
 for role,path in [(1,'Graphics/UI/Inventory/character-bar.png'),(2,'Graphics/UI/Overworld/flavours/defaultbox_title.png'),(3,'Graphics/UI/Overworld/flavours/defaultlabel.png')]:asset(role,path)
 portraits=[]
 for i,char in enumerate(order):portraits.append(dict(character=char,normal=asset(10+i*2,'Graphics/UI/Inventory/characters/'+char+'.png'),highlight=asset(11+i*2,'Graphics/UI/Inventory/characters/'+char+'_hl.png')))
 clips=[]
 for name in ['Open','Close']:
  ref=node(ui,'AnimationPlayer')['anims/'+name];a=animation(ui,ref['SubResource'],UI,name);t=a['tracks'][0];require(t['path']=='PSIMenu:rect_position'and t['keys']['update']==0 and not a['loop'],'Changed PSI menu animation');clips.append(dict(length=a['length'],keys=[dict(time=v,value=k[1],ease=e)for v,k,e in zip(t['keys']['times'],t['keys']['values'],t['keys']['transitions'])]))
 ca=animation(cost,node(cost,'AnimationPlayer')['anims/Show']['SubResource'],'Nodes/Ui/CostLabel.tscn','Show');ct=ca['tracks'][1]['keys'];cv=ca['tracks'][0]['keys'];cost_animation=dict(length=ca['length'],visible_after=cv['times'][0],base_y=costbox['margin_top'],keys=[dict(time=t,value=v,ease=e)for t,v,e in zip(ct['times'],ct['values'],ct['transitions'])])
 return dict(schema=1,kind='encore.field-psi.source-ir',commit=PIN,skills=entries,items=items,statuses=statuses,order=order,telepathy_users=users,telepathy_skill=tele['battle_skill'],locales=locales,layouts=layouts,levels=levels,sounds=sounds,animations=clips,cost_animation=cost_animation,source_font='Fonts/EBMain_la.tres',number_font='Fonts/BottleRocket.tres',message_delay=float(re.search(r'create_timer\(([\d.]+)\)',script)[1]),effect=dict(color=color,cut=cut,duration=duration,restore_cut=float(re.search(r'else:\n\s*_fade.set_cut\(([\d.]+),',body)[1]),transition=int(transition),ease_in=0,ease_out=1,spin=spin),fallbacks=fallbacks,portraits=portraits,assets=assets,sources=dict(sorted(ex.sources.items())),semantics=['Source PSISelect learned+inventory skills, PSI use_cases <=0, name groups and sorted level cells; Telepathy and LifeUpA typed execution only','First natural party caster with MAXPP>0, PP and forgetful selection, canonical target list with source unconscious/incapacitated policy','LifeUpA source PP debit before target changes, normal IQ integer division and one global randf per target, HP clamp but messages from unclamped new HP','Telepathy source silent close/unpause, actual Player learned field skill and cached EventDetector, parent-first method lookup, source thoughts/effect/prompt ordering','Source opening accepts input immediately; closing back signal immediate; description 3/4 rows from actual font wrap; 0.5s FIFO feedback timers','Original untranslated forgetful English literal retained in both locales; source assets admitted by pinned LICENSE; no JSON packaged'],unsupported=['Other field-visible PSI skill execution rejects explicitly','Telepathy Host must admit current NPC thought programme, world effect, real prompt and loaded source dialogue before invocation','Field PSI does not grant Telepathy at level1: source Ninten level2 table remains session progression owner','Godot default VBox separation4 adapter explicitly recorded; reflective/foreign game scenes not assumed','Visible multi-caster source Indicator.gd and visible Scrollbar child lifecycle require the next UI capability; current admitted one-member/two-skill field slice keeps them source-hidden'],unverified=['Manual tests not run','3DS renderer/art conversion and integrated world/Session wiring pending'])
def validate(d):
 require(d['schema']==1 and d['kind']=='encore.field-psi.source-ir'and d['commit']==PIN and len(d['layouts'])==33 and len(d['order'])==5 and len(d['locales'])==2 and len(d['levels'])==4 and len(d['sounds'])==7 and len(d['fallbacks'])==4 and 0<d['message_delay']<=60,'PSI schema/topology')
 ids=set();keys=set()
 for s in d['skills']:
  require(s['id']==ident(s['key'])and s['id']not in ids and s['key']not in keys and s['operation']in(0,1,2,3)and -1<=s['level']<=3 and all(type(s[k])is int and 0<=s[k]<=1000000 for k in ['target','pp','heal','variance','iq_divisor']),'PSI skill identity/numeric policy');ids.add(s['id']);keys.add(s['key'])
 require(sum(s['operation']==1 for s in d['skills'])==sum(s['operation']==2 for s in d['skills'])==1,'PSI reviewed operations')
 require(all(len(r)==4 and all(type(x)in(int,float)and math.isfinite(x)and abs(x)<=1000000 for x in r)for r in d['layouts']),'PSI layout finite')
 for a in d['animations']:require(0<a['length']<=10 and 0<len(a['keys'])<=32 and all(0<=k['time']<=a['length']and all(math.isfinite(k[n])for n in ['time','value','ease'])for k in a['keys']),'PSI animation')
 return d
def load():
 d=validate(json.loads(IR.read_text(encoding='utf-8')));review=json.loads(REVIEW.read_text(encoding='utf-8'));require(review['commit']==PIN and review['ir_sha256']==sha(IR)and review['sources']==d['sources'],'PSI review receipt');ex=Extractor(ROOT)
 for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Changed PSI source '+p)
 return d
def encode(d):
 validate(d);b=bytearray(64)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def s(v):raw=v.encode();u(len(raw));b.extend(raw)
 f(d['message_delay']);e=d['effect'];f(*e['color'],e['cut'],e['duration'],e['spin'],e['restore_cut']);u(e['transition'],e['ease_in'],e['ease_out']);s(d['telepathy_skill']);s(d['source_font']);s(d['number_font'])
 for array in[d['order'],d['telepathy_users'],d['levels'],d['sounds'],d['fallbacks']]:u(len(array));[s(v)for v in array]
 u(len(d['locales']))
 for l in d['locales']:
  for k in ['code','title','whom','pp','insufficient','forgetful','hp_max','hp_up']:s(l[k])
 u(len(d['statuses']))
 for t in d['statuses']:s(t['id']);u(int(t['forgetful'])|int(t['unconscious'])<<1|int(t['incapacitated'])<<2)
 u(len(d['skills']))
 for t in d['skills']:
  u(t['id'],t['operation'],t['level']&0xffffffff,t['target'],t['pp'],t['heal'],t['variance'],t['iq_divisor'],int(t['target_unconscious'])|int(t['target_incapacitated'])<<1)
  for k in ['source','key','name_key','name_en','name_zh','desc_en','desc_zh']:s(t[k])
 u(len(d['items']))
 for t in d['items']:
  s(t['source']);s(t['key']);s(t['skill']);u(int(t['equippable']),len(t['users']));[s(v)for v in t['users']]
 u(len(d['layouts']))
 for l in d['layouts']:f(*l)
 for a in d['animations']:
  f(a['length']);u(len(a['keys']))
  for k in a['keys']:f(k['time'],k['value'],k['ease'])
 a=d['cost_animation'];f(a['length'],a['visible_after'],a['base_y']);u(len(a['keys']))
 for k in a['keys']:f(k['time'],k['value'],k['ease'])
 receipt=receipt_path(ROOT/'romfs/graphics/menus/psi',ROOT);out={}
 if receipt.exists():
  receipt_data=json.loads(receipt.read_text());require(receipt_data['commit']==PIN and receipt_data['recipe_sha256']==sha(IR),'Stale PSI texture recipe receipt');out=receipt_data['outputs']
 u(len(d['assets']))
 for a in d['assets']:
  r=out.get(a['path']);raw=(ROOT/'romfs'/a['path']).read_bytes()if r else b''
  if r:require(r['sha256']==hashlib.sha256(raw).hexdigest()and r['bytes']==len(raw),'Stale PSI art receipt')
  u(a['role'],a['width'],a['height'],len(raw),zlib.crc32(raw)&0xffffffff if raw else 0);s(a['source']);s(a['path'])
 u(len(d['portraits']))
 for p in d['portraits']:s(p['character']);u(p['normal'],p['highlight'])
 u(len(d['sources']))
 for p,h in d['sources'].items():s(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20s12x',b,0,b'ENCPSI01',1,len(b),0,1,1,len(d['skills']),bytes.fromhex(PIN));struct.pack_into('<I',b,16,zlib.crc32(b)&0xffffffff);return bytes(b)
def assets(d,tex):
 def convert(a):
  target=ROOT/'romfs'/a['path'];target.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(tex),'-f','rgba8','-z','none','-o',str(target),str(ROOT/'upstream/MOTHER-Encore'/a['source'])],check=True);return a['path'],dict(bytes=target.stat().st_size,sha256=sha(target))
 with ThreadPoolExecutor(max_workers=4)as workers:outputs=dict(workers.map(convert,d['assets']))
 write(receipt_path(ROOT/'romfs/graphics/menus/psi',ROOT),dict(schema=1,commit=PIN,recipe_sha256=sha(IR),outputs=outputs,tex3ds_sha256=sha(tex)))
def font_bindings():
 d=load();texts=[]
 for l in d['locales']:
  texts.extend(l[k]for k in ['title','whom','insufficient','forgetful','hp_max','hp_up'])
 for s in d['skills']:
  if s['operation']:texts.extend(s[k]for k in ['name_en','name_zh','desc_en','desc_zh'])
 texts.extend(d['levels']);return{d['source_font']:texts,d['number_font']:[l['pp']for l in d['locales']]+[str(s['pp'])for s in d['skills']if s['operation']]}

def skin_paths():return[a['path']for a in load()['assets']]

def audio_bindings():
 d=load();source=d['sounds'][-1];return[dict(source=source,identity=dict(kind='stable',value=ident(source)),pcm='sound/effects/field-psi-heal.pcm',gain_db=0,conversion=None)]
def stage_files(source):
 d=load();raw=encode(d);require((Path(source)/'data/field.encpsi').read_bytes()==raw,'Stale PSI binary');out={Path('data/field.encpsi'):raw}
 for a in d['assets']:
  p=Path(source)/a['path'];require(p.exists(),'Unbuilt PSI GPU asset');raw=p.read_bytes();require(raw==(ROOT/'romfs'/a['path']).read_bytes(),'Different staged PSI GPU asset');out[Path(a['path'])]=raw
 return out
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','assets','verify']);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
 if a.action=='extract':d=validate(build());write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],semantics=d['semantics'],unsupported=d['unsupported'],unverified=d['unverified']));print('Field PSI source:',len(d['skills']),'source skill categories; Telepathy + LifeUpA; source menu/target/effect');return
 d=load()
 if a.action=='assets':require(a.tex3ds and a.tex3ds.is_file(),'Real tex3ds required');assets(d,a.tex3ds);return
 raw=encode(d)
 if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
 else:require(PACK.read_bytes()==raw,'Stale PSI binary')
 print('Field PSI:',len(raw),'checked bytes; source graphics receipt required at GPU admission')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,OSError,TypeError,struct.error,subprocess.SubprocessError)as e:sys.exit('FIELD PSI ERROR: '+str(e))
