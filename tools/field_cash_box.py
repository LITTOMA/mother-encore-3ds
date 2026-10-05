#!/usr/bin/env python3
"""Source CashBox/PhoneUnitsBox layout, animation and actual tex3ds assets."""
from __future__ import annotations
import argparse,csv,io,json,re,struct,subprocess,sys,zlib,math
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,sha,require
from tools.extract_battle_entry import Extractor,node,animation,properties
IR=ROOT/'content/native-field-cash-box.json';REVIEW=ROOT/'reports/field-cash-box/source-review.json';PACK=ROOT/'romfs/data/common.enccashbox';RECEIPT=ROOT/'content/asset-receipts/graphics/ui/cash-box/source.json'
SCRIPT='Scripts/UI/CashBox.gd';FONT='Fonts/EBMain.tres'
def write(p,d):
 p.parent.mkdir(parents=True,exist_ok=True)
 with p.open('w',encoding='utf-8',newline='\n')as f:json.dump(d,f,ensure_ascii=False,indent=2);f.write('\n')
def extract():
 ex=Extractor(ROOT);script=ex.text(SCRIPT);ui=ex.text('Scripts/global/uiManager.gd');inventory=ex.text('Scripts/global/Inventory.gd');ex.text('LICENSE')
 require(re.findall(r'^func (\w+)\(',script,re.M)==['open','update','close','open_and_close','_on_Timer_timeout'],'Unknown CashBox script methods')
 for s in ['_amount_label.text = var2str(Inventory.get_phone_units() if _phone_units_mode else globaldata.cash)','_container.margin_left = _container.margin_right','_anim_player.play("Open")','_anim_player.play("Close")','_timer.stop()','_timer.start()','func _on_Timer_timeout():\n\tclose()']:require(s in script,'Unknown CashBox source effect '+s)
 require('return _phone_units if is_phone_units else _cash'in ui and '_add_to_canvas(_cash, 3)'in ui and '_add_to_canvas(_phone_units, 3)'in ui,'CashBox global instance ownership')
 units=re.search(r'static func get_phone_units\(\) -> int:\n(.*?)(?=^static func |\Z)',inventory,re.M|re.S);require(units is not None,'CashBox actual Card query absent');units_item=re.search(r'find_all_in_inventories\("([^"]+)"\)',units[1])[1];ex.yaml('Data/Items/'+units_item+'.yaml');ex.text('Nodes/Ui/mainCanvasLayer.tscn');ex.text('Scripts/global/global.gd');require('sum += (card as Item).doses'in units[1]and 'static func _get_all_inventories(include_storage := false, include_keys := true)'in inventory and 'ret.append_array(inv.find_all_occurrences(item_name))'in inventory,'CashBox source actual doses sum/inventory order');units_scope_flags=1
 font=ex.text(FONT)
 for path in re.findall(r'path="res://([^\"]+)"',font):ex.data(path)
 translations=list(csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/menus - sheet.csv'))));symbols={k:{lang:next((r[lang]or r['en']or k)for r in translations if r['key']==k)for lang in ('en','zh_CN')}for k in ('$_LEFT','$_RIGHT')}
 tex=[];texids={};boxes=[]
 def texture(path,flavor):
  if path not in texids:
   ex.data(path);imp=ex.text(path+'.import');require('flags/filter=false'in imp,'CashBox source nearest filter');size=ex.png_size(path);require(all(0<v<=1024 for v in size),'CashBox texture extent');texids[path]=len(tex);tex.append(dict(source=path,output='graphics/ui/cash-box/'+Path(path).stem+'.t3x',size=size,flavor=flavor))
  require(tex[texids[path]]['flavor']==flavor,'CashBox shader identity mismatch');return texids[path]
 types={'PanelContainer':1,'HBoxContainer':2,'Label':3,'Control':4,'TextureRect':5}
 for mode,source in enumerate(('Nodes/Ui/CashBox.tscn','Nodes/Ui/PhoneUnitsBox.tscn')):
  text=ex.text(source);ext={int(i):p for p,i in re.findall(r'\[ext_resource path="res://([^\"]+)"[^\n]* id=(\d+)\]',text)};root=node(text,'.');require(bool(root.get('_phone_units_mode',False))==bool(mode),'CashBox mode routing')
  require(root['_container']=='Box'and root['_anim_player']=='AnimationPlayer'and root['_timer']=='Timer','CashBox exported host paths')
  rows=[];paths={};styles=[]
  for m in re.finditer(r'^\[node ([^\n]+)\]\n(.*?)(?=^\[|\Z)',text,re.M|re.S):
   a=dict(re.findall(r'(\w+)="([^"]*)"',m[1]));parent=a.get('parent');path=a['name']if parent=='.'else parent+'/'+a['name']if parent else '.'
   if a['type']not in types:require(a['type']in ('CanvasLayer','AnimationPlayer','Timer'),'Unknown CashBox node');continue
   v=properties(m[2]);kind=types[a['type']];allowed=set('material margin_left margin_top margin_right margin_bottom grow_horizontal rect_pivot_offset size_flags_horizontal size_flags_vertical custom_styles/panel custom_constants/separation alignment custom_fonts/font text valign align rect_min_size texture expand stretch_mode'.split());require(set(v)<=allowed,'Unknown CashBox layout property '+path)
   style=0xffffffff;image=0xffffffff
   if kind==1:
    sid=v['custom_styles/panel']['SubResource'];body=re.search(r'^\[sub_resource type="StyleBoxTexture" id='+str(sid)+r'\]\n(.*?)(?=^\[|\Z)',text,re.M|re.S)[1];rect=re.search(r'^region_rect = Rect2\( ([^)]+) \)$',body,re.M);require(rect and [float(x)for x in rect[1].split(',')]==[0,0,24,24],'CashBox source StyleBox region');s=properties(body[:rect.start()]+body[rect.end():]);require(set(s)<=set(['texture']+[prefix+k for prefix in ('content_margin_','margin_','expand_margin_')for k in ('left','top','right','bottom')]),'Unknown CashBox style');image=texture(ext[s['texture']['ExtResource']],True);style=len(styles);styles.append(dict(texture=image,content=[s['content_margin_'+k]for k in ('left','top','right','bottom')],patch=[s['margin_'+k]for k in ('left','top','right','bottom')],expand=[s.get('expand_margin_'+k,0)for k in ('left','top','right','bottom')],region=[0,0,24,24]))
   if kind==5:image=texture(ext[v['texture']['ExtResource']],False)
   if kind==3:require(ext[v['custom_fonts/font']['ExtResource']]==FONT,'CashBox font source variation')
   amount=path==root['_amount_label'];label_role=1 if amount else 2 if v.get('text')=='$_LEFT'else 3 if v.get('text')=='$_RIGHT'else 0
   p=0xffffffff if parent=='.'else paths[parent];paths[path]=len(rows);rows.append(dict(path=path,parent=p,kind=kind,amount=amount,label_role=label_role,style=style,texture=image,rect=[v.get('margin_'+k,0)for k in ('left','top','right','bottom')],minimum=v.get('rect_min_size',[0,0]),hflags=v.get('size_flags_horizontal',1),vflags=v.get('size_flags_vertical',1),align=v.get('align',0),valign=v.get('valign',0),separation=v.get('custom_constants/separation',0),alignment=v.get('alignment',0),expand=v.get('expand',False),stretch_mode=v.get('stretch_mode',0),text=v.get('text','')))
  require(sum(r['amount']for r in rows)==1 and rows[0]['path']=='Box'and rows[0]['kind']==1 and node(text,'Box')['grow_horizontal']==0,'CashBox narrow source tree')
  clips=[]
  for name in ('Open','Close','RESET'):
   a=animation(text,node(text,'AnimationPlayer')['anims/'+name]['SubResource'],source,name);require(not a['loop']and len(a['tracks'])==1,'CashBox clip scope');t=a['tracks'][0];require(t['path']=='Box:rect_position:y'and t['interp']==1 and t['keys']['update']==0,'CashBox continuous interpolation');clips.append(dict(length=a['length'],keys=[dict(time=x,ease=y,value=z)for x,y,z in zip(t['keys']['times'],t['keys']['transitions'],t['keys']['values'])]))
  timer=node(text,'Timer');require(timer['one_shot']is True,'Unknown CashBox repeat Timer');connected='[connection signal="timeout" from="Timer" to="." method="_on_Timer_timeout"]'in text
  boxes.append(dict(mode=mode,source=source,styles=styles,nodes=rows,clips=clips,timer_seconds=timer['wait_time'],timer_connected=connected,layer=int(re.search(r'_add_to_canvas\(_cash, (\d+)\)',ui)[1])))
 shader=ex.text('Shaders/MenuFlavors.tres');colors=[]
 for i in range(1,9):colors.append([int(round(float(c)*255))for c in re.search(r'shader_param/OLDCOLOR'+str(i)+r' = Color\( ([^)]+) \)',shader)[1].split(',')])
 threshold=float(re.search(r'distance\(curr_pixel, OLDCOLOR1\) < ([.0-9]+)',shader)[1]);project=ex.text('project.godot');viewport=[int(re.search(r'^window/size/'+axis+r'=(\d+)',project,re.M)[1])for axis in ('width','height')]
 pixel_snap=re.search(r'^2d/snapping/use_gpu_pixel_snap=(true|false)$',project,re.M);require(pixel_snap is not None,'CashBox source pixel snap policy absent')
 d=dict(schema=1,kind='encore.field-cash-box.source-ir',commit=PIN,script=SCRIPT,font=FONT,units_item=units_item,units_scope_flags=units_scope_flags,viewport=viewport,pixel_snap=pixel_snap[1]=='true',symbols=symbols,source_colors=colors,threshold=threshold,textures=tex,boxes=boxes,sources=dict(sorted(ex.sources.items())),semantics=['Two persistent uiManager CanvasLayer instances, actual cash/Inventory Card values, translated currency labels with empty CSV fallback to English','Source minimum-size Panel/HBox layout and grow BEGIN preserve right edge; 1:1 viewport right adaptation and source GPU pixel snapping','Continuous source value tracks use source key transitions, not a linear slide','CashBox one-shot Timer signal wired; PhoneUnits Timer signal is absent and is not synthesized','CashBox callbacks bind the frozen payphone independently scheduled .5 then 1 second coroutines'],unverified=['Tests not run','Main host integration, emulator and hardware pending'])
 validate(d);write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],semantics=d['semantics'],unverified=d['unverified']));return d
