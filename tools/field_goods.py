#!/usr/bin/env python3
"""Original singleton Ninten+KEY Goods interaction and GPU recipe; offline only."""
from __future__ import annotations
import argparse,csv,io,math,re,struct,subprocess,sys,zlib
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,stable,read,sha,require,write
from tools.extract_battle_entry import Extractor,node,animation
from tools.field_inventory import load as inventory
IR=ROOT/'content/native-field-goods.json';REVIEW=ROOT/'reports/field-goods/source-review.json';PACK=ROOT/'romfs/data/podunk.encgoods'
ASSET=ROOT/'content/field-goods-assets.json';RECEIPT=ROOT/'content/asset-receipts/graphics/ui/field-goods/source.json'
SCENE='Nodes/Ui/Inventory/InventoryUI.tscn'
ASSETS=(('bar','Graphics/UI/Inventory/character-bar.png',None),('inside','Graphics/UI/Overworld/flavours/defaultbox_inside.png',None),('ninten','Graphics/UI/Inventory/characters/ninten.png',None),('ninten-hl','Graphics/UI/Inventory/characters/ninten_hl.png',None),('key','Graphics/UI/Inventory/characters/key.png',None),('key-hl','Graphics/UI/Inventory/characters/key_hl.png',None),('stats','Graphics/UI/Inventory/stats_menu.png',None),('up','Graphics/UI/Inventory/modifiers.png',[3,0,7,9]),('down','Graphics/UI/Inventory/modifiers.png',[16,0,7,9]),('indicator','Graphics/UI/select_arrow.png',None),('scroll-bg','Graphics/UI/Overworld/flavours/defaultbox_inside_scrollbar.png',None),('scroll-thumb','Graphics/UI/Overworld/flavours/defaultscroll.png',None),('cursor0','Graphics/UI/Inventory/cursor.png',[0,0,8,8]),('cursor1','Graphics/UI/Inventory/cursor.png',[8,0,8,8]),('cursor2','Graphics/UI/Inventory/cursor.png',[16,0,8,8]),('equipped','Graphics/UI/Inventory/modifiers.png',[2,19,8,7]))
def extract():
 inv=inventory();ex=Extractor(ROOT)
 for p,h in inv['sources'].items():ex.data(p);require(ex.sources[p]==h,'Changed Inventory dependency '+p)
 scene=ex.text(SCENE);ui=ex.text('Scripts/UI/Inventory/InventoryUI.gd');action=ex.text('Nodes/Ui/Inventory/ActionSelect.gd');target=ex.text('Nodes/Ui/Inventory/TargetCharaSelect.gd');select=ex.text('Nodes/Ui/Inventory/InventorySelect.gd')
 for p in ('Nodes/Ui/Inventory/ConfirmationSelect.gd','Nodes/Ui/Inventory/SortTypeSelect.gd','Nodes/Ui/Inventory/DialogBox.gd','Nodes/Ui/Inventory/StatsBar.gd','Nodes/Ui/Inventory/Stats.gd','Scripts/UI/cursor.gd','Nodes/Ui/arrow.tscn','Nodes/Ui/Inventory/Stats.tscn','Nodes/Ui/InventorySelect.tscn','Nodes/Ui/Inventory/portrait.tscn','Nodes/Ui/Indicator.tscn','Scripts/UI/Indicator.gd','Shaders/MenuFlavors.tres'):ex.text(p)
 require('!is_key_item and global.party.size() > 1'in action and '"storage": include_storage'in select and 'export var include_storage = false'in select,'Changed singleton selector/Give scope')
 require(node(scene,'InventorySelect').get('noKey')is False and 'include_storage'not in node(scene,'InventorySelect'),'Unknown Goods owner scope')
 require('target_type'not in ''.join(ex.text(v['source'])for v in inv['policies']),'New self target operation pending review')
 for t,s in [(ui,'_current_character.inv.switch_items(_sort_source_idx, target_idx)'),(ui,'_current_scroll_pos = index / NB_COLUMNS - NB_ROWS_WITH_DESC + 1'),(action,'current_character.inv.sort_auto()'),(action,'current_character.inv.drop_item(cur_item)'),(action,'_current_char.unequip(_current_item)'),(action,'emit_signal("exit_with_item", _current_item)'),(target,'emit_signal("back", false)')]:require(s in t,'Changed source Goods execution '+s)
 require('dialog'not in ''.join(ex.text(v['source'])for v in inv['policies']) and all(not ex.yaml(v['source']).get('map_for')for v in inv['policies']),'Unimplemented use dialogue/map dispatch')
 layouts={};params={};ex.text('Scripts/UI/Reusables/Scrollbar.gd')
 scroll=ex.text('Nodes/Ui/Reusables/Scrollbar.tscn');scroll_node=node(scene,'Inventory/DescriptionPanel/Scrollbar');params['ScrollRect']=[scroll_node['margin_left'],scroll_node['margin_top'],scroll_node['margin_right']-scroll_node['margin_left'],scroll_node['margin_bottom']-scroll_node['margin_top']]
 for role,path in [('ScrollBG','ScrollBG'),('ScrollThumb','ScrollBG/Thumb')]:
  snode=node(scroll,path);params[role]=[snode.get('margin_left',0),snode.get('margin_top',0),snode.get('margin_right',0),snode.get('margin_bottom',0)];params[role+'Patch']=[snode.get('patch_margin_'+a,0)for a in ('left','top','right','bottom')]
 params['ScrollColor']=node(scroll,'ScrollBG')['self_modulate'];params['ScrollArrow']=[*node(scroll,'UpArrow/arrow')['position'],*node(scroll,'DownArrow/arrow')['position']];params['ScrollRotation']=[node(scroll,'UpArrow/arrow')['rotation'],node(scroll,'DownArrow/arrow')['rotation'],node(scroll,'DownArrow')['margin_top'],0]
 def rect(n):return[n.get('margin_left',0),n.get('margin_top',0),n.get('margin_right',0)-n.get('margin_left',0),n.get('margin_bottom',0)-n.get('margin_top',0)]
 for role,path in [('Confirm','ActionSelect/ConfirmationSelect'),('Sort','ActionSelect/SortTypeSelect'),('Stats','Bottom/StatsBar'),('StatsNumbers','Bottom/StatsBar/StatsLabels'),('StatsTitles','Bottom/StatsBar/StatsNameAndDividers'),('StatsPortrait','Bottom/StatsBar/CenterContainer/CharacterPortrait')]:layouts[role]=rect(node(scene,path))
 for role,path in [('Confirm','ActionSelect/ConfirmationSelect'),('Sort','ActionSelect/SortTypeSelect')]:
  n=node(scene,path);m=node(scene,path+'/MarginContainer');v=node(scene,path+'/MarginContainer/VBoxContainer');cur=node(scene,path+'/arrow')
  params[role+'Patch']=[n['patch_margin_'+s]for s in ('left','top','right','bottom')];params[role+'Inset']=[v['margin_left'],v['margin_top'],m['custom_constants/margin_right'],-m['margin_bottom']];params[role+'Cursor']=[*cur['cursor_offset'],0,0];params[role+'TitleColor']=node(scene,path+('/TitleMarginContainer/ConfirmationLabel'if role=='Confirm'else'/SortTypeLabel'))['modulate']
 scope=node(scene,'Inventory/HBoxContainer');scope_label=node(scene,'Inventory/HBoxContainer/Label');scope_button=node(scene,'Inventory/HBoxContainer/Button');params['ScopeHint']=[scope['margin_left'],scope['margin_top'],scope_button['margin_left']-scope_label['margin_right'],scope_label['margin_bottom']];button=ex.text('Nodes/Ui/ButtonText.tscn');ex.text('Scripts/UI/ButtonText.gd');params['ScopeColor']=node(button,'.')['modulate']
 n=node(scene,'ActionSelect/MarginContainer/VBoxContainer');style=re.search(r'^\[sub_resource type="StyleBoxTexture" id=17\]\n(.*?)(?=^\[)',scene,re.M|re.S)[1];params['ActionPatch']=[float(re.search(r'^margin_'+a+r' = ([-\d.]+)',style,re.M)[1])for a in ('left','top','right','bottom')]
 params['ActionRow']=[n['custom_constants/separation'],node(scene,'ActionSelect/MarginContainer/VBoxContainer/ActionOneLabel')['rect_min_size'][1],0,0];params['ActionCursor']=[*node(scene,'ActionSelect/arrow')['cursor_offset'],0,0]
 t=node(scene,'ActionSelect/TargetCharaSelect/MarginContainer');params['TargetInset']=[t['custom_constants/margin_left'],t['custom_constants/margin_top'],t['custom_constants/margin_right'],t['custom_constants/margin_bottom']-t['margin_bottom']]
 params['Input']=[node(ex.text('Nodes/Ui/arrow.tscn'),'Timer')['wait_time'],float(re.search(r'const TWEEN_LENGTH := ([\d.]+)',ex.text('Scripts/UI/cursor.gd'))[1]),0,0]
 params['GridCancelSound']=[int(node(scene,'Inventory/Arrow').get('cancel_sfx',False)),0,0,0]
 highlight=ex.text('Nodes/Ui/HighlightLabel.gd');require('bg_color:a", 0.5, 0.3).from(0.8)'in highlight,'Changed source blink');params['Highlight']=[0.729412,0.32549,0.894118,1];params['Blink']=[0.8,0.5,0.3,2]
 indicator=ex.text('Nodes/Ui/Indicator.tscn');ip=animation(indicator,1,'Nodes/Ui/Indicator.tscn','Indicator');require(ip['loop']and len(ip['tracks'])==1,'Unknown Indicator animation');keys=ip['tracks'][0]['keys'];params['Indicator']=[ip['length'],keys['times'][0],keys['times'][1],node(indicator,'AnimContainer/HBoxContainer')['custom_constants/separation']];params['IndicatorPosition']=[keys['values'][0][0],keys['values'][1][0],0,0];ibox=float(re.search(r'export \(int\) var size_before_growing = (\d+)',ex.text('Scripts/UI/Indicator.gd'))[1]);params['IndicatorBox']=[node(indicator,'AnimContainer/HBoxContainer/Arrow')['margin_bottom'],ibox,-ibox/2+1,ibox/2]
 bounce=re.search(r'tween_property\(self, "rect_position", rect_position, ([\d.]+)\).*?from\(rect_position - Vector2\(0, (\d+)\)\)',action,re.S);require(bounce,'Unknown source bounce');params['Bounce']=[float(bounce[1]),float(bounce[2]),1,0]
 ns=ex.text('Nodes/Ui/InventorySelect.tscn');nr=node(ns,'.');selectnode=node(scene,'InventorySelect');root=node(scene,'.');width=root['margin_right']-root['margin_left']
 layouts['Select']=[0,0,width,selectnode['margin_bottom']];params['SelectPatch']=[nr.get('patch_margin_'+s,0)for s in ('left','top','right','bottom')]
 cp=node(ns,'CharacterPortraits');pn=node(ns,'CharacterPortraits/Ninten');params['Portraits']=[cp['margin_left'],cp.get('margin_top',0),*pn['rect_min_size']]
 title=node(ns,'CenterContainer');name=node(ns,'CenterContainer/MenuName');layouts['SelectTitle']=[width+title['margin_left'],title['margin_top'],title['margin_right']-title['margin_left'],title['margin_bottom']-title['margin_top']];params['SelectTitleInset']=[name['margin_left'],name['margin_top'],title['margin_right']-name['margin_right']-title['margin_left'],title['margin_bottom']-name['margin_bottom']-title['margin_top']]
 style=re.search(r'^\[sub_resource type="StyleBoxTexture" id=2\]\n(.*?)(?=^\[)',ns,re.M|re.S)[1];params['SelectTitlePatch']=[float(re.search(r'^margin_'+s+r' = ([-\d.]+)',style,re.M)[1])for s in ('left','top','right','bottom')]
 sn=node(scene,'Bottom/StatsBar');params['StatsPatch']=[sn.get('patch_margin_'+s,0)for s in ('left','top','right','bottom')];params['StatsPortraitParent']=[node(scene,'Bottom/StatsBar/CenterContainer')['margin_left'],node(scene,'Bottom/StatsBar/CenterContainer')['margin_top'],0,0]
 portrait=ex.text('Nodes/Ui/Inventory/portrait.tscn');ibody=re.search(r'^\[node name="Indicators"[^\n]*\]\n(.*?)(?=^\[)',portrait,re.M|re.S)[1];anchor=float(re.search(r'^margin_left = ([-\d.]+)',ibody,re.M)[1]);
 for label in ('suitable','equipped','better','lower'):
  body=re.search(r'^\[node name="is_item_'+label+r'"[^\n]*\]\n(.*?)(?=^\[|\Z)',portrait,re.M|re.S)[1];xy=re.search(r'position = Vector2\( ([-\d.]+), ([-\d.]+) \)',body);require('centered = false'in body and xy,'Unknown portrait source pose');params['Portrait'+label]=[anchor,float(xy[1]),float(xy[2]),0]
 stats=ex.text('Nodes/Ui/Inventory/Stats.tscn');normal=node(stats,'StatLabel');mod=node(stats,'Modifier');num=node(stats,'Modifier/ModiferLabel');require('region_rect = Rect2( 3, 0, 7, 9 )'in stats and 'region_rect = Rect2( 16, 0, 7, 9 )'in stats,'Changed stat modifier source regions');up=node(stats.replace('region_rect = Rect2( 3, 0, 7, 9 )','').replace('region_rect = Rect2( 16, 0, 7, 9 )',''),'Modifier/ModifierIconUp');params['StatValue']=[normal['rect_min_size'][0],normal['margin_bottom'],mod['margin_top'],num['margin_bottom']];params['StatIcon']=[*up['position'],0,0]
 dnode=node(scene,'Bottom/StatsBar/StatsLabels/Divider1');params['StatDivider']=[dnode['rect_min_size'][0],dnode['rect_min_size'][1],dnode['margin_top'],0];params['StatDividerColor']=dnode['color']
 titles=node(scene,'Bottom/StatsBar/StatsNameAndDividers/Divider1');params['StatTitleDivider']=[*titles['rect_min_size'],titles['margin_top'],node(scene,'Bottom/StatsBar/StatsNameAndDividers')['custom_constants/separation']];params['StatTitleColor']=titles['color']
 params['StatTitleMin']=[*node(scene,'Bottom/StatsBar/StatsNameAndDividers/HP')['rect_min_size'],node(scene,'Bottom/StatsBar/StatsNameAndDividers/HP')['margin_top'],0]
 label_keys=[node(scene,'Bottom/StatsBar/StatsNameAndDividers/'+n)['text']for n in ('HP','PP','OFE','DEF','SPD','IQ','GUT')];stat_order=[inv['stats'].index(k)for k in ('maxhp','maxpp','offense','defense','speed','iq','guts')]
 texts={t['key']:t for t in inv['texts']};keys=['MENU_TITLE_GOODS','MENU_YES','MENU_NO','INVENTORY_DESCRIPTION']+label_keys
 for row in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/menus - sheet.csv'))):
  if row['key']in keys:texts[row['key']]=dict(key=row['key'],en=row['en'],zh=row['zh_CN'])
 require(all(k in texts for k in keys),'Missing original Goods locale')
 clips=[]
 for name,resource,path in [('Open',4,'.:rect_position:y'),('Close',3,'.:rect_position:y'),('MessageOpen',10,'.:rect_position:y'),('MessageClose',9,'.:rect_position:y'),('StatsOpen',7,'.:rect_position:y'),('StatsClose',6,'.:rect_position:y')]:
  anim=animation(scene,resource,SCENE,name);track=next(t for t in anim['tracks']if t['path']==path);require(track['type']=='value'and track['interp']==1 and track['keys']['update']==0 and not anim['loop'],'Unsupported Goods position animation');clips.append(dict(name=name,length=anim['length'],keys=[list(v)for v in zip(track['keys']['times'],track['keys']['values'],track['keys']['transitions'])]))
 textures=[]
 for name,src,region in ASSETS:
  size=ex.png_size(src);imp=ex.text(src+'.import');require('flags/filter=false'in imp and 'flags/mipmaps=false'in imp,'Unknown Goods texture filtering');textures.append(dict(name=name,source=src,region=region or[0,0,*size],output='graphics/ui/field-goods/'+name+'.t3x'))
 result=dict(schema=1,kind='encore.field-goods.source-ir',commit=PIN,family=0x454e0048,inventory_sha=sha(IR.parent/'native-field-inventory.json'),sources=dict(sorted(ex.sources.items())),dependencies={'content/native-field-inventory.json':sha(IR.parent/'native-field-inventory.json')},layouts=layouts,parameters=params,clips=clips,texts=list(texts.values()),stat_order=stat_order,stat_labels=label_keys,labels=['MENU_TITLE_GOODS','MENU_YES','MENU_NO','INVENTORY_ACTION_SORT_HOW','INVENTORY_ACTION_SORT_MANUAL','INVENTORY_ACTION_SORT_AUTO','INVENTORY_ACTION_DROP_CONFIRM','INVENTORY_ACTION_TARGET','INVENTORY_ACTION_USE_TARGET','INVENTORY_ACTION_TARGET_ALL','INVENTORY_ACTION_USE','INVENTORY_DESCRIPTION'],sounds=[s['source']for s in __import__('tools.item_use',fromlist=['load']).load(ROOT)['sounds']if s['event']in ('Open','Move','Confirm','Back','Close')],textures=textures,semantics=['Singleton normal Goods exposes Ninten and KEY; source include_storage=false and Give requires more than one member','Source two-column sparse-tail movement, description scroll, confirmation cancel, manual swap and auto sort are retained','Consumables execute the shared owning-state consumer once after source target confirmation; future members/transform/map/dialogue source actions remain rejected','Source equipment projection uses actual current slot item and source boosts; no mutated preview state','Source timer result queue and UI animation keys are retained; no test/emulator/hardware claim','Native 400x240 keeps source pixels1:1; measured font container sizing is a bounded singleton adapter, not a claim of complete Godot container parity'])
 validate(result);write(IR,result);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=result['sources'],dependencies=result['dependencies'],semantics=result['semantics']));return result

