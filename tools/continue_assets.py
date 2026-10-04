#!/usr/bin/env python3
"""Pinned source title/LOAD adapter; source intro, naming and management excluded."""
from __future__ import annotations
import argparse,csv,hashlib,io,json,os,re,struct,subprocess,sys,zlib
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.ui_presentation_bindings import load as load_ui_bindings
from tools.extract_battle_entry import Extractor,require,one,node,properties
from tools.upstream import read_json,write_json
from tools.menu_audio_binding import source_sound
from tools import save_menu_assets
RECIPE=ROOT/'content/native-continue.json';PACK=ROOT/'romfs/data/opening.enccontinue';REPORT=ROOT/'reports/continue-menu'

def checked_bindings():return load_ui_bindings('continue-presentation-bindings.json',BINDING_SCHEMA,ROOT,expected_checks=66,expected_contracts=106)
BINDING_SCHEMA={'source_0':str,'source_1':str,'source_2':str,'source_3':str,'source_4':str,'source_5':str,'source_6':str,'source_7':str,'source_8':str,'source_9':str,'source_10':str,'source_11':str,'source_12':str,'source_13':str,'source_14':str,'source_15':str,'source_16':str,'source_17':str,'source_18':str,'node_0':str,'node_1':str,'node_2':str,'node_3':str,'node_4':str,'node_5':str,'node_6':str,'node_7':str,'node_8':str,'node_9':str,'node_10':str,'node_11':str,'node_12':str,'viewport':[int,int],'translation_keys':[str,str,str,str],'sheets':[[str,str,[int,int],int],[str,str,[int,int],int],[str,str,[int,int],int]],'sheet_root':str,'title_node_root':str,'options':[[str,str],[str,str],[str,str],[str,str]],'menu_node_root':str,'animation_ids':[int,int,int,int],'fade_animations':[[str,int],[str,int],[str,int],[str,int]],'cursor_size':[float,float],'selected_cursor_tracks':[int,int,int,int],'instant_visibility_tracks':[int,int,int],'final_hidden_tracks':[int,int,int,int,int,int,int,int,int,int,int,int,int,int,int],'shader_material':int,'selected_animation':int,'sounds':[str,str,str,str],'music_root':str,'background_color':int,'font_outline':int,'font_char_spacing':int,'font_space_spacing':int,'resource_root':str,'resource_suffix':str,'background_name':str,'layer_name_prefix':str,'selected_name_suffix':str,'tint_tracks':[int,int]}
b=checked_bindings()

SOURCES=[b['source_0'],b['source_1'],b['source_2'],b['source_3'],b['source_4'],b['source_5'],b['source_6'],b['source_7'],b['source_8'],b['source_9'],b['source_10'],b['source_11'],b['source_12'],b['source_13'],b['source_14'],b['source_15'],b['source_16']]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def sub(text,kind,ident):return properties(one(r'^\[sub_resource type="'+kind+r'" id='+str(ident)+r'\]\n(.*?)(?=^\[|\Z)',text,str(ident),re.M|re.S)[1])
def rect(p):return [p.get('margin_left',0),p.get('margin_top',0),p.get('margin_right',0)-p.get('margin_left',0),p.get('margin_bottom',0)-p.get('margin_top',0)]
def title_viewport_layout(title,image,tint):
 """Keep source pixels; add only the source's uniform outer backdrop color."""
 authored=rect(node(title,b['node_6']));require(authored[:2]==[0,0] and authored[2:]==list(image.size),'Title backdrop/reference geometry changed')
 reference=authored[2:];expanded=b['viewport'];delta=[b-a for a,b in zip(reference,expanded)]
 require(all(d>=0 and d%2==0 for d in delta),'Title viewport adaptation must preserve integer offsets')
 # Bottom pixels contain the cropped globe. Preserve that crop at the lower
 # viewport edge; only extend the reviewed uniform top/left/right edge color.
 edge=image.getpixel((0,0));require(edge[3]==255,'Title backdrop edge must be opaque')
 require(all(image.getpixel((x,0))==edge for x in range(image.width)) and all(image.getpixel((x,y))==edge for x in [0,image.width-1] for y in range(image.height)),'Title backdrop exterior is not uniform')
 color=[round(v*t)for v,t in zip(edge,tint)]
 return reference+expanded,[[0,0],[d/2 for d in delta]],[[0,0],[delta[0]/2,delta[1]]],sum(v<<(i*8)for i,v in enumerate(color))