def validate(d):
 require(set(d)==set('schema kind commit script font units_item units_scope_flags viewport pixel_snap symbols source_colors threshold textures boxes sources semantics unverified'.split())and d['schema']==1 and d['kind']=='encore.field-cash-box.source-ir'and d['commit']==PIN and type(d['pixel_snap'])is bool and d['units_scope_flags']==1 and d['units_item'],'CashBox IR identity/unknown fields')
 require(d['script']==SCRIPT and d['font']==FONT and len(d['boxes'])==2 and len(d['textures'])==5 and len(d['source_colors'])==8 and 0<d['threshold']<=1,'CashBox scope')
 for i,b in enumerate(d['boxes']):
  require(set(b)==set('mode source styles nodes clips timer_seconds timer_connected layer'.split()),'CashBox unknown policy fields')
  require(b['mode']==i and b['layer']==3 and 0<b['timer_seconds']<=60 and type(b['timer_connected'])is bool and len(b['nodes'])in (5,7)and len(b['clips'])==3,'CashBox exact tree/timer')
  for j,n in enumerate(b['nodes']):
   require(set(n)==set('path parent kind amount label_role style texture rect minimum hflags vflags align valign separation alignment expand stretch_mode text'.split()),'CashBox unknown node field')
   require(n['kind']in range(1,6)and (n['parent']==0xffffffff if j==0 else n['parent']<j and b['nodes'][n['parent']]['kind']in (1,2))and 0<=n['hflags']<=15 and 0<=n['vflags']<=15 and n['align']in range(3)and n['valign']in range(3)and n['label_role']in range(4)and bool(n['amount'])==(n['label_role']==1),'CashBox bounded source node')
   require(type(n['amount'])is bool and type(n['expand'])is bool and len(n['rect'])==4 and len(n['minimum'])==2 and all(type(v)in (float,int)and math.isfinite(v)and abs(v)<=1000000 for v in n['rect']+n['minimum']+[n['separation']])and min(n['minimum'])>=0 and n['rect'][2]>=n['rect'][0]and n['rect'][3]>=n['rect'][1],'CashBox bounded layout scalar')
  for s in b['styles']:require(set(s)==set('texture content patch expand region'.split())and all(len(s[k])==4 and all(type(v)in (int,float)and math.isfinite(v)and v>=0 for v in s[k])for k in ('content','patch','expand','region')),'CashBox unknown/invalid style fields')
  for c in b['clips']:
   require(set(c)=={'length','keys'}and 0<c['length']<=60 and 0<len(c['keys'])<=256,'CashBox unknown clip')
   last=-1
   for k in c['keys']:require(set(k)=={'time','ease','value'}and all(math.isfinite(v)for v in k.values())and last<k['time']<=c['length']and k['ease']!=0,'CashBox invalid source key');last=k['time']
   require(c['keys'][0]['time']==0,'CashBox missing initial key')
 for t in d['textures']:require(set(t)==set('source output size flavor'.split())and len(t['size'])==2 and all(type(v)is int and 0<v<=1024 for v in t['size'])and type(t['flavor'])is bool,'CashBox unknown texture')
 return d
