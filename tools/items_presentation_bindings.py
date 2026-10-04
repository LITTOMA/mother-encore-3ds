"""Checked bindings for the bounded source Items menu; no script evaluator."""
import copy,hashlib,json,math,re
from pathlib import Path
from tools.extract_battle_entry import ROOT,PIN,one,node,properties,require
IR=Path(__file__).resolve().parents[1]/'content/items-presentation-bindings.json'
SOURCE_KEYS='battle items_script info_script cursor_script arrow label description scrollbar button inventory item audio system save item_data item_text menu_text flavor text_tools project main_font hint_font main_font_resource hint_font_resource'.split()
NODE_KEYS='panel grid cursor cursor_back scroll info description hint hint_margin hint_label party_info animation_player party_vbox player_info equipped scroll_background scroll_thumb scroll_up scroll_down desc_hbox desc_icon desc_margin desc_text cursor_timer label_prefix'.split()
ASSET_ROLES='panel cursor item equipped icon_frame scroll_background scroll_thumb hint_panel hint_glyph font scroll_up scroll_down'.split()
LAYOUT_KEYS='panel grid label equipped cursor cursor_back scroll scroll_background scroll_thumb scroll_up scroll_down info icon_frame item description hint_panel hint_glyph'.split()
LAYOUT_ROLES={'Container','Panel','Grid','ItemLabel','ItemIcon','Equipped','Cursor','InfoPanel','Description','Scrollbar','ScrollBackground','ScrollThumb','Hint'}
DRAW_KINDS={'Container','Sprite','Rectangle','NinePatch','Text'}
def fields(value,keys,label):require(type(value)is dict and set(value)==set(keys),'Unknown/missing Items '+label)
def uint(value,maximum=0xffffffff):return type(value)is int and 0<value<=maximum
def finite(value):return type(value)in(int,float)and math.isfinite(value)
def vector(value,size):return type(value)is list and len(value)==size and all(finite(v)for v in value)
def safe(value):return type(value)is str and value and ':'not in value and '\\'not in value and all(ord(v)>=32 and ord(v)!=127 for v in value)and all(v not in('','.','..')for v in value.split('/'))
def unique(pairs):
 result={}
 for key,value in pairs:require(key not in result,'Duplicate Items binding field');result[key]=value
 return result
