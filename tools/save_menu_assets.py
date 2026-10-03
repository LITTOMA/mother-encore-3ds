#!/usr/bin/env python3
"""Bounded pinned-source compiler for embedded SaveSelect SAVE, never game scripts."""
from __future__ import annotations
import argparse,csv,hashlib,io,json,os,re,struct,subprocess,sys,zlib
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require,one,node,properties
from tools.upstream import read_json,write_json
from tools.menu_audio_binding import source_sound
RECIPE=ROOT/'content/native-save-menu.json';OUT=ROOT/'romfs/save-preview';PACK=ROOT/'romfs/data/opening.encsavemenu';REPORT=ROOT/'reports/save-menu'
SOURCES=['Scripts/UI/SaveSelect.gd','Maps/SaveSelect.tscn','Scripts/UI/SaveSelection/saveFile.gd','Nodes/Ui/Saves/saveFile.tscn','Nodes/Ui/arrow.tscn','Scripts/UI/cursor.gd','Graphics/UI/save_cursor_idle.tres','Scripts/global/uiManager.gd','Scripts/global/globalData.gd','Fonts/EBMain.tres','Fonts/EBMain.ttf','Fonts/BottleRocket.tres','Fonts/BottleRocket.ttf','Translations/TranslatedText/menus - sheet.csv','Shaders/MenuFlavorShader.gdshader','Shaders/MenuFlavors.tres','LICENSE']
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def rect(p):
 x,y=p.get('margin_left',0),p.get('margin_top',0);return [x,y,p.get('margin_right',0)-x,p.get('margin_bottom',0)-y]