def extract():
 global b;b=checked_bindings()
 ex=Extractor(ROOT)
 for p in SOURCES:ex.data(p)
 title=ex.text(SOURCES[0]);ts=ex.text(SOURCES[1]);scene=ex.text(SOURCES[2]);card=ex.text(SOURCES[4]);cs=ex.text(SOURCES[5]);fade=ex.text(b['source_8']);door=ex.text(b['source_11'])
 require('if globaldata.save_file != 0:'in ts and 'option = MenuOptions.LOAD'in ts,'Unreviewed title selection')
 require('save_file.activate(true)'in ex.text(SOURCES[3]) and '_play_sfx("restricted")'in ex.text(SOURCES[3]) and '"Play":\n\t\t\t\tload_game()'in cs,'Unreviewed LOAD routing')
 require('loop_around = true'in card and 'skip_empty_labels = true'in card,'Unreviewed action navigation')
 translations={r[0]:r[1]for r in csv.reader(io.StringIO(ex.text(b['source_15'])))if len(r)>1}
 texts=[translations[k]for k in b['translation_keys']]
 resources=[];layers=[]
 def add(name,source,crop=None,tint=None,flash=0):
  size=ex.png_size(source);ex.data(source+'.import');out=[crop[2],crop[3]]if crop else size
  resources.append(dict(path=b['resource_root']+name+b['resource_suffix'],source=source,width=out[0],height=out[1],columns=1,rows=1,crop=crop,tint=tint,flash=flash));return len(resources)-1
 # Explicit static sample: completed title Fade plus Shine/Glow/Spin at t=0.
 # Original node positions are exact in the reference viewport. Expanded view
 # centers the title/menu and bottom-centers the unchanged backdrop at 1:1.
 # No intro animation or new background artwork is claimed.
 final,instant,glow,shine=[sub(title,'Animation',i) for i in b['animation_ids']]
 require(all(instant[f'tracks/{i}/keys']['values']==[True] for i in b['instant_visibility_tracks']),'Title final visibility changed')
 require(all(final[f'tracks/{i}/keys']['values'][-1]is False for i in b['final_hidden_tracks']),'Final title still has intro letter sprites')
 back=node(title,b['node_0']);base=node(title,b['node_1']);origin=[base['margin_left'],base['margin_top']]
 tint=[a*b for a,b in zip(instant[f"tracks/{b['tint_tracks'][0]}/keys"]['values'][-1],glow[f"tracks/{b['tint_tracks'][1]}/keys"]['values'][0])]
 viewport,foreground_offsets,backdrop_offsets,title_background=title_viewport_layout(title,Image.open(ex.upstream/b['source_17']).convert('RGBA'),tint)
 bi=add(b['background_name'],b['source_17'],tint=tint);a=resources[bi];layers.append(dict(resource=bi,rect=[back['position'][0]-a['width']/2,back['position'][1]-a['height']/2,a['width'],a['height']]))
 for name,source,grid,frame in b['sheets']:
  p=node(title,b['title_node_root']+name);size=ex.png_size(b['sheet_root']+source);w,h=size[0]//grid[0],size[1]//grid[1];require(size[0]%grid[0]==0 and size[1]%grid[1]==0,'Title sheet geometry');idx=add(b['layer_name_prefix']+name.lower(),b['sheet_root']+source,[frame%grid[0]*w,frame//grid[0]*h,w,h]);layers.append(dict(resource=idx,rect=[origin[0]+p['position'][0]-w/2,origin[1]+p['position'][1]-h/2,w,h]))
 menu=node(title,b['node_2']);flash=float(one(r'flash_modifier", (0\.\d+)\)',ts,'highlight')[1]);title_options=[]
 for name,file in b['options']:
  p=node(title,b['menu_node_root']+name);normal=add(b['layer_name_prefix']+name.lower(), b['sheet_root']+file);selected=add(b['layer_name_prefix']+name.lower()+b['selected_name_suffix'],b['sheet_root']+file,flash=flash);a=resources[normal];title_options.append(dict(resource=normal,selected_resource=selected,rect=[origin[0]+menu['margin_left']+(p['margin_right']-a['width'])/2,origin[1]+menu['margin_top']+p.get('margin_top',0),a['width'],a['height']]))
 for layer in layers:layer['viewport_offsets']=backdrop_offsets if layer['resource']==bi else foreground_offsets
 for option in title_options:option['viewport_offsets']=foreground_offsets
 body=node(scene,b['node_3']);saves=node(scene,b['node_4']);container=node(card,b['node_5']);arrow=node(card,b['node_7'])['cursor_offset'];size=b['cursor_size']
 select=sub(scene,'Animation',b['selected_animation']);selected_cursor=[select[f'tracks/{i}/keys']['values'][0]for i in b['selected_cursor_tracks']]
 animations=[]
 for kind,ident in b['fade_animations']:
  anim=sub(fade,'Animation',ident);k=anim['tracks/0/keys'];require(len(k['times'])==len(k['values'])==len(k['transitions'])==2 and k['times'][0]==0,'Unreviewed fade keys');animations.append(dict(id=kind,length=anim.get('length',1.0),key_end=k['times'][1],from_value=k['values'][0],to_value=k['values'][1],ease=k['transitions'][0]))
 shader=sub(fade,'ShaderMaterial',b['shader_material']);fade_rect=rect(node(fade,b['node_8']))
 music=one(r'play_music_on_latest_player\("", "([^"\n]+)"\)',ts,'title music')[1]
 r=dict(schema=2,commit=ex.lock['commit'],scope='English static final title development entry and original nonembedded LOAD -> occupied card -> Play. Frozen Shine/Glow/Spin t=0 sample; intro, naming, settings, copy, delete and options are explicit unsupported boundaries. Expanded viewport centers title/menu, bottom-centers unchanged backdrop and extends its uniform tinted exterior color; reference viewport remains exact.',sources=ex.sources,resources=resources,title_layers=layers,title_options=title_options,title_background_color=title_background,texts=texts,sounds=[source_sound(s)for s in b['sounds']],title_music=b['music_root']+music,layouts=[viewport,[body['margin_left'],saves['margin_top'],body['margin_right'],0],rect(container),[arrow[0]-size[0]/6,arrow[1]+size[1]/2,0,0],selected_cursor,[fade_rect[2],fade_rect[3],shader['shader_param/screenWidth'],shader['shader_param/screenHeight']],[*node(fade,b['node_12'])['position'],0,0]],animations=animations,door_speeds=[float(one(r'fade_in_speed := ([\d.]+)',door,'door in speed')[1]),float(one(r'fade_out_speed := ([\d.]+)',door,'door out speed')[1])],door_music_fade=float(one(r'fadeout_music_length := ([\d.]+)',door,'door music fade')[1]),music_fade=float(one(r'fadeout_all_music\(([\d.]+)\)',cs,'load music fade')[1]),action_repeat=node(ex.text(b['source_7']),b['node_10'])['wait_time'],background_color=b['background_color'])
 r['layouts'].append([*foreground_offsets[0],*foreground_offsets[1]])
 probe=dict(outline=b['font_outline'],char_spacing=b['font_char_spacing'],space_spacing=b['font_space_spacing'],font=str(ex.upstream/b['source_14']),texts=texts,container={k:v for k,v in container.items()if k.startswith(('margin_','anchor_','custom_constants/'))},widths=[r['layouts'][0][0],r['layouts'][0][2]],body=r['layouts'][1],card_height=node(card,b['node_11'])['margin_bottom'],animations=animations)
 return ex,r,probe
