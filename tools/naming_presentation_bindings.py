"""Checked reviewed naming projection; no game binding defaults are embedded here."""
from __future__ import annotations
import hashlib,json,math,re
from pathlib import Path
from tools.extract_battle_entry import Extractor,require,one,node
IR='content/naming-presentation-bindings.json'

def pairs(rows):
 result={}
 for key,value in rows:
  require(key not in result,'Duplicate naming binding field: '+key);result[key]=value
 return result

def keys(value,fields,label):require(type(value)is dict and set(value)==set(fields.split()),'Unknown/missing naming '+label)
def integer(v,limit=4096):require(type(v)is int and 0<=v<=limit,'Naming integer out of bounds');return v
def path(v):require(type(v)is str and v and not v.startswith('/') and not any(c in v.replace("\\'","'") for c in (':','\\','..','\0')),'Unsafe naming path');return v

def route(v,count,depth=0):
 require(type(v)is dict and depth<16,'Naming route malformed')
 op=v.get('op')
 if op=='if':
  keys(v,'op condition yes no','route');require(v['condition']=='right_half','Unknown naming route condition');route(v['yes'],count,depth+1);route(v['no'],count,depth+1)
 else:
  require(op in ('closest','first','adjust','edge'),'Unknown naming route operation')
  keys(v,'op group column_scale bias' if op=='adjust'else'op group','route');require(integer(v['group'])<count,'Unknown naming route group')
  if op=='adjust':require(type(v['column_scale'])is int and abs(v['column_scale'])<=16 and type(v['bias'])is int and abs(v['bias'])<=512,'Naming route adjustment rejected')

def function(text,name):return one(r'^func '+re.escape(name)+r'\([^\n]*\n.*?(?=^func |\Z)',text,'naming function '+name,re.M|re.S)[0]