def patch(p):return [p.get('patch_margin_'+s,0)for s in ('left','top','right','bottom')]
def color(s):return sum(v<<(8*i)for i,v in enumerate(bytes.fromhex(s)+b'\xff'))
PROBE='''extends SceneTree
func _init(): call_deferred("run")
func run():
 var f=File.new()
 assert(f.open("res://input.json",File.READ)==OK)
 var c=JSON.parse(f.get_as_text()).result
 f.close()
 var d=DynamicFontData.new()
 d.font_path=c.font
 d.antialiased=false
 var font=DynamicFont.new()
 font.font_data=d
 font.outline_size=1
 font.extra_spacing_char=-1
 font.extra_spacing_space=3
 var advances=[]
 for cp in range(32,127): advances.append(font.get_char_size(cp,32).x)
 var ebdata=DynamicFontData.new()
 ebdata.font_path=c.ebfont
 var eb=DynamicFont.new()
 eb.font_data=ebdata
 eb.extra_spacing_top=-1
 eb.extra_spacing_bottom=-1
 var root=Control.new()
 root.rect_size=Vector2(320,180)
 get_root().add_child(root)
 var menu=NinePatchRect.new()
 root.add_child(menu)
 for key in c.confirm: menu.set(key,c.confirm[key])
 var box=HBoxContainer.new()
 menu.add_child(box)
 for key in c.choices: box.set(key,c.choices[key])
 var labels=[]
 for text in c.labels:
  var label=Label.new()
  label.text=text
  label.add_font_override("font",eb)
  box.add_child(label)
  labels.append(label)
 for _i in range(4): yield(self,"idle_frame")
 var out={"engine":Engine.get_version_info(),"bottle":{"size":font.size,"height":font.get_height(),"ascent":font.get_ascent(),"advances":advances},"eb_height":eb.get_height(),"choices":[]}
 for label in labels: out.choices.append([label.rect_position.x,label.rect_position.y,label.rect_size.x,label.rect_size.y])
 assert(f.open("res://reference.json",File.WRITE)==OK)
 f.store_string(JSON.print(out,"  "))
 f.close()
 print("SAVE_MENU_REFERENCE_OK")
 quit()
'''
def extract():
 ex=Extractor(ROOT)
 for p in SOURCES:ex.data(p)
 script=ex.text(SOURCES[0]);scene=ex.text(SOURCES[1]);card=ex.text('Nodes/Ui/Saves/saveFile.tscn');cursor=ex.text('Scripts/UI/cursor.gd')
 require('enum Type { LOAD, SAVE }'in script and 'yield(get_tree(), "idle_frame")'in script and 'Tween.TRANS_QUART'in script and 'Tween.EASE_OUT'in script,'Unknown save menu execution')
 slots=int(one(r'var _max_files := 100 if OS.is_debug_build\(\) else (\d+)',script,'release slots')[1])
 spacing=int(one(r'var _save_file_height := (\d+)',script,'card spacing')[1]);activation=float(one(r'create_timer\(([\d.]+)\)',script,'activation')[1])
 require(set(re.findall(r'tween_property\([^\n]*, ([\d.]+)\)',script))=={'0.2'},'Unknown save scroll timing')
 common={r[0]:r[1]for r in csv.reader(io.StringIO(ex.text('Translations/TranslatedText/menus - sheet.csv')))if len(r)>1}
 texts=[common[k]for k in ['SAVE_NODATA','MENU_LV','SAVE_TIME','SAVE_TIME_TOO_MUCH','SAVE_SAVECONFIRM','MENU_YES','MENU_NO']]
 require(texts[3].startswith('{time} '),'Unreviewed long playtime format');texts[3]=texts[3][len('{time}'):];texts.append(' : ');texts.append(' ')
 require('" : "'in ex.text('Scripts/UI/SaveSelection/saveFile.gd'),'Changed time separator')
 flavors=json.loads(one(r'const FLAVORS := (\[[^\n]+\])',ex.text('Scripts/global/globalData.gd'),'flavors')[1]);flavorblock=one(r'var menuFlavors := \[(.*?)\n\]',ex.text('Scripts/global/uiManager.gd'),'palettes',re.S)[1];palettes=[json.loads(x)for x in re.findall(r'(\[[^\n]+?\])',flavorblock)]
 require(len(flavors)==len(palettes),'Palette count changed')
 layouts=[ [320,180,400,240],[32,node(scene,'CanvasLayer/Body/Saves')['margin_top'],-32,0],rect(node(card,'.')),patch(node(card,'.')),patch(node(scene,'CanvasLayer/Body/Cursor/cursor_menu')) ]
 for p in ['Name','Level','Title','Time','NoData','NoData/Label','FileNum','ColorRect','ColorRect2']:layouts.append(rect(node(card,p)))
 layouts += [rect(node(scene,'CanvasLayer/SaveConfirmation')),patch(node(scene,'CanvasLayer/SaveConfirmation')),rect(node(scene,'CanvasLayer/SaveConfirmation/Text')),rect(node(scene,'CanvasLayer/SaveConfirmation/HBoxContainer'))]
 arrowbase=node(ex.text('Nodes/Ui/arrow.tscn'),'.');offset=arrowbase['cursor_offset'];size=[8,8];layouts.append([offset[0]-size[0]/6,offset[1]+size[1]/2,0,0])
 first=node(card,'icons/Control1/picon1')['position'];other=node(card,'icons/Control2/picon2')['position'];top=node(card,'icons')['margin_top'];layouts.append([first[0],first[1]+top,0,0]);layouts.append([other[0],other[1]+top,node(card,'icons')['custom_constants/separation'],0])
 # Rects with right anchors store source-relative negative X; width below is
 # resolved from source card width by renderer, not a blanket scaled canvas.
 for i in [9,10]:
  if layouts[i][2]<=0: layouts[i][2]+=layouts[2][2]
 # NoData Label spans its source parent's width; neither uses its placeholder text size.
 layouts[10][2]=layouts[9][2]-layouts[10][0]
 anim=properties(ex.text('Graphics/UI/save_cursor_idle.tres').split('[resource]\n',1)[1]);keys=[]
 for j,t in enumerate(anim['tracks/0/keys']['times']):keys.append({'time':t,'margins':[anim[f'tracks/{i}/keys']['values'][j] for i in range(4)]})
 arrow_scene=ex.text('Nodes/Ui/arrow.tscn');speed=float(one(r'"speed": ([\d.]+)',arrow_scene,'arrow rate')[1]);frames=[int(x)-1 for x in re.findall(r'SubResource\( (\d) \)',one(r'"frames": \[([^\n]+)\]',arrow_scene,'arrow frames')[1])]
 resources=[]
 def add(name,source,grid=(1,1),seams=None):
  size=ex.png_size(source);ex.data(source+'.import');outsize=size[:]
  if seams:
   for i in range(2):outsize[i]=seams[i]+abs(size[i]-seams[i]-seams[i+2])+seams[i+2]
  resources.append(dict(path='save-preview/'+name+'.t3x',source=source,width=outsize[0],height=outsize[1],columns=grid[0],rows=grid[1],seams=seams));return len(resources)-1
 # Separate normalized box for prompt: card's 8px margins use the original24px.
 flavorrows=[];confirmrows=[]
 for ident,palette in zip(flavors,palettes):
  idx=add('card-'+ident.lower(),'Graphics/UI/Overworld/flavours/defaultbox.png');resources[idx]['palette']=palette
  ci=add('confirm-'+ident.lower(),'Graphics/UI/Overworld/flavours/defaultbox.png',seams=layouts[15]);resources[ci]['palette']=palette
  flavorrows.append(dict(id=ident,resource=idx,confirm_resource=ci,divider_color=color(palette[1]),background_color=color(palette[3])))
 cursorid=add('cursor','Graphics/UI/Misc/file_select_cursor.png',seams=layouts[4]);arrowid=add('arrow','Graphics/UI/Inventory/cursor.png',(3,1))
 icons=[]
 for ident in ['ninten','lloyd','ana','teddy','pippi']:icons.append(dict(id=ident,resource=add('icon-'+ident,'Graphics/UI/Inventory/characters/'+ident+'.png')))
 fontid=len(resources);resources.append(dict(path='save-preview/bottle-font.t3x',source='Fonts/BottleRocket.ttf',width=512,height=192,columns=1,rows=1,seams=None));outlineid=len(resources);resources.append(dict(resources[-1],path='save-preview/bottle-outline.t3x'))
 save_script=ex.text('Scripts/UI/SaveSelection/saveFile.gd')
 time_format=[int(one(r'playtime"\]/(3600)',save_script,'hours divisor')[1]),int(one(r'var minutes = int\(_save_data\["playtime"\]/(60)\)',save_script,'minutes divisor')[1]),int(one(r'if len\(hours\) < (\d+):',save_script,'hours padding')[1]),int(one(r'if len\(hours\) > (\d+):',save_script,'long playtime cutoff')[1])]
 recipe=dict(time_format=time_format,schema=1,commit=ex.lock['commit'],scope='English original embedded SAVE only; metadata supplied by validated native sessions. 1:1 expanded viewport follows original anchors; no management actions.',licence_review='Pinned LICENSE permits original art/fonts in this game-related fork; upstream conditions retained, no MIT relicensing.',sources=ex.sources,slot_count=slots,activation=activation,scroll=.2,cursor_loop=anim['length'],arrow_loop=len(frames)/speed,arrow_move=float(one(r'const TWEEN_LENGTH := ([\d.]+)',cursor,'arrow tween')[1]),spacing=spacing,layouts=layouts,texts=texts,sounds=['cursor1','cursor2','back'],resources=resources,font=fontid,outline=outlineid,cursor=cursorid,arrow=arrowid,text_color=color('ffffff'),time_color=color('f8f800'),outline_color=color('000000'),flavors=flavorrows,icons=icons,cursor_keys=keys,arrow_keys=[dict(time=i/speed,frame=f)for i,f in enumerate(frames)],font_recipe=dict(size=16,first=32,last=126,cell=[32,32],columns=16,outline=1,char_spacing=-1,space_spacing=3))
 def simple(p):return {k:v for k,v in p.items()if k.startswith(('margin_','anchor_','custom_constants/'))or k in ['alignment']}
 probe=dict(font=str(ex.upstream/'Fonts/BottleRocket.ttf'),ebfont=str(ex.upstream/'Fonts/EBMain.ttf'),confirm=simple(node(scene,'CanvasLayer/SaveConfirmation')),choices=simple(node(scene,'CanvasLayer/SaveConfirmation/HBoxContainer')),labels=[common['MENU_YES'],common['MENU_NO']])
 return ex,recipe,probe