PROBE='''extends SceneTree
func _init(): call_deferred("run")
func run():
 var f=File.new()
 assert(f.open("res://input.json",File.READ)==OK)
 var c=JSON.parse(f.get_as_text()).result
 f.close()
 var data=DynamicFontData.new()
 data.font_path=c.font
 data.antialiased=false
 var font=DynamicFont.new()
 font.font_data=data
 font.outline_size=c.outline
 font.extra_spacing_char=c.char_spacing
 font.extra_spacing_space=c.space_spacing
 var out={"engine":Engine.get_version_info(),"viewports":[]}
 for width in c.widths:
  var card=Control.new()
  card.rect_size=Vector2(width-c.body[0]+c.body[2],c.card_height)
  get_root().add_child(card)
  var box=HBoxContainer.new()
  card.add_child(box)
  for key in c.container:box.set(key,c.container[key])
  var labels=[]
  for i in range(c.texts.size()):
   var label=Label.new()
   label.text=c.texts[i]
   label.add_font_override("font",font)
   box.add_child(label)
   labels.append(label)
   if i+1<c.texts.size():
    var spacer=Label.new()
    spacer.size_flags_horizontal=3
    spacer.add_font_override("font",font)
    box.add_child(spacer)
  for _j in range(4):yield(self,"idle_frame")
  var row={"width":width,"actions":[]}
  for label in labels:row.actions.append([label.rect_position.x,label.rect_position.y,label.rect_size.x,label.rect_size.y])
  out.viewports.append(row)
  card.queue_free()
  yield(self,"idle_frame")
 out.fades=[]
 var samples=Node.new()
 get_root().add_child(samples)
 var control=Control.new()
 control.name="Sample"
 samples.add_child(control)
 var player=AnimationPlayer.new()
 samples.add_child(player)
 for i in range(c.animations.size()):
  var row=c.animations[i]
  var anim=Animation.new()
  anim.length=row.length
  var track=anim.add_track(Animation.TYPE_VALUE)
  anim.track_set_path(track,NodePath("Sample:rect_position:x"))
  anim.track_insert_key(track,0,row.from_value,row.ease)
  anim.track_insert_key(track,row.key_end,row.to_value,row.ease)
  player.add_animation("sample",anim)
  player.play("sample")
  for time in [0,row.key_end*.25,row.key_end*.5,row.key_end*.75,row.key_end,row.length]:
   player.seek(time,true)
   out.fades.append([i,time,control.rect_position.x])
  player.stop()
  player.remove_animation("sample")
 assert(f.open("res://reference.json",File.WRITE)==OK)
 f.store_string(JSON.print(out,"  "))
 f.close()
 print("CONTINUE_REFERENCE_OK")
 quit()
'''
def assets(tex3ds,godot):
 global b;b=checked_bindings()
 ex,r,probe=extract();build=ROOT/'build/continue-assets';build.mkdir(parents=True,exist_ok=True);REPORT.mkdir(parents=True,exist_ok=True);(ROOT/'romfs/graphics/ui/continue').mkdir(parents=True,exist_ok=True)
 write_json(build/'input.json',probe);(build/b['source_18']).write_text('config_version=4\n[logging]\nfile_logging/enable_logging=false\n');(build/'probe.gd').write_text(PROBE)
 run=subprocess.run([str(godot.resolve()),'--path',str(build.resolve()),'-s','probe.gd'],capture_output=True,text=True,timeout=30,env=dict(os.environ,XDG_DATA_HOME=str(build/'userdata')));(REPORT/'native-reference.log').write_text(run.stdout+run.stderr);require(run.returncode==0 and 'CONTINUE_REFERENCE_OK'in run.stdout,'Continue native reference failed')
 ref=read_json(build/'reference.json');require(ref['engine']['hash']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8','Unknown Godot');write_json(REPORT/'native-layout.json',ref);(REPORT/'native-fade-oracle.txt').write_text(''.join(' '.join(str(v)for v in row)+'\n'for row in ref['fades']));r['viewports']=ref['viewports'];r['native_reference_sha256']=sha(REPORT/'native-layout.json');r['dependencies']={'content/native-save-menu.json':sha(save_menu_assets.RECIPE),'romfs/data/opening.encsavemenu':sha(save_menu_assets.PACK)};r['outputs']={}
 for a in r['resources']:
  img=Image.open(ex.upstream/a['source']).convert('RGBA')
  if a['crop']:x,y,w,h=a['crop'];img=img.crop((x,y,x+w,y+h))
  if a['tint']:img.putdata([tuple(round(v*t)for v,t in zip(px,a['tint']))for px in img.getdata()])
  if a['flash']:img.putdata([tuple(round(v+(255-v)*a['flash'])for v in px[:3])+(px[3],)for px in img.getdata()])
  require(list(img.size)==[a['width'],a['height']],'Image geometry changed');png=build/(Path(a['path']).stem+'.png');img.save(png);target=ROOT/'romfs'/a['path'];subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(png)],check=True);r['outputs'][a['path']]={'sha256':sha(target),'bytes':target.stat().st_size}
 write_json(RECIPE,r);compile_pack(r);print('Compiled checked source Continue resources and native reference')
