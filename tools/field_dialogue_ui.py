#!/usr/bin/env python3
"""Specific original DialogueBox native panel/text/animation consumer resource."""
from __future__ import annotations
import argparse,hashlib,json,re,struct,sys,zlib,math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,decode,require
from tools.field_node_recipe import load as recipe_load,stable,SCENE
from tools.extract_battle_entry import animation
IR=ROOT/'content/native-field-dialogue-ui.json';REVIEW=ROOT/'reports/field-dialogue-ui/source-review.json';OUT=ROOT/'romfs/data/podunk-dialogue-ui.encdui'
FAMILY=0x454e0041
KINDS=['Pending','CanvasLayer','Control','NinePatchRect','GridContainer','HBoxContainer','Label','RichTextLabel','VScrollBar','AnimationPlayer']
ROLES=['.','Dialoguebox','Dialoguebox/Namebox','Dialoguebox/Namebox/ClipBox/Name','Dialoguebox/ClipBox','Dialoguebox/ClipBox/HBoxContainer','Dialoguebox/ClipBox/HBoxContainer/Dialogue','Dialoguebox/ClipBox/HBoxContainer/DippinDots','Dialoguebox/Options']+['Dialoguebox/Options/Option'+str(i)for i in range(1,7)]+['Dialoguebox/ClipBox/HBoxContainer/Dialogue/@@3','Dialoguebox/ClipBox/HBoxContainer/DippinDots/@@2','Dialoguebox/Namebox/ClipBox','AnimationPlayer','NameAnim']
def validate(d):
 require(d['schema']==1 and d['kind']=='encore.field-dialogue-ui.source-ir'and d['commit']==PIN and d['family']==FAMILY and d['scene']==SCENE,'UI source/schema')
 require(len(d['nodes'])==47 and len(d['controls'])==17 and len(d['animations'])==6 and len(d['borrowed_resources'])==2,'UI exact source closure')
 require([n['role']for n in d['nodes']if n['role']]==[ROLES.index(n['node'])+1 for n in d['nodes']if n['role']],'UI role source binding')
 require(len(d['signals'])==5 and len(d['markers'])==2 and all(len(x)==1 for x in d['markers']),'UI source signals/text markers')
 require(len({n['id']for n in d['nodes']})==47 and len({n['role']for n in d['nodes']if n['role']})==20,'UI native ids')
 for n in d['nodes']:require(n['kind']==KINDS.index(n['native_class']) if n['kind'] and n['native_class']in KINDS else n['kind']==0 and n['role']==0,'Unknown UI native capability')
 for a in d['animations']:
  c=a['clip'];require(a['owner']in[stable('AnimationPlayer'),stable('NameAnim')]and len(c['tracks'])==1,'UI animation source owner')
  t=c['tracks'][0];require(t['type']=='value'and t['enabled']and not t['imported']and t['keys']['update']==0 and t['interp']in[1,2]and all(len(v)==2 for v in t['keys']['values']),'Unknown native UI animation codec')
 return d