def validate(d):
 require(set(d)==set('schema kind commit family inventory_sha sources dependencies layouts parameters clips texts stat_order stat_labels labels sounds textures semantics'.split()),'Unknown Goods IR')
 require(d['schema']==1 and d['kind']=='encore.field-goods.source-ir'and d['commit']==PIN and d['family']==0x454e0048,'Goods source identity');require(sorted(d['stat_order'])==list(range(7))and len(d['stat_labels'])==7 and len(d['labels'])==12 and len(d['sounds'])==5,'Goods source selectors')
 require(set(d['layouts'])==set('Confirm Sort Stats StatsNumbers StatsTitles StatsPortrait Select SelectTitle'.split()),'Goods layout roles')
 require(all(len(v)==4 and all(type(x)in(int,float)and math.isfinite(x)for x in v)for v in list(d['layouts'].values())+list(d['parameters'].values())),'Goods finite layout')
 require([c['name']for c in d['clips']]==['Open','Close','MessageOpen','MessageClose','StatsOpen','StatsClose']and all(c['length']>0 and c['keys']and all(len(k)==3 and all(math.isfinite(v)for v in k)for k in c['keys'])and [k[0]for k in c['keys']]==sorted(k[0]for k in c['keys'])for c in d['clips']),'Goods animation source');return d