def encode(r):
 p=bytearray();put=lambda fmt,*x:p.extend(struct.pack('<'+fmt,*x))
 def string(s):b=s.encode('ascii');put('I',len(b));p.extend(b)
 put('2I',r['background_color'],r['title_background_color']);put('4d',*r['door_speeds'],r['music_fade'],r['door_music_fade']);put('d',r['action_repeat']);string(r['title_music'])
 for key in ['texts','sounds']:
  put('I',len(r[key]))
  for s in r[key]:string(s)
 put('I',len(r['layouts']))
 for a in r['layouts']:put('4f',*a)
 put('I',len(r['animations']))
 for a in r['animations']:put('5d',*[a[k]for k in ['length','key_end','from_value','to_value','ease']])
 put('I',len(r['resources']))
 for a in r['resources']:string(a['path']);put('4I',*[a[k]for k in ['width','height','columns','rows']])
 put('I',len(r['title_layers']))
 for a in r['title_layers']:
  put('I4f',a['resource'],*a['rect'])
  for offset in a['viewport_offsets']:put('2f',*offset)
 put('I',len(r['title_options']))
 for a in r['title_options']:
  put('2I4f',a['resource'],a['selected_resource'],*a['rect'])
  for offset in a['viewport_offsets']:put('2f',*offset)
 put('I',len(r['viewports']))
 for a in r['viewports']:
  put('fI',a['width'],len(a['actions']))
  for b in a['actions']:put('4f',*b)
 return struct.pack('<8s4I',b'ENCCONT1',2,24+len(p),zlib.crc32(p),1)+p