def load():
 d=validate(read(IR));r=read(REVIEW);require(r['commit']==PIN and r['ir_sha256']==sha(IR)and r['sources']==d['sources'],'CashBox review stale');ex=Extractor(ROOT)
 for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'CashBox source changed '+p)
 return d
def glyph_request():
 d=load();return{FONT:'' .join('0123456789-'+''.join(v for r in d['symbols'].values()for v in r.values()))}
def font_bindings():return{p:[text]for p,text in glyph_request().items()}
def encode(d,receipt):
 validate(d);out=bytearray(64)
 def u(*v):out.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):out.extend(struct.pack('<'+'f'*len(v),*v))
 def s(v):raw=v.encode();u(len(raw));out.extend(raw)
 s(d['script']);s(d['font']);s(d['units_item']);u(d['units_scope_flags'],*d['viewport'],int(d['pixel_snap']));out.extend(struct.pack('<d',d['threshold']))
 for c in d['source_colors']:u(c[0]|c[1]<<8|c[2]<<16|c[3]<<24)
 for key in ('$_LEFT','$_RIGHT'):
  for lang in ('en','zh_CN'):s(d['symbols'][key][lang])
 u(len(d['textures']))
 for t,a in zip(d['textures'],receipt['textures']):s(t['source']);s(t['output']);u(*t['size'],int(t['flavor']),a['bytes']);out.extend(bytes.fromhex(a['sha256']))
 for b in d['boxes']:
  s(b['source']);u(b['mode'],b['layer'],int(b['timer_connected']));f(b['timer_seconds']);u(len(b['styles']))
  for st in b['styles']:u(st['texture']);f(*(st[k][i]for k in ('content','patch','expand','region')for i in range(4)))
  u(len(b['nodes']))
  for n in b['nodes']:
   s(n['path']);u(*(int(n[k])for k in ('parent','kind','amount','label_role','style','texture','hflags','vflags','align','valign','alignment','expand','stretch_mode')));f(*n['rect'],*n['minimum'],n['separation']);s(n['text'])
  for c in b['clips']:
   f(c['length']);u(len(c['keys']))
   for k in c['keys']:f(k['time'],k['ease'],k['value'])
 u(len(d['sources']))
 for p,h in d['sources'].items():s(p);out.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20sIII',out,0,b'ENCCASH1',1,len(out),0,1,1,2,bytes.fromhex(PIN),len(d['sources']),0,0);struct.pack_into('<I',out,16,zlib.crc32(out)&0xffffffff);return bytes(out)