def normalize(image,margins):
 if not margins:return image
 w,h=image.size;l,t,r,b=map(int,margins)
 if w-r<l:require(all(image.getpixel((x,y))==image.getpixel((w-r,y))for y in range(h)for x in range(w-r,l)),'Unreviewed nonconstant reversed horizontal seam')
 if h-b<t:require(all(image.getpixel((x,y))==image.getpixel((x,h-b))for x in range(w)for y in range(h-b,t)),'Unreviewed nonconstant reversed vertical seam')
 xs=list(range(l))+list(range(w-r,l))[::-1] if w-r<l else list(range(w-r))
 if w-r>=l:xs=list(range(l))+list(range(l,w-r))
 xs+=list(range(w-r,w))
 ys=list(range(t))+list(range(h-b,t))[::-1] if h-b<t else list(range(h-b))
 if h-b>=t:ys=list(range(t))+list(range(t,h-b))
 ys+=list(range(h-b,h))
 out=Image.new('RGBA',(len(xs),len(ys)));out.putdata([image.getpixel((x,y))for y in ys for x in xs]);return out

def assets(tex3ds,godot):
 ex,r,probe=extract();build=ROOT/'build/save-menu-assets';build.mkdir(parents=True,exist_ok=True);OUT.mkdir(parents=True,exist_ok=True);REPORT.mkdir(parents=True,exist_ok=True)
 write_json(build/'input.json',probe);(build/'project.godot').write_text('config_version=4\n[logging]\nfile_logging/enable_logging=false\n');(build/'probe.gd').write_text(PROBE)
 run=subprocess.run([str(godot.resolve()),'--path',str(build.resolve()),'-s','probe.gd'],text=True,capture_output=True,timeout=30,env=dict(os.environ,XDG_DATA_HOME=str(build/'userdata')));(REPORT/'native-reference.log').write_text(run.stdout+run.stderr);require(run.returncode==0 and 'SAVE_MENU_REFERENCE_OK'in run.stdout,'Native reference failed')
 ref=read_json(build/'reference.json');require(ref['engine']['hash']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8','Unknown Godot metrics');write_json(REPORT/'native-layout.json',ref)
 r['bottle_height']=ref['bottle']['height'];r['ebmain_height']=ref['eb_height'];r['choice_rects']=ref['choices']
 face=ImageFont.truetype(str(ex.upstream/'Fonts/BottleRocket.ttf'),16);glyphs=[];images=[Image.new('RGBA',(512,192))for _ in range(2)];draws=[ImageDraw.Draw(x)for x in images]
 for draw in draws:draw.fontmode='1'
 for i,cp in enumerate(range(32,127)):
  x,y=i%16*32,i//16*32;char=chr(cp);native=ref['bottle']['advances'][i];expected=face.getlength(char)-1+(3 if char==' 'else 0)
  require(native<=0 or abs(expected-native)<.001,'BottleRocket advance differs for '+repr(char)+': '+str((expected,native)))
  for j,draw in enumerate(draws):
   if native<=0: continue
   draw.text((x+2,y+2),char,font=face,fill='white',stroke_width=j,stroke_fill='white')
  glyphs.append(dict(codepoint=cp,u=x,v=y,width=32,height=32,advance=max(0,native),offset_x=-2,offset_y=ref['bottle']['ascent']-face.getmetrics()[0]-2))
 r['glyphs']=glyphs;battle=read_json(ROOT/'romfs/battle-preview/source.json');r['ebmain_codepoints']=[g['codepoint']for g in battle['glyphs']if g['advance']>0];r['dependencies']={'romfs/battle-preview/font.t3x':sha(ROOT/'romfs/battle-preview/font.t3x'),'romfs/battle-preview/source.json':sha(ROOT/'romfs/battle-preview/source.json')}
 old=['f3f2f4','bfb4cd','7a6c86','141117','ba53e4'];outputs={}
 for i,a in enumerate(r['resources']):
  if i in [r['font'],r['outline']]:image=images[i-r['font']]
  else:
   image=Image.open(ex.upstream/a['source']).convert('RGBA')
   if 'palette'in a:
    mapping={tuple(bytes.fromhex(o))+ (255,):tuple(bytes.fromhex(n))+(255,)for o,n in zip(old,a['palette'])};image.putdata([mapping.get(px,px)for px in image.getdata()])
   image=normalize(image,a['seams'])
  require(list(image.size)==[a['width'],a['height']],'Compiled texture dimensions mismatch');png=build/(Path(a['path']).stem+'.png');image.save(png);target=ROOT/'romfs'/a['path'];subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(png)],check=True);outputs[a['path']]=dict(sha256=sha(target),bytes=target.stat().st_size)
 r['outputs']=outputs;r['native_reference_sha256']=sha(REPORT/'native-layout.json');write_json(RECIPE,r);compile_pack(r);print('Compiled checked SaveSelect art, fonts, layout and pack')