def verify_recipe(r):
 global b;b=checked_bindings()
 _,source,_=extract();require(set(r)==set(source)|{'viewports','native_reference_sha256','dependencies','outputs'},'Unknown continue fields')
 for k,v in source.items():require(r[k]==v,'Changed source Continue field: '+k)
 require(sha(REPORT/'native-layout.json')==r['native_reference_sha256'],'Continue native reference drift');ref=read_json(REPORT/'native-layout.json');require(r['viewports']==ref['viewports'],'Continue action layout drift');require((REPORT/'native-fade-oracle.txt').read_text()==''.join(' '.join(str(v)for v in row)+'\n'for row in ref['fades']),'Continue fade oracle drift')
 save_menu_assets.verify_recipe(read_json(save_menu_assets.RECIPE))
 for p,digest in r['dependencies'].items():require(sha(ROOT/p)==digest,'Continue shared save data drift')
 require(set(r['outputs'])=={a['path']for a in r['resources']},'Continue resource coverage')
 for p,o in r['outputs'].items():require(sha(ROOT/'romfs'/p)==o['sha256']and(ROOT/'romfs'/p).stat().st_size==o['bytes'],'Continue output drift: '+p)
def compile_pack(r=None):
 r=r or read_json(RECIPE);verify_recipe(r);PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(encode(r))
def stage_files(source_root):
 global b;b=checked_bindings()
 r=read_json(RECIPE);verify_recipe(r);root=Path(source_root).resolve();out={}
 for name in ['data/opening.enccontinue']+list(r['outputs']):
  rel=Path(name);require(not rel.is_absolute()and'..'not in rel.parts and'\\'not in name and':'not in name,'Continue stage path');p=(root/rel).resolve();require(p.is_relative_to(root),'Continue stage path escape');raw=p.read_bytes();require(raw==encode(r)if name=='data/opening.enccontinue'else hashlib.sha256(raw).hexdigest()==r['outputs'][name]['sha256'],'Stale Continue staged bytes');out[rel]=raw
 return out
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('command',choices=['assets','compile','verify']);ap.add_argument('--tex3ds',type=Path,default=Path(os.environ.get('DEVKITPRO','/opt/devkitpro'))/'tools/bin/tex3ds');ap.add_argument('--godot',type=Path,default=Path('/workspace/scratch/c6ba063dd54d/toolchain/godot-3.6.2/Godot_v3.6.2-stable_linux_headless.64'));a=ap.parse_args()
 if a.command=='assets':assets(a.tex3ds,a.godot)
 elif a.command=='compile':compile_pack()
 else:verify_recipe(read_json(RECIPE));require(PACK.read_bytes()==encode(read_json(RECIPE)),'Stale Continue pack');print('Verified checked source Continue resources and pack')