def load(root,ex=None,document=None):
 root=Path(root);ex=ex or Extractor(root)
 c=document if document is not None else json.loads((root/IR).read_text(encoding='utf-8'),object_pairs_hook=pairs,parse_constant=lambda x:(_ for _ in ()).throw(ValueError('Nonfinite naming JSON')))
 keys(c,'schema commit paths sources review source_rows targets groups group_aliases group_counts character_groups command_groups panel_keys panel_paths commands probe_extra_nodes layouts nodes texts blacklist favorite_food_target favorite_food_save food_asset food_grid food_animation actor_animation actor_type actor_grid sprite_prefix sprite_suffix output_prefix output_suffix translations dependencies save_ir opening_ir session_ir font_receipt battle_irs player_article name_roles target_roles sounds default_count arrow_layout first_resources keyboard_resources field_resources presentation routing','document')
 require(type(c['schema'])is int and c['schema']==1 and c['commit']==ex.lock['commit'],'Naming binding schema/pin rejected')
 keys(c['source_rows'],'save_flavor opening_actor animation_direction session_character layout_label layout_field','source rows')
 for value in c['source_rows'].values():integer(value,4096)
 keys(c['paths'],'scene script sequence save title audio cursor arrow party global text_tools license npc npc_script sprite_script animation project font font_resource','source paths')
 require(set(c['sources'])==set(c['paths'].values()),'Naming source coverage differs')
 for p,digest in c['sources'].items():require(hashlib.sha256(ex.data(path(p))).hexdigest()==digest,'Stale naming binding source '+p)
 require(set(c['review'])=={c['paths']['script'],c['paths']['cursor']},'Naming reviewed function scope differs')
 for p,functions in c['review'].items():
  require(type(functions)is dict and functions,'Missing naming review')
  for name,text in functions.items():require(function(ex.text(p),name)==text,'Changed naming source function '+name)
 script=ex.text(c['paths']['script']);scene=ex.text(c['paths']['scene']);cursor=ex.text(c['paths']['cursor'])
 require(len(c['groups'])==len(c['group_counts'])==len(c['group_aliases'])==len(c['routing']) and 0<len(c['groups'])<=32,'Naming group scope')
 require(len(set(c['groups']))==len(c['groups']) and len(set(c['group_aliases']))==len(c['group_aliases']),'Duplicate naming group')
 for g,alias,count in zip(c['groups'],c['group_aliases'],c['group_counts']):
  path(g);integer(count,512);node(scene,g)
  require(one(r'^onready var '+re.escape(alias)+r' := \$([^\n]+)',script,'naming grid alias')[1]==g,'Naming source group binding changed')
 for r in c['routing']:
  require(type(r)is list and len(r)==4,'Naming route direction scope')
  for item in r:route(item,len(c['groups']))
 for v in ('character_groups','command_groups'):
  require(type(c[v])is list and len(set(c[v]))==len(c[v]),'Naming group list')
  for n in c[v]:require(integer(n)<len(c['groups']),'Naming group reference')
 require(set(c['character_groups']).isdisjoint(c['command_groups']) and set(c['character_groups']+c['command_groups'])==set(range(len(c['groups']))),'Naming group partition')
 require(type(c['panel_keys'])is list and 0<len(c['panel_keys'])<=8 and len(c['panel_keys'])==len(c['panel_paths'])==len(c['keyboard_resources']),'Naming panels')
 for p in c['panel_keys']:require(type(p)is list and len(p)==len(c['character_groups']) and all(type(k)is str and k for k in p),'Naming panel keys')
 for p in c['panel_paths']:path(p)
 require(len(set(c['panel_paths']))==len(c['panel_paths']),'Duplicate keyboard texture')
 require(len(c['layouts'])==5 and len(set(c['layouts']))==5,'Naming layout scope')
 for p in c['layouts']+c['probe_extra_nodes']:node(scene,path(p))
 keys(c['nodes'],'background field label actors npc_sprite npc_shadow dim','nodes')
 for k,p in c['nodes'].items():node(ex.text(c['paths']['npc']) if k.startswith('npc_')else scene,path(p))
 require(len(c['commands'])==3,'Naming command scope');ids=[]
 for command in c['commands']:
  keys(command,'node kind text','command');require(node(scene,path(command['node']))['text']==command['text'],'Naming command source changed');ids.append(integer(command['kind'],3))
 require(set(ids)=={1,2,3},'Naming command identity duplicate/unknown')
 keys(c['translations'],'menus keyboard battle','translations');keys(c['first_resources'],'box cursor actor shadow','resources');keys(c['presentation'],'layouts texts sounds field_bevel','presentation')
 require(len(c['texts'])==7 and len(c['sounds'])==3 and len(c['presentation']['layouts'])==5 and len(c['presentation']['texts'])==7 and len(c['presentation']['sounds'])==3,'Naming semantic role scope')
 for k in ('layouts','texts','sounds'):
  values=c['presentation'][k];require(len(set(values))==len(values),'Duplicate naming role')
  for v in values:integer(v)
 for v in list(c['first_resources'].values())+c['keyboard_resources']+c['field_resources']:integer(v,31)
 require(len(set(c['first_resources'].values()))==4 and len(set(c['keyboard_resources']))==len(c['keyboard_resources']),'Duplicate naming resource role')
 require(len(c['field_resources'])==len(c['targets'])-2,'Naming supported field count')
 for p in c['dependencies']+c['battle_irs']+[c[k]for k in ('save_ir','opening_ir','session_ir','font_receipt')]:require((root/path(p)).is_file(),'Missing naming dependency '+p)
 for p in ('food_asset','sprite_prefix','sprite_suffix','output_prefix','output_suffix'):path(c[p].lstrip('/'))
 integer(c['default_count'],32);integer(c['arrow_layout'],32);integer(c['actor_type']);require(len(c['actor_grid'])==len(c['food_grid'])==2,'Naming grid size')
 for v in c['actor_grid']+c['food_grid']:require(integer(v,1024)>0,'Naming grid zero')
 keys(c['food_animation'],'position shadow length keys','food animation')
 require(len(c['presentation']['field_bevel'])==4 and all(type(v)in (float,int) and math.isfinite(v) and abs(v)<=64 for v in c['presentation']['field_bevel']),'Naming field bevel')
 sequence=ex.yaml(c['paths']['sequence']);require([s.get('target')for s in sequence['scenario']]==c['targets'],'Naming source target order changed')
 require(int(one(r'const DONT_CARE_CHOICES := (\d+)',script,'default choices')[1])==c['default_count'],'Naming default count changed')
 move_sounds=re.findall(r"play_sfx\(['\"]([^'\"]+)['\"]\)",function(cursor,'set_cursor_from_index'))
 selection_sounds=set(re.findall(r"play_sfx\(['\"]([^'\"]+)['\"]\)",function(script,'_on_arrow_selected')))
 back_sound=one(r"play_sfx\(['\"]([^'\"]+)['\"]\)",function(script,'_backspace'),'backspace sound')[1]
 require(move_sounds==[c['sounds'][0]] and selection_sounds=={c['sounds'][1]} and back_sound==c['sounds'][2],'Naming source sound binding changed')
 project=ex.text(c['paths']['project']);width=int(one(r'^window/size/width=(\d+)',project,'source width')[1]);height=int(one(r'^window/size/height=(\d+)',project,'source height')[1])
 size=[float(x)for x in one(r'export var _cursor_size := Vector2\(([\d.]+), ([\d.]+)\)',cursor,'cursor size').groups()]
 xdiv=float(one(r'size.x = -size.x/([\d.]+)',function(cursor,'set_cursor_from_index'),'cursor x divisor')[1]);ydiv=float(one(r'size.y = size.y/([\d.]+)',function(cursor,'set_cursor_from_index'),'cursor y divisor')[1])
 closest=function(cursor,'get_closest_menu_item_index');one(r'cursor_position.x \+= _cursor_size.x - cursor_offset.x',closest,'closest cursor expression')
 offset=node(ex.text(c['paths']['arrow']),'.')['cursor_offset']
 blink=float(one(r'const BLINK_DURATION := ([\d.]+)',script,'error blink')[1]);highlight=function(script,'_highlight_color')
 loops=int(one(r'for i in range\((\d+)\)',highlight,'highlight loops')[1]);final=float(one(r'tween_property\(_prompt_label, "modulate", Color.white, ([\d.]+)\)',highlight,'highlight final duration')[1]);tweens=len(re.findall(r'tween_property\(_prompt_label, "modulate", [^\n]*BLINK_DURATION\)',highlight))
 font_size=int(one(r'^size = (\d+)',ex.text(c['paths']['font_resource']),'naming font size')[1]) if re.search(r'^size = ',ex.text(c['paths']['font_resource']),re.M) else 16 # Godot DynamicFont schema default.
 facts={'source_width':width,'source_height':height,'font_size':font_size,'font_top':int(one(r'^extra_spacing_top = (-?\d+)',ex.text(c['paths']['font_resource']),'font top')[1]),'font_bottom':int(one(r'^extra_spacing_bottom = (-?\d+)',ex.text(c['paths']['font_resource']),'font bottom')[1]),'navigation_point':[size[0]-size[0]/xdiv,offset[1]+size[1]/ydiv],'error_duration':blink*loops*tweens+final,'cancel_delay':float(one(r'create_timer\(([\d.]+)\)',script,'cancel delay')[1]),'dim_color':sum(round(x*255)<<(8*i)for i,x in enumerate(node(scene,c['nodes']['dim'])['color']))}
 projected={'source_width':width,'source_height':height,**c['first_resources'],'keyboard':c['keyboard_resources'],**c['presentation']}
 return c,facts,projected

def check(r,root):
 c,facts,projected=load(root)
 require(r.get('schema')==3 and r.get('presentation')==projected,'Stale naming presentation bindings')
 require(r.get('presentation_sha256')==hashlib.sha256((Path(root)/IR).read_bytes()).hexdigest(),'Stale naming recipe digest')
 require(r.get('error_duration')==facts['error_duration'] and r.get('colors',[None]*5)[4]==facts['dim_color'],'Stale naming source tuning')
 for key,limit in [('layouts',len(r['layouts'])),('texts',len(r['texts'])),('sounds',len(r['sounds']))]:
  require(all(x<limit for x in projected[key]),'Naming role reference out of range')
 for x in list(c['first_resources'].values())+projected['keyboard']:require(x<len(r['resources']),'Naming resource reference out of range')
 for panel,resource in zip(r['panels'],projected['keyboard']):
  require(r['resources'][resource]['path']==c['panel_paths'][projected['keyboard'].index(resource)],'Naming keyboard resource changed')
  for key in panel:
   x,y,w,h=key['rect'];require(0<=x and 0<=y and w>0 and h>0 and x+w<=projected['source_width'] and y+h<=projected['source_height'],'Naming key exceeds source viewport')
 return projected