def read(path):return json.loads(Path(path).read_text(encoding='utf-8'),object_pairs_hook=unique)
def sn(text,path):return node(text.replace('Rect2(','Color('),path)
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def load(root=ROOT,recipe=None):
 root=Path(root);b=copy.deepcopy(read(IR)if recipe is None else recipe)
 fields(b,'schema kind commit sources source_refs nodes assets asset_roles reuse layouts item clips cursor info platform probe sounds texture_links source_facts grid'.split(),'binding root')
 require(type(b['schema'])is int and b['schema']==1 and b['kind']=='encore.items-presentation-bindings'and b['commit']==PIN,'Unsupported Items binding schema/pin')
 inventory=read(root/'compatibility/upstream-inventory.json');lock=read(root/'upstream.lock')
 require(inventory['commit']==lock['commit']==PIN and type(b['sources'])is dict and 32<=len(b['sources'])<=128,'Items binding source coverage')
 texts={}
 for path,digest in b['sources'].items():
  require(safe(path)and path in inventory['files']and inventory['files'][path]['sha256']==digest and sha(root/'upstream/MOTHER-Encore'/path)==digest,'Changed Items binding source '+str(path))
 for key in SOURCE_KEYS:
  require(key in b['source_refs']and b['source_refs'][key]in b['sources'],'Unknown Items source selector')
 fields(b['source_refs'],SOURCE_KEYS,'source selectors');fields(b['nodes'],NODE_KEYS,'node selectors')
 fields(b['grid'],['columns','rows'],'grid');require(uint(b['grid']['columns'],8)and uint(b['grid']['rows'],16),'Invalid Items source grid')
 def source(key):
  if key not in texts:texts[key]=(root/'upstream/MOTHER-Encore'/b['source_refs'][key]).read_text(encoding='utf-8')
  return texts[key]
 battle_keys=NODE_KEYS[:14]+['label_prefix']
 for key,value in b['nodes'].items():
  require(safe(value),'Invalid Items node selector')
  if key=='label_prefix':
   for i in range(b['grid']['columns']*b['grid']['rows']):sn(source('battle'),value+str(i))
  elif key in battle_keys:sn(source('battle'),value)
  elif key=='equipped':sn(source('label'),value)
  elif key.startswith('scroll_'):sn(source('scrollbar'),value)
  elif key.startswith('desc_'):sn(source('description'),value)
  else:sn(source('arrow'),value)
 fields(b['grid'],['columns','rows'],'grid');require(uint(b['grid']['columns'],8)and uint(b['grid']['rows'],16),'Invalid Items source grid')
 items=source('items_script')
 for key,axis in [('columns','X'),('rows','Y')]:require(int(one(r'^const ITEM_PAGE_SIZE_'+axis+r' := (\d+)$',items,'Items grid')[1])==b['grid'][key],'Items grid source mismatch')
 fields(b['asset_roles'],ASSET_ROLES,'asset roles');require(type(b['assets'])is list and len(b['assets'])==len(ASSET_ROLES),'Items asset coverage')
 asset_names=set();outputs=set();assets={}
 for i,row in enumerate(b['assets']):
  base={'id','name','source','size','grid','output','reuse'};optional=set(row)-base
  require(base<=set(row)and optional in(set(),{'crop'},{'glyph'},{'quarter_turns'})and row['id']==i+1 and type(row['id'])is int and type(row['name'])is str and safe(row['name'])and row['name']not in asset_names and safe(row['output'])and row['output'].endswith('.t3x')and row['output']not in outputs and row['source']in b['sources']and type(row['reuse'])is bool,'Unknown/duplicate Items asset binding')
  for key in ['size','grid']:require(type(row[key])is list and len(row[key])==2 and all(uint(v,1024)for v in row[key]),'Invalid Items asset geometry')
  require(all(s%g==0 for s,g in zip(row['size'],row['grid'])),'Invalid Items atlas geometry')
  if row['source'].endswith('.png'):
   from PIL import Image
   with Image.open(root/'upstream/MOTHER-Encore'/row['source'])as image:require(list(image.size)==row['size'],'Items image source dimensions')
   require(row['source']+'.import'in b['sources'],'Missing Items texture import provenance')
  if 'crop'in row:
   require(vector(row['crop'],4)and all(type(v)is int for v in row['crop']),'Invalid Items crop')
   x,y,w,h=row['crop'];require(x>=0 and y>=0 and w>0 and h>0 and x+w<=row['size'][0]and y+h<=row['size'][1],'Items crop outside source')
  if 'quarter_turns'in row:require(type(row['quarter_turns'])is int and row['quarter_turns']in(1,3)and not row['reuse'],'Unsupported Items frame rotation')
  asset_names.add(row['name']);outputs.add(row['output']);assets[row['name']]=row
 require(set(b['asset_roles'].values())==asset_names,'Incomplete/aliased Items resource roles')
 indices={key:assets[value]['id']-1 for key,value in b['asset_roles'].items()}
 require(type(b['reuse'])is dict and set(b['reuse'])=={r['name']for r in b['assets']if r['reuse']},'Incomplete Items reuse receipt mapping')
 for key,row in b['reuse'].items():
  fields(row,['receipt','name'],'reuse binding');require(safe(row['receipt'])and row['receipt'].startswith('romfs/')and row['receipt'].endswith('/source.json')and safe(row['name']),'Invalid Items reuse binding')
  receipt=read(root/row['receipt']);matches=[r for r in receipt['resources']if r['name']==row['name']]
  require(len(matches)==1 and [matches[0]['width'],matches[0]['height']]==assets[key]['size'],'Items reused resource source geometry')
  target=root/'romfs'/assets[key]['output'];entry=receipt['outputs'][target.name]
  require(sha(target)==entry['sha256']and target.stat().st_size==entry['bytes'],'Changed Items reused resource')
 fields(b['layouts'],LAYOUT_KEYS,'layout bindings')
 for key,row in b['layouts'].items():
  fields(row,['role','kind','resource','anchor','flags'],'layout row')
  require(row['role']in LAYOUT_ROLES and row['kind']in DRAW_KINDS and row['resource']in set(ASSET_ROLES)|{''}and vector(row['anchor'],2)and all(0<=v<=1 for v in row['anchor'])and type(row['flags'])is int and 0<=row['flags']<=15,'Invalid Items layout binding')
 fields(b['item'],['party','source_name','definition_id','owner_id','instance_id','flags','can_use','slot'],'identity')
 require(all(uint(b['item'][key])for key in ['definition_id','owner_id','instance_id'])and b['item']['flags']==1 and type(b['item']['flags'])is int and b['item']['can_use']==1 and type(b['item']['can_use'])is int,'Unsupported Items stable identity/behavior')
 import yaml
 save=yaml.safe_load(source('save'));item=yaml.safe_load(source('item_data'))
 require(save['party']==[b['item']['party']]and save[b['item']['party']]['inventory']==[{'item_name':b['item']['source_name'],'equipped':True}]and Path(b['source_refs']['item_data']).stem==b['item']['source_name']and item['slot']==b['item']['slot']and item['can_use']==[b['item']['party']],'Items source identity mismatch')
 fields(b['clips'],['names','player','properties','cursor_idle','cursor_animation','duration'],'clips')
 require(b['clips']['names']==['Open','Close']and b['clips']['player']=='animation_player'and b['clips']['cursor_idle']=='CursorIdle'and b['clips']['properties']=={'.:rect_position':'Position','.:visible':'Visible'}and finite(b['clips']['duration'])and b['clips']['duration']>0,'Unsupported Items animation lowering')
 player=sn(source('battle'),b['nodes'][b['clips']['player']])
 for name in b['clips']['names']:require('anims/'+name in player,'Missing Items source clip')
 arrow=sn(source('arrow'),'.');rid=arrow['frames']['SubResource']
 body=one(r'^\[sub_resource type="SpriteFrames" id='+str(rid)+r'\]\n(.*?)(?=^\[|\Z)',source('arrow'),'Items cursor frames',re.M|re.S)[1]
 require(properties(body)['animations'][0]['name']==b['clips']['cursor_animation'],'Items cursor animation source mismatch')
 for reference in properties(body)['animations'][0]['frames']:
  rid=reference['SubResource'];body=one(r'^\[sub_resource type="AtlasTexture" id='+str(rid)+r'\]\n(.*?)(?=^\[|\Z)',source('arrow'),'Items atlas texture',re.M|re.S)[1]
  props=properties(body.replace('Rect2(','Color('));atlas=props['atlas']['ExtResource']
  path=one(r'^\[ext_resource path="res://([^"]+)" type="Texture" id='+str(atlas)+r'\]',source('arrow'),'Items cursor texture')[1]
  require(path==assets[b['asset_roles']['cursor']]['source'],'Items cursor atlas source mismatch')
 fields(b['cursor'],['x_expression','y_expression','frame_size','scroll_inset'],'cursor')
 for axis,sign in [('x','-'),('y','')]:
  expression=b['cursor'][axis+'_expression'];match=re.fullmatch(re.escape(sign+'size.'+axis)+r'/([0-9]+(?:\.[0-9]+)?)',expression or '')
  require(match and float(match[1])>0 and '\tsize.'+axis+' = '+expression in source('cursor_script'),'Unreviewed Items cursor source expression')
  b['cursor_'+axis+'_divisor']=float(match[1])
 require(vector(b['cursor']['frame_size'],2)and all(uint(v,64)for v in b['cursor']['frame_size'])and finite(b['cursor']['scroll_inset'])and b['cursor']['scroll_inset']>=0,'Invalid Items cursor geometry')
 cursor=assets[b['asset_roles']['cursor']]
 require(cursor['grid']==[cursor['size'][0]//b['cursor']['frame_size'][0],1]and cursor['size'][0]%b['cursor']['frame_size'][0]==0 and cursor['size'][1]==b['cursor']['frame_size'][1],'Items cursor frame grid source mismatch')
 for key in ['scroll_up','scroll_down']:
  angle=sn(source('scrollbar'),b['nodes'][key])['rotation'];row=assets[b['asset_roles'][key]]
  require(abs(abs(angle)-math.pi/2)<.00001 and row['source']==assets[b['asset_roles']['cursor']]['source']and row.get('quarter_turns')==(1 if angle<0 else 3),'Items rotated cursor source binding mismatch')
 fields(b['info'],['hide_expression','show_expression','property','trans','ease'],'info tween')
 require(b['info']['property']=='rect_position:y'and b['info']['trans']=='QUAD'and b['info']['ease']=='OUT','Unsupported Items info tween mechanism')
 times=[]
 for key in ['hide','show']:
  pattern=r'_tween.tween_property\(self, "'+re.escape(b['info']['property'])+'", '+re.escape(b['info'][key+'_expression'])+r', ([0-9.]+)\) \\\n\t+\.set_trans\(Tween.TRANS_'+b['info']['trans']+r'\)\.set_ease\(Tween.EASE_'+b['info']['ease']+r'\)'
  times.append(float(one(pattern,source('info_script'),'Items info tween',re.M)[1]))
 require(times[0]==times[1]and 0<times[0]<=10,'Unsupported Items info tween timing');b['info_duration']=times[0]
 fields(b['platform'],'viewport scope_action scope_button input_adapter normal_color hint_glyph hint_size hint_offset hint_patch glyph_origin description_line_gap'.split(),'platform bindings')
 p=b['platform'];require(vector(p['viewport'],4)and all(uint(v,1024)for v in p['viewport'][:2])and p['viewport'][2:]==[0,0]and re.fullmatch(r'ui_[a-z_]+',p['scope_action'])and uint(p['scope_button'],0xffff)and p['scope_button']&(p['scope_button']-1)==0 and safe(p['input_adapter'])and p['input_adapter'].startswith('content/'),'Invalid Items platform/input binding')
 for key,size in [('normal_color',4),('hint_size',2),('hint_offset',2),('hint_patch',4),('glyph_origin',2)]:require(vector(p[key],size),'Invalid Items platform geometry')
 require(all(0<=v<=1 for v in p['normal_color'])and all(uint(v,64)for v in p['hint_size'])and all(type(v)is int and 0<=v<=64 for v in p['hint_patch'])and finite(p['description_line_gap'])and 0<=p['description_line_gap']<=64 and type(p['hint_glyph'])is str and re.fullmatch('[A-Z]',p['hint_glyph']),'Unsupported Items hint/color adapter')
 hint=assets[b['asset_roles']['hint_glyph']];require(hint.get('glyph')==p['hint_glyph']and hint['size']==p['hint_size']and hint['source']==b['source_refs']['hint_font']and not hint['reuse'],'Items hint glyph source binding mismatch')
 require(sn(source('battle'),b['nodes']['hint_label'])['key']==p['scope_action']and 'event.is_action_pressed("'+p['scope_action']+'")'in source('info_script'),'Items input action source mismatch')
 action=one(r'^'+re.escape(p['scope_action'])+r'=\{\n(.*?)\n\}',source('project'),'Items source scope action',re.M|re.S)[1]
 b['scope_joy']=int(one(r'Object\(InputEventJoypadButton,[^\n]*"button_index":(\d+)',action,'Items scope joypad')[1]);b['scope_key']=int(one(r'Object\(InputEventKey,[^\n]*"scancode":(\d+)',action,'Items scope keyboard')[1])
 # Resolve exported NodePath references instead of accepting unrelated nodes.
 links=[('panel','_info_box','info'),('cursor','menu_parent_path','grid'),('info','desc_label','description')]
 for owner,prop,target in links:
  from posixpath import normpath,join
  value=sn(source('battle'),b['nodes'][owner])[prop]
  require(normpath(join(b['nodes'][owner],value))==b['nodes'][target],'Items exported node reference mismatch')
 fields(b['probe'],'main_font_size main_font_spacing_top main_font_spacing_bottom hint_antialiased hint_outline hint_spacing_char hint_spacing_space engine'.split(),'font/reference')
 probe=b['probe'];require(uint(probe['main_font_size'],64)and type(probe['hint_antialiased'])is bool and uint(probe['hint_outline'],8)and re.fullmatch('[0-9a-f]{40}',probe['engine']),'Invalid Items native font/reference')
 for key in ['main_font_spacing_top','main_font_spacing_bottom','hint_spacing_char','hint_spacing_space']:require(type(probe[key])is int and -32<=probe[key]<=32,'Invalid Items native font spacing')
 for source_key,prefix in [('main_font_resource','main_font'),('hint_font_resource','hint')]:
  body=one(r'^\[resource\]\n(.*)',source(source_key),'Items font resource',re.M|re.S)[1];props=properties(body)
  for prop,key in [('extra_spacing_top','main_font_spacing_top'),('extra_spacing_bottom','main_font_spacing_bottom')]if prefix=='main_font'else [('outline_size','hint_outline'),('extra_spacing_char','hint_spacing_char'),('extra_spacing_space','hint_spacing_space')]:require(props[prop]==probe[key],'Items font property source mismatch')
  require(probe['main_font_size']==props.get('size',16),'Items source default font size mismatch')
 require(type(b['texture_links'])is list and len(b['texture_links'])==5,'Items texture link coverage')
 for link in b['texture_links']:
  fields(link,['asset','source','node','property'],'texture link');require(link['asset']in ASSET_ROLES and link['source']in SOURCE_KEYS and link['node']in NODE_KEYS and link['property']=='texture','Unknown Items texture source link')
  props=sn(source(link['source']),b['nodes'][link['node']]);rid=props[link['property']]['ExtResource']
  path=one(r'^\[ext_resource path="res://([^"]+)" type="Texture" id='+str(rid)+r'\]',source(link['source']),'Items texture source')[1]
  require(path==assets[b['asset_roles'][link['asset']]]['source'],'Items texture role source mismatch')
  if link['asset']=='equipped':require(props['region_rect']==assets[b['asset_roles'][link['asset']]].get('crop'),'Items equipped region source mismatch')
 require(type(b['sounds'])is list and len(b['sounds'])==5 and {r.get('event')for r in b['sounds']}=={'Open','Move','Close','Disabled','Confirm'},'Items sound event coverage')
 for row in b['sounds']:
  fields(row,['event','key','audio_id','source'],'sound binding');require(type(row['key'])is str and re.fullmatch('[a-z0-9_]+',row['key'])and uint(row['audio_id'])and row['source']in SOURCE_KEYS,'Invalid Items sound binding')
  path=one(r'"'+re.escape(row['key'])+r'": load\("res://([^"\n]+)"\)',source(row['source']),'Items sound source')[1]
  require(path in b['sources']and path+'.import'in b['sources'],'Missing Items audio source provenance')
  audio=read(root/'content/native-audio.json');matches=[r for r in audio['assets']if r['stable_id']==row['audio_id']]
  require(audio['upstream_commit']==PIN and len(matches)==1 and matches[0]['source_path']=='res://'+path,'Items stable audio identity source mismatch')
 require(type(b['source_facts'])is dict and b['source_facts'],'Missing Items source semantic facts')
 for path,facts in b['source_facts'].items():
  require(path in b['sources']and type(facts)is list and facts,'Invalid Items semantic fact source')
  text=(root/'upstream/MOTHER-Encore'/path).read_text(encoding='utf-8')
  require(all(type(f)is str and f and f in text for f in facts),'Changed Items semantic fact')
 require(set(b['source_facts'])=={b['source_refs'][key]for key in ['items_script','cursor_script','info_script','item']},'Incomplete Items semantic source coverage')
 b['asset_indices']=indices
 return b

def check_round(ir,root=ROOT):
 # The ordinary compiler uses exactly the same bounded source extraction.
 from tools import items_assets as assets
 require(Path(root)==assets.ROOT,'Items extractor root must match compiler checkout')
 ex,definitions,raw=assets.reviewed_extract()
 reference=read(root/'reports/items-menu-source/native-layout.json')
 receipt=read(root/'romfs/items-preview/source.json')
 require(assets.make_recipe(ex)==receipt['recipe'],'Items asset binding differs from compiled checked source receipt')
 require(sha(root/'reports/items-menu-source/native-layout.json')==receipt['native_reference_sha256']and reference['engine']['hash']==load(root)['probe']['engine'],'Changed Items checked native layout/reference')
 expected=assets.export_ir(ex,definitions,raw,reference,receipt['resources'],receipt['dependencies'],write=False)
 require(expected==ir,'Items IR differs from checked presentation/source bindings')
