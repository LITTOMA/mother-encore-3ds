#!/usr/bin/env python3
"""Source-bound New Game settings, final confirmation and explicit UI palettes."""
import argparse,csv,hashlib,io,json,re,struct,subprocess,sys,zlib
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,node,one,animation,require,properties
from tools.source_settings import choices
from tools.save_menu_assets import normalize
IR=ROOT/'content/native-startup-settings.json';PACK=ROOT/'romfs/data/opening.encsettings'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def rect(p):
 x,y=p.get('margin_left',0),p.get('margin_top',0);return [x,y,p.get('margin_right',0)-x,p.get('margin_bottom',0)-y]
def add(a,b):return [a[0]+b[0],a[1]+b[1],*b[2:]]
def extract():
 ex=Extractor(ROOT);path='Maps/Naming screen.tscn';scene=ex.text(path);script=ex.text('Scripts/UI/NamingScreen/Naming screen.gd');source=choices(ex)
 for p in ['Scripts/UI/NamingScreen/TextSpeed.gd','Scripts/UI/NamingScreen/Flavors.gd','Scripts/UI/NamingScreen/ButtonPrompts.gd','Scripts/UI/colorRectFlavor.gd','Shaders/MenuFlavors.tres']:ex.data(p)
 require('_current_step = 0' in script and '_on_ConfirmationArrow_cancel():\n\t_restart_sequence()' in script,'Source restart changed')
 require(ex.yaml('Data/NamingSequences/intro.yaml')['scenario'][-2:]==[{'char_anims_enter':[], 'char_anims_leave':[], 'type':'settings'}, {'char_anims_enter':[], 'char_anims_leave':[], 'type':'confirm'}],'Settings/confirmation ordering changed')
 menus={r['key']:r['en']for r in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/menus - sheet.csv')))}
 save=ex.yaml('Data/save_new_game.yaml');defaults=[source['speeds'].index(save['textspeed']),source['flavors'].index(save['menuflavor']),source['prompts'].index(save['buttonprompts'])]
 def pose(name):
  ident=int(one('anims/'+name+r' = SubResource\( (\d+) \)',scene,name)[1]);a=animation(scene,ident,path,name)
  return {t['path']:t['keys']['values'][-1]for t in a['tracks']if t['type']=='value'}
 settings=pose('SettingsOpen');confirm=pose('ConfirmationOpen')
 def positioned(path,positions):
  r=rect(node(scene,path));r[:2]=positions.get(path+':rect_position',r[:2]);return r
 r=dict(schema=1,commit=ex.lock['commit'],scope='Original settings choices and static final confirmation layout, with source restart/cancel semantics. Source transition choreography and Introduction remain separate.',choices=source,defaults=defaults,description=save['description'],text_color=0xffffffff,patch=[node(scene,'CanvasLayer/Settings')['patch_margin_'+v]for v in ['left','top','right','bottom']],settings_box=positioned('CanvasLayer/Settings',settings),confirmation_settings_box=positioned('CanvasLayer/Settings',confirm))
 r['rows']=[]
 for index,name in enumerate(['TextSpeed','MenuFlavor','ButtonPrompts','End']):
  key='CanvasLayer/Settings/VBoxContainer/'+name;label=node(scene,key);text=menus[label['text']]
  value_path='CanvasLayer/Settings/VBoxContainer2/'+['Speed','Flavor','Prompts'][index]if index<3 else None
  r['rows'].append(dict(text=text,label=add(positioned('CanvasLayer/Settings/VBoxContainer',settings),rect(label)),value=add(positioned('CanvasLayer/Settings/VBoxContainer2',settings),rect(node(scene,value_path)))if value_path else [0,0,0,0]))
 r['confirmation_row_offset']=[confirm['CanvasLayer/Settings/VBoxContainer:rect_position'][i]-settings['CanvasLayer/Settings/VBoxContainer:rect_position'][i]for i in range(2)]
 r['panels']=[]
 for title,names,values in [('TextSpeed',['Fast','Medium','Slow'],source['speed_names']),('Flavors',source['flavors'],source['flavors']),('ButtonPrompts',source['prompts'],source['prompts'])]:
  p='CanvasLayer/'+title;box=rect(node(scene,p));box[0]=float(one(r'tween_property\(menu, "rect_position:x", ([0-9.]+)',script,'setting panel x')[1]);container=rect(node(scene,p+'/VBoxContainer'));labels=[]
  for name in names:
   q=node(scene,p+'/VBoxContainer/'+name);rr=rect(q)
   if rr[3]==0:rr[3]=12 # inherited FlavorListLabel is verified below
   labels.append(dict(text=menus[q['text']],rect=add(container,rr)))
  r['panels'].append(dict(box=box,labels=labels))
 # Resolve inherited label metadata explicitly rather than accepting invented geometry.
 ext={int(i):p for p,i in re.findall(r'^\[ext_resource path="res://([^"]+)"[^\n]* id=(\d+)\]',scene,re.M)}
 flavor=ex.text(ext[23]);require(rect(node(flavor,'.'))[3]==12,'Flavor inherited label height changed')
 r['speed_labels']=[menus['MENU_'+s]for s in source['speed_names']]
 r['flavor_labels']=[menus['FLAVOR_'+s.upper()]for s in source['flavors']]
 r['prompt_labels']=[menus['MENU_'+s.upper()]for s in source['prompts']]
 r['resources']=[]
 def resource(source,normalized=False):
  ex.data(source);image=Image.open(ex.upstream/source).convert('RGBA');image=normalize(image,r['patch'])if normalized else image;ident=len(r['resources']);r['resources'].append(dict(path='settings-preview/ui-'+str(ident)+'.t3x',source=source,width=image.width,height=image.height,columns=1,rows=1,normalize=normalized));return ident
 r['box_resource']=resource(ext[node(scene,'CanvasLayer/Settings')['texture']['ExtResource']],True)
 r['card_resource']=resource(ext[node(scene,'CanvasLayer/ConfirmationLeft/Confirm0')['texture']['ExtResource']],True)
 r['inside_resource']=resource(ext[node(scene,'CanvasLayer/ConfirmationLeft/Confirm0/Inside')['texture']['ExtResource']],True)
 r['confirmation_fields']=[]
 for index,actor in enumerate(['Ninten','Ana','Lloyd','Pippi','Teddy','Plate']):
  p='CanvasLayer/Confirmation'+('Left'if index<5 else 'Right');card=p+'/Confirm'+str(index);global_rect=add(positioned(p,confirm),rect(node(scene,card)));icon=node(scene,'Toolbox/Confirm/'+actor)
  r['confirmation_fields'].append(dict(box=global_rect,inside=rect(node(scene,card+'/Inside')),label=rect(node(scene,card+'/Label')),icon=rect(icon),resource=resource(ext[icon['texture']['ExtResource']])))
 p='CanvasLayer/ConfirmationRight/Surely';r['confirmation_box']=add(positioned('CanvasLayer/ConfirmationRight',confirm),rect(node(scene,p)));r['certainty']=dict(text=menus[node(scene,p+'/Label')['text']],rect=rect(node(scene,p+'/Label')))
 r['confirmation_choices']=[dict(text=menus[node(scene,p+'/VBoxContainer/'+n)['text']],rect=add(rect(node(scene,p+'/VBoxContainer')),rect(node(scene,p+'/VBoxContainer/'+n))))for n in ['Label','Label2']]
 # Plain palette values are already baked into existing native UI skins. A path
 # allowlist prevents a same-colored world/actor pixel ever being changed.
 ui=ex.text('Scripts/global/uiManager.gd');block=one(r'var menuFlavors := \[(.*?)\n\]',ui,'palettes',re.S)[1];r['palettes']=[]
 palettes=[json.loads(v)for v in re.findall(r'(\[[^\n]+?\])',block)];require(len(palettes)==len(source['flavors'])and all(len(p)==8 for p in palettes),'Palette dimensions changed')
 shader=ex.text('Shaders/MenuFlavors.tres');thresholds=re.findall(r'distance\(curr_pixel, OLDCOLOR\d\) < ([0-9.]+)',shader);require(len(thresholds)==8 and len(set(thresholds))==1,'Menu flavor threshold changed');r['palette_threshold']=float(thresholds[0]);material=properties(shader.split('[resource]\n',1)[1]);r['source_palette']=[sum(round(channel*255)<<(8*i)for i,channel in enumerate(material['shader_param/OLDCOLOR'+str(n)]))for n in range(1,9)]
 r['palettes']=[[int(c[0:2],16)|(int(c[2:4],16)<<8)|(int(c[4:6],16)<<16)|0xff000000 for c in row]for row in palettes]
 # Native resources whose upstream nodes use MenuFlavors, reviewed by role.
 bindings=[('battle-preview/source.json',{'box','plate','plate-bg','hp-label','pp-label'}),('round-preview/source.json',{'box'}),('doll-preview/source.json',{'box'}),('pillow-preview/source.json',{'box'}),('house-preview/source.json',{'dialogue_box'})]
 paths=set()
 for name,roles in bindings:
  d=json.loads((ROOT/'romfs'/name).read_text())
  for a in d['resources']:
   if a.get('name',a.get('role'))in roles:paths.add(a.get('output',a.get('path')))
 # Save cards retain their per-slot palette and must never join this registry.
 paths.update(a['path']for a in r['resources'][:3]);r['skin_paths']=sorted(paths)
 r['skin_sha256']={p:sha(ROOT/'romfs'/p)for p in paths if not p.startswith('settings-preview/')}
 for p in ['Nodes/Ui/Battle/PartyInfoPlate.tscn','Nodes/Ui/DialogueBox.tscn']:ex.data(p)
 r['sources']=ex.sources;return r

def encode(r):
 raw=bytearray();put=lambda f,*v:raw.extend(struct.pack('<'+f,*v))
 def text(s):b=s.encode();put('I',len(b));raw.extend(b)
 def texts(v):put('I',len(v));[text(x)for x in v]
 def rect(v):put('4f',*v)
 def label(v):text(v['text']);rect(v['rect'])
 c=r['choices'];put('I',len(c['speeds']));[put('d',v)for v in c['speeds']];texts(c['flavors']);texts(c['prompts']);texts(r['speed_labels']);texts(r['flavor_labels']);texts(r['prompt_labels']);put('3I2I4I',*r['defaults'],int(r['description']),r['text_color'],*r['patch'])
 rect(r['settings_box']);rect(r['confirmation_settings_box']);put('2f',*r['confirmation_row_offset']);put('I',len(r['rows']))
 for row in r['rows']:text(row['text']);rect(row['label']);rect(row['value'])
 put('I',len(r['panels']))
 for p in r['panels']:rect(p['box']);put('I',len(p['labels']));[label(v)for v in p['labels']]
 put('I',len(r['resources']))
 for a in r['resources']:text(a['path']);put('4I',*[a[k]for k in ['width','height','columns','rows']])
 put('3I',r['box_resource'],r['card_resource'],r['inside_resource']);put('I',len(r['confirmation_fields']))
 for f in r['confirmation_fields']:
  for key in ['box','inside','label','icon']:rect(f[key])
  put('I',f['resource'])
 rect(r['confirmation_box']);label(r['certainty']);put('I',len(r['confirmation_choices']));[label(v)for v in r['confirmation_choices']]
 put('d8I',r['palette_threshold'],*r['source_palette']);put('I',len(r['palettes']));[put('8I',*v)for v in r['palettes']];texts(r['skin_paths'])
 return struct.pack('<8s4I',b'ENCSETUI',1,24+len(raw),zlib.crc32(raw),1)+raw

def stage_files(root):
 r=json.loads(IR.read_text());base=extract();require({k:v for k,v in r.items()if k!='outputs'}==base,'Settings source recipe changed');files={Path('data/opening.encsettings'):(Path(root)/'data/opening.encsettings').read_bytes()};require(files[Path('data/opening.encsettings')]==encode(r),'Settings pack stale')
 for p,d in r['outputs'].items():
  raw=(Path(root)/p).read_bytes();require(hashlib.sha256(raw).hexdigest()==d,'Settings texture changed');files[Path(p)]=raw
 return files
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('action',choices=['assets','verify']);ap.add_argument('--tex3ds');a=ap.parse_args()
 if a.action=='verify':stage_files(ROOT/'romfs');print('Settings source/pack/assets verified')
 else:
  r=extract();build=ROOT/'build/settings-assets';build.mkdir(parents=True,exist_ok=True);r['outputs']={}
  for i,res in enumerate(r['resources']):
   image=Image.open(ROOT/'upstream/MOTHER-Encore'/res['source']).convert('RGBA');image=normalize(image,r['patch'])if res['normalize']else image;png=build/(str(i)+'.png');image.save(png);out=ROOT/'romfs'/res['path'];out.parent.mkdir(parents=True,exist_ok=True);subprocess.run([a.tex3ds,'-f','rgba8','-z','none','-o',str(out),str(png)],check=True);r['outputs'][res['path']]=sha(out)
  IR.write_text(json.dumps(r,indent=2)+'\n');PACK.write_bytes(encode(r));print('Settings UI pack',PACK.stat().st_size,'bytes')