def encode(r):
 p=bytearray();put=lambda fmt,*x:p.extend(struct.pack('<'+fmt,*x))
 def string(s):b=s.encode('ascii');put('I',len(b));p.extend(b)
 put('I5d4f',r['slot_count'],r['activation'],r['scroll'],r['cursor_loop'],r['arrow_loop'],r['arrow_move'],r['spacing'],r['bottle_height'],r['ebmain_height'],r['font_recipe']['char_spacing'])
 layouts=r['layouts']+[v for v in r['choice_rects']]+[r['time_format']];put('I',len(layouts))
 for v in layouts:put('4f',*v)
 for name in ['texts','sounds']:
  put('I',len(r[name]))
  for s in r[name]:string(source_sound(s) if name=='sounds' else s)
 put('I',len(r['resources']))
 for a in r['resources']:string(a['path']);put('4I',a['width'],a['height'],a['columns'],a['rows'])
 put('7I',*[r[k]for k in ['font','outline','cursor','arrow','text_color','time_color','outline_color']]);put('I',len(r['glyphs']))
 for g in r['glyphs']:put('5I3f',*[g[k]for k in ['codepoint','u','v','width','height','advance','offset_x','offset_y']])
 put('I',len(r['ebmain_codepoints']))
 for cp in r['ebmain_codepoints']:put('I',cp)
 put('I',len(r['flavors']))
 for f in r['flavors']:string(f['id']);put('4I',f['resource'],f['confirm_resource'],f['divider_color'],f['background_color'])
 put('I',len(r['icons']))
 for a in r['icons']:string(a['id']);put('I',a['resource'])
 put('I',len(r['cursor_keys']))
 for k in r['cursor_keys']:put('d4f',k['time'],*k['margins'])
 put('I',len(r['arrow_keys']))
 for k in r['arrow_keys']:put('dI',k['time'],k['frame'])
 return struct.pack('<8s4I',b'ENCSMENU',1,24+len(p),zlib.crc32(p),1)+p