def extract(native,metadata):
 recipe=recipe_load();d=read(native);m=read(metadata);require(sha(native)==recipe['native_sha256'] and d['source']=='res://'+SCENE and len(d['nodes'])==47,'UI original native proof differs')
 require(m['scene']==SCENE and m['scene_entered']is False and [m['engine'][k]for k in['major','minor','patch','build']]==[3,6,2,'official']and len(m['controls'])==17,'UI native-only metadata proof')
 native_nodes={n['path']:n for n in d['nodes']};meta={c['node']:c for c in m['controls']};resources={r['id']:r for r in d['resources']};nodes=[];controls=[]
 for row in recipe['records']:
  n=dict(id=row['id'],parent=row['parent'],ready=row['ready'],node=row['node'],native_class=row['native_class'],script=row['script']);n['role']=ROLES.index(n['node'])+1 if n['node']in ROLES else 0;n['kind']=KINDS.index(n['native_class'])if n['role']else 0;nodes.append(n)
 for c in recipe['controls']:
  row=next(n for n in nodes if n['id']==c['id']);props=decode(native_nodes[row['node']]['properties']);mm=meta[row['node']];font=mm.get('font',{});texture=props.get('texture');texture=resources[texture['id']]['path'][6:]if texture else'';material=props.get('material');material=resources[material['id']]['path'][6:]if material else''
  require(not props.get('use_parent_material',False),'Unknown UI material inheritance')
  if row['native_class']=='VScrollBar':require(not any(props[k]for k in ['exp_edit','rounded','allow_greater','allow_lesser']) and props['custom_step']==-1,'Unknown ScrollBar Range policy')
  flags=sum(int(props.get(k,False))<<i for i,k in enumerate(['visible','bbcode_enabled','scroll_active','scroll_following','fit_content_height','autowrap']))
  controls.append(dict(id=c['id'],flags=flags,align=props.get('align',0),valign=props.get('valign',0),columns=props.get('columns',0),hseparation=mm.get('hseparation',mm.get('separation',0)),vseparation=mm.get('vseparation',0),line_separation=mm.get('line_separation',0),font_height=font.get('height',0),font_ascent=font.get('ascent',0),font_descent=font.get('descent',0),visible_characters=props.get('visible_characters',-1),minimum=mm['minimum'],patch=[props.get('patch_margin_'+side,0)for side in['left','top','right','bottom']],font=font.get('path','')[6:],texture=texture,material=material,range=[props.get(k,0)for k in ['min_value','max_value','step','page','value']],style=[*mm.get('normal_style',{}).get('minimum',[0,0]),*mm.get('normal_style',{}).get('offset',[0,0])],text=props.get('text',''),bbcode=props.get('bbcode_text','')))
 font_paths={c['font']for c in controls if c['font']};require(len(font_paths)==2 and len({(ROOT/'upstream/MOTHER-Encore'/p).read_text(encoding='utf-8').strip()for p in font_paths})==1,'UI fonts no longer source-equivalent')
 text=(ROOT/'upstream/MOTHER-Encore'/SCENE).read_text(encoding='utf-8');anims=[]
 for owner,ids in [('AnimationPlayer',[1,2,6]),('NameAnim',[3,4,5])]:
  props=decode(native_nodes[owner]['properties']);require(props['autoplay']=='' and props['playback_process_mode']==1 and props['playback_default_blend_time']==0 and props['playback_speed']==1,'Unknown native AnimationPlayer policy')
  for name,res in zip(['Close','Open','RESET'],ids):
   clip=animation(text,res,SCENE,name);target=clip['tracks'][0]['path'].split(':')[0];anims.append(dict(owner=stable(owner),target=stable(target),clip=clip))
 box=(ROOT/'upstream/MOTHER-Encore/Scripts/UI/DialogueBox.gd').read_text(encoding='utf-8');padding=int(re.search(r'var new_size = _name_label.rect_size.x \+ (\d+)',box)[1]);duration=float(re.search(r'tween_property\(\$Dialoguebox/Namebox, "rect_size", Vector2\(new_size, (\d+)\), ([0-9.]+)\)',box)[2])
 require('set_trans(Tween.TRANS_QUART).set_ease(Tween.EASE_OUT)'in box,'Unknown source name tween')
 house=read(ROOT/'content/native-house-presentation.json');require(house['sources'][SCENE]==recipe['sources'][SCENE],'House original UI source differs');borrow=[]
 for role,source in [('dialogue_box','Graphics/UI/Overworld/flavours/defaultbox.png'),('name_box','Graphics/UI/Overworld/flavours/defaulttag.png')]:
  r=next(r for r in house['resources']if r['role']==role);require(sha(ROOT/'romfs'/r['path'])==r['sha256'],'Borrowed genuine texture changed');borrow.append(dict(source=source,**r))
 sources=dict(recipe['sources']);sources['Scripts/UI/DialogueBox.gd']=sha(ROOT/'upstream/MOTHER-Encore/Scripts/UI/DialogueBox.gd');marker_path='Scripts/global/text_tools.gd';sources[marker_path]=sha(ROOT/'upstream/MOTHER-Encore'/marker_path);marker_source=(ROOT/'upstream/MOTHER-Encore'/marker_path).read_text(encoding='utf-8');markers=[re.search('const '+key+r' := "([^"]+)"',marker_source)[1]for key in ['CHAR_DELAY','CHAR_WAIT']]
 out=dict(schema=1,kind='encore.field-dialogue-ui.source-ir',commit=PIN,scene=SCENE,scene_id=recipe['scene_id'],scene_sha256=recipe['source_sha256'],family=FAMILY,recipe_ir_sha256=sha(ROOT/'content/dialogue-node-recipe.json'),house_ir_sha256=sha(ROOT/'content/native-house-presentation.json'),native_sha256=sha(native),metadata_sha256=sha(metadata),exporter_sha256=sha(ROOT/'tools/godot_exporter/field_dialogue_ui.gd'),sources=sources,nodes=nodes,controls=controls,animations=anims,borrowed_resources=borrow,display=house['parameters']['DisplayReference'],name_tween=[padding,duration,house['parameters']['NameSizing'][2],house['parameters']['NameRect'][3]],signals=['animation_started','animation_finished','item_rect_changed','changed','value_changed'],markers=markers,scope=['Native-only metadata was read without scene ENTER/Ready; source scripts were quarantined, and it never grants gameplay Ready','Native CanvasLayer/17 Control widgets/two source AnimationPlayers only: 20 of 47 actual nodes. Other 27 nodes retain pending capabilities','Actual DynamicFont glyphs, HousePresentation text printing/TextTools/locale/source tag semantics, DialogueChoices and original textures are borrowed with provenance; no duplicate content or fonts','Animation value tracks operate real source Control state and emit actual source AnimationPlayer signals only through native internal process order','Root DialogueBox script/Camera/cursor/Timer/audio and singleton endpoints remain independent required consumers. Unknown scripts/methods/extra effect rendering reject before admission'])
 validate(out);write(IR,out);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),exporter_sha256=out['exporter_sha256'],sources=sources,scope=out['scope']))