def receipt(d):
 r=read(RECEIPT);require(r['commit']==PIN and r['ir_sha256']==sha(IR)and r['producer_sha256']==sha(ROOT/'tools/field_cash_box.py')and len(r['textures'])==len(d['textures']),'CashBox texture receipt stale')
 for t,a in zip(d['textures'],r['textures']):p=ROOT/'romfs'/t['output'];require(a['output']==t['output']and a['bytes']==p.stat().st_size and a['sha256']==sha(p),'CashBox actual texture changed')
 return r
def compile_pack():
 d=load();raw=encode(d,receipt(d));PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw);return raw
def compile_assets(tool):
 d=load();require(tool and Path(tool).is_file(),'Actual tex3ds required')
 def one(t):
  p=ROOT/'romfs'/t['output'];p.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(tool),'-f','rgba8','-z','none','-o',str(p),str(ROOT/'upstream/MOTHER-Encore'/t['source'])],check=True);return dict(output=t['output'],bytes=p.stat().st_size,sha256=sha(p))
 with ThreadPoolExecutor(max_workers=4)as workers:a=list(workers.map(one,d['textures']))
 write(RECEIPT,dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(ROOT/'tools/field_cash_box.py'),tex3ds_sha256=sha(tool),textures=a));return compile_pack()
def stage_files(source):
 d=load();a=receipt(d);raw=encode(d,a);p=Path(source);require((p/'data/common.enccashbox').read_bytes()==raw,'Stale CashBox pack');out={Path('data/common.enccashbox'):raw}
 for t,r in zip(d['textures'],a['textures']):b=(p/t['output']).read_bytes();require(len(b)==r['bytes']and sha(p/t['output'])==r['sha256'],'Stale CashBox atlas');out[Path(t['output'])]=b
 return out
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','pack']);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
 try:
  if a.action=='extract':extract();print('CashBox/PhoneUnitsBox source layouts and source easing reviewed')
  else:print('CashBox binary:',len(compile_assets(a.tex3ds)if a.action=='compile'else compile_pack()))
 except (ValueError,KeyError,OSError,TypeError,struct.error,subprocess.SubprocessError)as e:sys.exit('CASH BOX ERROR: '+str(e))