def verify_recipe(r):
 ex,source,_=extract()
 require(set(r)==set(source)|{'bottle_height','ebmain_height','choice_rects','glyphs','ebmain_codepoints','dependencies','outputs','native_reference_sha256'},'Unknown save menu recipe fields')
 for k,v in source.items():require(r.get(k)==v,'Changed source-derived save menu field: '+k)
 require(sha(REPORT/'native-layout.json')==r['native_reference_sha256'],'Native reference changed')
 for p,digest in r['dependencies'].items():require(sha(ROOT/p)==digest,'Save menu shared font changed')
 require(set(r['outputs'])=={a['path'] for a in r['resources']},'Save menu output coverage changed')
 for p,out in r['outputs'].items():require(sha(ROOT/'romfs'/p)==out['sha256'] and (ROOT/'romfs'/p).stat().st_size==out['bytes'],'Save menu texture changed: '+p)
 ref=read_json(REPORT/'native-layout.json');require(r['choice_rects']==ref['choices'] and r['bottle_height']==ref['bottle']['height'] and r['ebmain_height']==ref['eb_height'],'Save menu native metrics mismatch')
 face=ImageFont.truetype(str(ex.upstream/'Fonts/BottleRocket.ttf'),r['font_recipe']['size']);expected=[]
 for i,cp in enumerate(range(32,127)):
  expected.append(dict(codepoint=cp,u=i%16*32,v=i//16*32,width=32,height=32,advance=max(0,ref['bottle']['advances'][i]),offset_x=-2,offset_y=ref['bottle']['ascent']-face.getmetrics()[0]-2))
 require(r['glyphs']==expected,'Save menu glyph geometry/advances mismatch')
 battle=read_json(ROOT/'romfs/battle-preview/source.json');require(r['ebmain_codepoints']==[g['codepoint']for g in battle['glyphs']if g['advance']>0],'Save menu EBMain domain mismatch')

def stage_files(source_root):
    """Return exact checked RomFS bytes; reject symlinks/path escapes and drift."""
    r=read_json(RECIPE);verify_recipe(r);source_root=Path(source_root).resolve()
    names={'data/opening.encsavemenu'}|{a['path'] for a in r['resources']}|{p.removeprefix('romfs/') for p in r['dependencies'] if p.endswith('.t3x')}
    out={}
    for name in names:
        relative=Path(name)
        require(not relative.is_absolute() and '..' not in relative.parts and '\\' not in name and ':' not in name,'Save menu stage path rejected')
        target=(source_root/relative).resolve();require(target.is_relative_to(source_root),'Save menu stage path escape')
        raw=target.read_bytes()
        if name=='data/opening.encsavemenu':require(raw==encode(r),'Stale staged save menu pack')
        else:
            digest=r['outputs'][name]['sha256'] if name in r['outputs'] else r['dependencies']['romfs/'+name]
            require(hashlib.sha256(raw).hexdigest()==digest,'Changed staged save menu resource: '+name)
        out[relative]=raw
    return out

def compile_pack(r=None):
 r=r or read_json(RECIPE);verify_recipe(r);PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(encode(r))
def main():
 ap=argparse.ArgumentParser();ap.add_argument('command',choices=['assets','compile','verify']);ap.add_argument('--tex3ds',type=Path,default=Path(os.environ.get('DEVKITPRO','/opt/devkitpro'))/'tools/bin/tex3ds');ap.add_argument('--godot',type=Path,default=Path('/workspace/scratch/c6ba063dd54d/toolchain/godot-3.6.2/Godot_v3.6.2-stable_linux_headless.64'));a=ap.parse_args()
 if a.command=='assets':assets(a.tex3ds,a.godot)
 elif a.command=='compile':compile_pack()
 else:r=read_json(RECIPE);verify_recipe(r);require(PACK.read_bytes()==encode(r),'Save menu pack differs');print('Verified source-pinned SaveSelect resources and pack')
if __name__=='__main__':main()