def load():
 d=validate(read(IR));v=read(REVIEW);require(v['ir_sha256']==sha(IR)and v['sources']==d['sources']and v['dependencies']==d['dependencies']and v['commit']==PIN,'Goods source review stale');ex=Extractor(ROOT)
 for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Changed Goods source '+p)
 for p,h in d['dependencies'].items():require(sha(ROOT/p)==h,'Changed Goods dependency')
 return d

def encode(d):
 validate(d);b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def s(v):z=v.encode();u(len(z));b.extend(z)
 for dic in (d['layouts'],d['parameters']):
  u(len(dic))
  for k,v in dic.items():s(k);f(*v)
 u(*d['stat_order']);[s(k)for k in d['stat_labels']+d['labels']+d['sounds']]
 u(len(d['texts']))
 for t in d['texts']:[s(t[k])for k in ('key','en','zh')]
 u(len(d['clips']))
 for c in d['clips']:
  s(c['name']);f(c['length']);u(len(c['keys']));[f(*k)for k in c['keys']]
 assets=read(ASSET);require(assets['ir_sha256']==sha(IR),'Stale genuine Goods atlas receipt');u(len(assets['textures']))
 for a in assets['textures']:s(a['name']);s(a['output']);u(a['bytes'],a['width'],a['height']);b.extend(bytes.fromhex(a['sha256']))
 u(len(d['sources']))
 for p,h in d['sources'].items():s(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20sIII',b,0,b'ENCFGDS1',1,len(b),0,1,1,d['family'],bytes.fromhex(PIN),0,0,0);b[64:96]=bytes.fromhex(sha(IR));b[96:128]=bytes.fromhex(d['inventory_sha']);struct.pack_into('<I',b,16,zlib.crc32(b)&0xffffffff);return bytes(b)
def compile_pack():b=encode(load());PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b);return b
def compile_assets(tex3ds):
 from PIL import Image
 d=load();work=ROOT/'build/field-goods';work.mkdir(parents=True,exist_ok=True)
 def task(v):
  image=Image.open(ROOT/'upstream/MOTHER-Encore'/v['source']).convert('RGBA');x,y,w,h=v['region'];require(x>=0 and y>=0 and x+w<=image.width and y+h<=image.height,'Goods atlas extent');src=work/(v['name']+'.png');image.crop((x,y,x+w,y+h)).save(src);out=ROOT/'romfs'/v['output'];out.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(tex3ds),'-f','rgba8888','-z','auto','-o',str(out),str(src)],check=True);return dict(name=v['name'],output=v['output'],bytes=out.stat().st_size,width=w,height=h,sha256=sha(out),source=v['source'],source_sha256=d['sources'][v['source']])
 with ThreadPoolExecutor(max_workers=4)as pool:rows=list(pool.map(task,d['textures']))
 a=dict(schema=1,commit=PIN,ir_sha256=sha(IR),textures=rows);write(ASSET,a);write(RECEIPT,a);return a
def stage_files(source):
 d=load();b=encode(d);require((Path(source)/'data/podunk.encgoods').read_bytes()==b,'Goods staged pack mismatch');files={Path('data/podunk.encgoods'):b}
 for a in read(ASSET)['textures']:p=Path(a['output']);raw=(Path(source)/p).read_bytes();require(len(raw)==a['bytes']and sha(Path(source)/p)==a['sha256'],'Goods staged atlas mismatch');files[p]=raw
 return files
def glyph_requests():d=load();return {'Fonts/EBMain_la.tres':[''.join(t[k]for t in d['texts']for k in ('en','zh'))],'Fonts/BottleRocket.tres':[''.join(next(t[k]for t in d['texts']if t['key']==key)for key in d['stat_labels']+[d['labels'][11]]for k in ('en','zh'))]}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','assets']);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
 if a.action=='extract':print('Goods actual source extracted:',len(extract()['textures']),'textures')
 elif a.action=='assets':print('Genuine Goods tex3ds:',len(compile_assets(a.tex3ds)['textures']))
 else:print('Goods checked binary:',len(compile_pack()),'bytes')