def load():
 d=validate(read(IR));r=read(REVIEW);recipe=recipe_load();require(r['ir_sha256']==sha(IR)and r['producer_sha256']==sha(Path(__file__))and r['exporter_sha256']==sha(ROOT/'tools/godot_exporter/field_dialogue_ui.gd')and r['sources']==d['sources']and d['recipe_ir_sha256']==sha(ROOT/'content/dialogue-node-recipe.json')and d['house_ir_sha256']==sha(ROOT/'content/native-house-presentation.json'),'UI semantic/provenance review stale')
 for p,h in d['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h,'UI source changed '+p)
 for n in d['nodes']:
  source=next(r for r in recipe['records']if r['id']==n['id']);require(all(n[k]==source[k]for k in['parent','ready','node','native_class','script']),'UI actual recipe binding differs')
 for resource in d['borrowed_resources']:require(sha(ROOT/'romfs'/resource['path'])==resource['sha256'],'UI borrowed source texture stale')
 return d

def encode(d):
 validate(d);b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def i(v):b.extend(struct.pack('<i',v))
 def f(*v):require(all(math.isfinite(x)for x in v),'Nonfinite UI scalar');b.extend(struct.pack('<'+'f'*len(v),*v))
 def t(v):r=v.encode();u(len(r));b.extend(r)
 b.extend(bytes.fromhex(d['recipe_ir_sha256']));b.extend(bytes.fromhex(d['house_ir_sha256']));f(*d['display'],*d['name_tween']);[t(s)for s in d['signals']];[t(s)for s in d['markers']]
 u(len(d['nodes']))
 for n in d['nodes']:u(n['id'],n['parent'],n['ready'],n['role'],n['kind']);t(n['node']);t(n['native_class']);t(n['script'])
 u(len(d['controls']))
 for c in d['controls']:
  u(c['id'],c['flags'],c['align'],c['valign'],c['columns']);i(c['visible_characters']);f(c['hseparation'],c['vseparation'],c['line_separation'],c['font_height'],c['font_ascent'],c['font_descent'],*c['minimum'],*c['patch'],*c['range'],*c['style']);[t(c[k])for k in['font','texture','material','text','bbcode']]
 u(len(d['animations']))
 for a in d['animations']:
  c=a['clip'];k=c['tracks'][0];u(a['owner'],a['target'],c['source_resource_id'],k['interp']);f(c['length'],c['step']);t(c['name']);u(len(k['keys']['times']))
  for time,transition,value in zip(k['keys']['times'],k['keys']['transitions'],k['keys']['values']):f(time,transition,*value)
 u(len(d['borrowed_resources']))
 for r in d['borrowed_resources']:u(r['id'],r['width'],r['height']);t(r['path']);t(r['source']);b.extend(bytes.fromhex(r['sha256']))
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s8I',b,0,b'ENCDUI01',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,20,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['scene_sha256']);b[92:124]=hashlib.sha256(IR.read_bytes()).digest();return bytes(b)
def stage_files(source):
 raw=encode(load());p=Path('data/podunk-dialogue-ui.encdui');require((Path(source)/p).read_bytes()==raw,'UI staged binary stale');return{p:raw}
def main():
 a=argparse.ArgumentParser();a.add_argument('action',choices=['extract','compile']);a.add_argument('--native',type=Path);a.add_argument('--metadata',type=Path);args=a.parse_args()
 if args.action=='extract':require(args.native and args.metadata,'UI extract explicit complete native proof required');extract(args.native,args.metadata);print('Dialogue UI: 20 native owners, 27 pending');return
 raw=encode(load());OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(raw);print('Dialogue UI:',len(raw),'checked bytes; 20 native owners, no scene admission')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,StopIteration,TypeError,OSError,struct.error)as e:sys.exit('FIELD DIALOGUE UI ERROR: '+str(e))
