#!/usr/bin/env python3
"""Extract the reviewed initial battle Items menu, without running game scripts.

The adapter is deliberately bounded to the new-game equipped Baseball Cap. Its
source gate is the immutable inventory, not a generic GDScript interpreter.
Headless Godot resolves native Control/font/container geometry; raw source
animations and their finite lowering are retained as review evidence.
"""
from __future__ import annotations
import argparse, csv, hashlib, io, json, math, os, re, struct, subprocess, sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.extract_battle_entry import Extractor, PIN, animation, node, one, properties, require
from tools.upstream import read_json, safe_path, write_json

RECIPE = ROOT / 'content/items-assets.json'
IR = ROOT / 'content/native-items.json'
OUT = ROOT / 'romfs/items-preview'
REPORT = ROOT / 'reports/items-menu-source'
NIL = 0xffffffff
PARAMETERS = ['SourceViewport','PlatformViewport','GridShape','LabelSize','CursorOffset','CursorMotion','InfoMotion','DisabledColor','NormalColor','ScrollColor','InputBinding','InputRepeat']
BATTLE = 'Nodes/Ui/Battle/Battle.tscn'
SOURCES = [BATTLE, 'Nodes/Ui/Battle/ItemsBox.gd', 'Nodes/Ui/Battle/InfoBox.gd',
    'Nodes/Ui/Battle/ActionMenuBox.gd', 'Scripts/UI/Battle/BattleMenuBox.gd',
    'Scripts/UI/Battle/BattleSystem.gd', 'Nodes/Ui/Description.tscn',
    'Scripts/UI/Reusables/Description.gd', 'Nodes/Ui/HighlightLabel.tscn',
    'Nodes/Ui/HighlightLabel.gd', 'Nodes/Ui/arrow.tscn', 'Scripts/UI/cursor.gd',
    'Nodes/Ui/Reusables/Scrollbar.tscn', 'Scripts/UI/Reusables/Scrollbar.gd',
    'Nodes/Ui/ButtonText.tscn', 'Scripts/UI/ButtonText.gd', 'Scripts/global/text_tools.gd',
    'Scripts/global/Inventory.gd', 'Scripts/global/Item.gd', 'Scripts/global/Character.gd',
    'Scripts/global/PartyMember.gd', 'Scripts/global/uiManager.gd',
    'Scripts/global/audioManager.gd', 'Scripts/global/globalData.gd',
    'Data/save_new_game.yaml', 'Data/save_overrides.yaml', 'Data/Items/BaseballCap.yaml',
    'Translations/TranslatedText/items - sheet.csv',
    'Translations/TranslatedText/menus - sheet.csv',
    'Fonts/EBMain_la.tres', 'Fonts/EBMain.ttf', 'Fonts/BottleRocket.tres',
    'Fonts/BottleRocket.ttf', 'Shaders/MenuFlavors.tres', 'project.godot', 'LICENSE']
IMAGE_SPECS = [
    ('box','Graphics/UI/Overworld/flavours/defaultbox.png','battle-preview/box.t3x',[1,1],None),
    ('cursor','Graphics/UI/Inventory/cursor.png','round-preview/dialogue_cursor.t3x',[3,1],None),
    ('cap','Graphics/Objects/Items/BaseballCap.png','items-preview/cap.t3x',[1,1],None),
    ('equipped','Graphics/UI/Inventory/modifiers.png','items-preview/equipped.t3x',[1,1],[2,19,8,7]),
    ('icon_frame','Graphics/UI/Inventory/item_icon.png','items-preview/icon-frame.t3x',[1,1],None),
    ('scroll_bg','Graphics/UI/Overworld/flavours/defaultbox_inside_scrollbar.png','items-preview/scroll-bg.t3x',[1,1],None),
    ('scroll_thumb','Graphics/UI/Overworld/flavours/defaultscroll.png','items-preview/scroll-thumb.t3x',[1,1],None),
    ('hint_box','Graphics/UI/Overworld/flavours/defaultlabel.png','items-preview/hint-box.t3x',[1,1],None),
]
LICENSE_REVIEW = 'Pinned LICENSE permits assets/music in game-related forks and modifications. Original art/font/audio are used only by this Mother: Encore port and retain upstream terms; they are not relicensed as MIT.'

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def f32(value): return struct.unpack('<f', struct.pack('<f', value))[0]
def sn(text,path): return node(text.replace('Rect2(', 'Color('),path)
def rect(props):
    x,y=props.get('margin_left',0),props.get('margin_top',0)
    return [x,y,props.get('margin_right',0)-x,props.get('margin_bottom',0)-y]
def patch(props): return [props.get('patch_margin_'+side,0) for side in ['left','top','right','bottom']]
def rgba(hexcode): return [int(hexcode[i:i+2],16)/255 for i in [0,2,4]]+[1]

def reviewed_extract():
    ex=Extractor(ROOT)
    for path in SOURCES: ex.data(path)
    for _,path,_,_,_ in IMAGE_SPECS:
        ex.data(path);ex.data(path+'.import')
    battle=ex.text(BATTLE); items=ex.text('Nodes/Ui/Battle/ItemsBox.gd')
    cap=ex.yaml('Data/Items/BaseballCap.yaml'); save=ex.yaml('Data/save_new_game.yaml')
    require(save['party']==['ninten'] and save['ninten']['inventory']==[{'item_name':'BaseballCap','equipped':True}], 'Unreviewed initial inventory')
    require(cap['actions']==[{'function':'equip'}] and cap['can_use']==['ninten'] and cap['slot']=='other' and not cap['keyitem'] and not cap['HPrecover'] and not cap['PPrecover'] and not cap.get('battle_action') and not cap.get('status_heals'), 'Initial item requires unsupported behavior')
    columns=int(one(r'^const ITEM_PAGE_SIZE_X := (\d+)$',items,'item columns')[1])
    rows=int(one(r'^const ITEM_PAGE_SIZE_Y := (\d+)$',items,'item rows')[1])
    require((columns,rows)==(2,5),'Unreviewed item grid')
    require('item.is_battle_usable() and _user.character.can_use_item(item)' in items and 'else uiManager.get_flavor_color(3)' in items,'Unreviewed selectability/color')
    inventory=ex.text('Scripts/global/Inventory.gd')
    capacity=int(one(r'^const _MAX_INVENTORY_SIZE := (\d+)$',inventory,'inventory capacity')[1])
    slots=json.loads(one(r'^const SLOTS := (\[[^\n]+\])$',inventory,'equipment slots')[1])
    dose=int(one(r'self.doses = get_data\(\).get\("doses", (\d+)\)',ex.text('Scripts/global/Item.gd'),'item default doses')[1])
    table={row[0]:row[1] for row in csv.reader(io.StringIO(ex.text('Translations/TranslatedText/items - sheet.csv'))) if len(row)>1}
    item_value=next((value for value in cap['boost'].values() if value>0),0)
    description=table[cap['description']].replace('\\n','\n').replace('[ItemValue]',str(item_value))
    require('[' not in description and ']' not in description,'Unknown description tag')
    common={row[0]:row[1] for row in csv.reader(io.StringIO(ex.text('Translations/TranslatedText/menus - sheet.csv'))) if len(row)>1}
    require(common['WORD_SEPARATOR']==' ','Unreviewed English word separator')
    flavor=json.loads(one(r'var menuFlavors := \[\s*(\[[^\n]+?\]),\s*# Plain',ex.text('Scripts/global/uiManager.gd'),'plain flavor',re.S)[1])
    clipnodes=sn(battle,'ItemsBox/AnimationPlayer')
    clips=[animation(battle,clipnodes['anims/'+key]['SubResource'],BATTLE,key) for key in ['Open','Close']]
    require(all(c['length']==.1 and not c['loop'] for c in clips),'Unreviewed menu duration')
    arrow=ex.text('Nodes/Ui/arrow.tscn')
    body=one(r'^\[sub_resource type="SpriteFrames" id=4\]\n(.*?)(?=^\[|\Z)',arrow,'arrow frames',re.M|re.S)[1]
    idle=properties(body)['animations'];require(len(idle)==1 and idle[0]['name']=='Idle' and idle[0]['loop'],'Unreviewed cursor frames')
    frames=[]
    for reference in idle[0]['frames']:
        rid=reference['SubResource'];match=one(r'^\[sub_resource type="AtlasTexture" id='+str(rid)+r'\]\n(.*?)(?=^\[|\Z)',arrow,'arrow atlas',re.M|re.S)
        region=properties(match[1].replace('Rect2(', 'Color('))['region']
        require(region[1:]==[0,8,8] and region[0]%8==0,'Unreviewed cursor atlas');frames.append(int(region[0]/8))
    info=ex.text('Nodes/Ui/Battle/InfoBox.gd')
    require('var _is_visible := true' in info and 'event.is_action_pressed("ui_scope")' in info,'Unreviewed info toggle')
    require(info.count('0.1) \\')==2 and info.count('.set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)')==2,'Unreviewed info tween')
    paths={};manager=ex.text('Scripts/global/audioManager.gd');system=ex.text('Scripts/UI/Battle/BattleSystem.gd')
    for key in ['cursor1','cursor2','restricted']:
        paths[key]=one(r'"'+key+r'": load\("res://([^"\n]+)"\)',manager,key+' audio')[1]
    paths['back']=one(r'"back": load\("res://([^"\n]+)"\)',system,'battle back audio')[1]
    for path in paths.values():ex.data(path);ex.data(path+'.import')
    definitions=[dict(id=1,source='BaseballCap',name=table[cap['name']],description=description,icon=2,equipment_slot=slots.index(cap['slot']),heal_hp=cap['HPrecover'],heal_pp=cap['PPrecover'],max_hp_boost=cap['boost']['maxhp'],max_pp_boost=cap['boost']['maxpp'],flags=1,can_use=1)]
    raw=dict(layout={p:sn(battle,p) for p in ['ItemsBox','ItemsBox/GridContainer','ItemsBox/Arrow','ItemsBox/Arrow/ColorRect','ItemsBox/Scrollbar','InfoBox','InfoBox/Description','InfoBox/PPCost','InfoBox/PPCost/MarginContainer','InfoBox/PPCost/MarginContainer/ButtonText','PlayerInfo/PlayerInfoVbox/PartyInfo']},clips=clips,cursor_idle=dict(frames=frames,fps=idle[0]['speed']),item_value=item_value,cap=cap,audio_paths=paths,description=description,columns=columns,rows=rows,capacity=capacity,doses=dose,flavor=flavor)
    return ex,definitions,raw

def make_recipe(ex):
    resources=[]
    for i,(name,source,output,grid,crop) in enumerate(IMAGE_SPECS):
        spec=dict(id=i+1,name=name,source=source,size=ex.png_size(source),grid=grid,output=output,reuse=not output.startswith('items-preview/'))
        if crop:spec['crop']=crop
        resources.append(spec)
    resources.append(dict(id=9,name='hint_glyph',source='Fonts/BottleRocket.ttf',size=[16,16],grid=[1,1],output='items-preview/hint-glyph.t3x',reuse=False,glyph='L'))
    resources.append(dict(id=10,name='font',source='Fonts/EBMain.ttf',size=[384,144],grid=[1,1],output='battle-preview/font.t3x',reuse=True))
    for name,rotation in [('cursor_up',1),('cursor_down',3)]:
        resources.append(dict(id=len(resources)+1,name=name,source='Graphics/UI/Inventory/cursor.png',size=ex.png_size('Graphics/UI/Inventory/cursor.png'),grid=[3,1],output='items-preview/'+name+'.t3x',reuse=False,quarter_turns=rotation))
    return dict(schema=1,commit=PIN,game_version=ex.lock['game_version'],licence_review=LICENSE_REVIEW,sources=ex.sources,resources=resources)

def validate_source(root,recipe,lock):
    require(set(recipe)=={'schema','commit','game_version','licence_review','sources','resources'} and recipe['schema']==1 and recipe['commit']==lock['commit']==PIN and recipe['game_version']==lock['game_version'] and recipe['licence_review']==LICENSE_REVIEW,'Unreviewed Items source/permission/schema')
    require(bool(recipe['sources']) and bool(recipe['resources']),'Missing Items source/resource review')
    for path,digest in recipe['sources'].items():require(sha(safe_path(root,path))==digest,'Changed Items source: '+path)
    names=set();paths=set()
    for i,r in enumerate(recipe['resources']):
        required={'id','name','source','size','grid','output','reuse'}
        require(set(r) in [required,required|{'crop'},required|{'glyph'},required|{'quarter_turns'}] and r['id']==i+1 and r['source'] in recipe['sources'] and type(r['reuse']) is bool,'Unknown Items resource fields/order')
        require(r['name'] not in names and r['output'] not in paths,'Duplicate Items resource');names.add(r['name']);paths.add(r['output']);safe_path(ROOT/'romfs',r['output'])
        for key in ['size','grid']:require(len(r[key])==2 and all(type(v) is int and 0<v<=1024 for v in r[key]),'Invalid Items resource geometry')
        require(all(s%g==0 for s,g in zip(r['size'],r['grid'])),'Invalid Items atlas')
        if 'crop' in r:
            x,y,w,h=r['crop'];require(all(type(v)is int for v in r['crop']) and x>=0 and y>=0 and w>0 and h>0 and x+w<=r['size'][0] and y+h<=r['size'][1],'Invalid Items crop')
        if 'glyph' in r:require(r['glyph']=='L' and r['source']=='Fonts/BottleRocket.ttf' and not r['reuse'],'Unreviewed hint glyph')
        if 'quarter_turns' in r:require(r['quarter_turns'] in [1,3] and r['grid']==[3,1] and r['size']==[24,8] and not r['reuse'],'Unreviewed cursor rotation')
        require(r['reuse'] == (not r['output'].startswith('items-preview/')),'Invalid Items reuse destination')

PROBE = '''extends SceneTree
var config
var native_nodes = {}
func _init(): call_deferred("run")
func run():
 assert(Engine.get_version_info().hash == "3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8")
 var file = File.new()
 assert(file.open("res://probe-input.json", File.READ) == OK)
 config = JSON.parse(file.get_as_text()).result
 file.close()
 var data = DynamicFontData.new()
 data.font_path = config.font_path
 var font = DynamicFont.new()
 font.font_data = data
 font.size = 16
 font.extra_spacing_top = -1
 font.extra_spacing_bottom = -1
 var hint_data = DynamicFontData.new()
 hint_data.antialiased = false
 hint_data.font_path = config.hint_font_path
 var hint_font = DynamicFont.new()
 hint_font.font_data = hint_data
 hint_font.outline_size = 1
 hint_font.extra_spacing_char = -1
 hint_font.extra_spacing_space = 3
 for row in config.nodes:
  var n = ClassDB.instance(row.type)
  n.name = row.name
  if row.parent == "": get_root().add_child(n)
  else: native_nodes[row.parent].add_child(n)
  native_nodes[row.path] = n
  for key in row.props:
   var value = row.props[key]
   if typeof(value) == TYPE_ARRAY: value = Vector2(value[0],value[1])
   n.set(key,value)
  if n is Label:
   n.add_font_override("font",hint_font if row.path.ends_with("ButtonText") else font)
   if row.path.ends_with("ButtonText"): n.text = "L"
  if n is RichTextLabel: n.add_font_override("normal_font",font)
  if n is PanelContainer:
   var style = StyleBoxEmpty.new()
   for side in 4: style.set_default_margin(side,4)
   n.add_stylebox_override("panel",style)
 for _frame in range(4): yield(self,"idle_frame")
 var desc = native_nodes["InfoBox/Description/HBox/MarginContainer/Desc"]
 desc.bbcode_text = add_line_breaks(config.description,desc)
 for _frame in range(4): yield(self,"idle_frame")
 var result = {"engine":Engine.get_version_info(),"nodes":{},"description":desc.bbcode_text,"font_height":font.get_height(),"font_ascent":font.get_ascent(),"line_widths":[]}
 for path in native_nodes:
  var n = native_nodes[path]
  result.nodes[path] = {"rect":[n.rect_position.x,n.rect_position.y,n.rect_size.x,n.rect_size.y],"minimum":[n.get_combined_minimum_size().x,n.get_combined_minimum_size().y]}
 for line in desc.bbcode_text.split("\\n"): result.line_widths.append(font.get_string_size(line).x)
 var menu_item = native_nodes["ItemsBox/GridContainer/Item0"]
 var cursor_size = Vector2(8,8)
 cursor_size.y = cursor_size.y/2.0
 cursor_size.x = -cursor_size.x/6.0
 var cursor_pos = menu_item.rect_global_position + Vector2(-5,1) + cursor_size
 result.cursor_position = [cursor_pos.x,cursor_pos.y]
 result.hint_metrics = {"size":hint_font.size,"height":hint_font.get_height(),"ascent":hint_font.get_ascent(),"advance":hint_font.get_string_size("L").x}
 assert(file.open("res://reference.json",File.WRITE)==OK)
 file.store_string(JSON.print(result,"  "))
 file.close()
 print("ITEMS_NATIVE_LAYOUT_COMPLETE")
 quit()
func _tr(_key) -> String: return " "
'''

def run_native_reference(ex,raw,godot,build):
    build.mkdir(parents=True,exist_ok=True)
    desc=ex.text('Nodes/Ui/Description.tscn');battle=ex.text(BATTLE);label=ex.text('Nodes/Ui/HighlightLabel.tscn')
    nodes=[]
    def add(path,kind,props):
        selected={k:v for k,v in props.items() if k.startswith(('margin_','anchor_','rect_min_size','custom_constants/','size_flags_')) or k in ['columns','alignment','align','valign','fit_content_height','scroll_active','bbcode_enabled','max_lines_visible','grow_horizontal','grow_vertical']}
        if 'rect_min_size' in selected:selected['rect_min_size']=[float(v) for v in selected['rect_min_size']]
        nodes.append(dict(path=path,name=path.split('/')[-1],parent=path.rsplit('/',1)[0] if '/' in path else '',type=kind,props=selected))
    for p,t in [('ItemsBox','NinePatchRect'),('ItemsBox/GridContainer','GridContainer')]:add(p,t,sn(battle,p))
    for i in range(raw['columns']*raw['rows']):
        props=sn(label,'.');props.update(sn(battle,'ItemsBox/GridContainer/Item'+str(i)))
        add('ItemsBox/GridContainer/Item'+str(i),'Label',props)
    for p,t in [('InfoBox','NinePatchRect'),('InfoBox/Description','Control')]:
        props=sn(battle,p)
        if p.endswith('/Description'):base=sn(desc,'.');base.update(props);props=base
        add(p,t,props)
    for p,t in [('HBox','HBoxContainer'),('HBox/TextureRect','NinePatchRect'),('HBox/MarginContainer','MarginContainer'),('HBox/MarginContainer/Desc','RichTextLabel')]:add('InfoBox/Description/'+p,t,sn(desc,p))
    for p,t in [('InfoBox/PPCost','PanelContainer'),('InfoBox/PPCost/MarginContainer','MarginContainer'),('InfoBox/PPCost/MarginContainer/ButtonText','Label')]:
        props=sn(battle,p)
        if p.endswith('/ButtonText'):base=sn(ex.text('Nodes/Ui/ButtonText.tscn'),'.');base.update(props);props=base
        add(p,t,props)
    add('PlayerInfo','Control',{})
    add('PlayerInfo/PlayerInfoVbox','VBoxContainer',sn(battle,'PlayerInfo/PlayerInfoVbox'))
    add('PlayerInfo/PlayerInfoVbox/PartyInfo','Control',sn(battle,'PlayerInfo/PlayerInfoVbox/PartyInfo'))
    text=ex.text('Scripts/global/text_tools.gd')
    linebreak=one(r'^static func add_line_breaks\(.*?\n(.*?)(?=^static func)',text,'line wrapping',re.M|re.S)[0].replace('static func','func')
    strip=one(r'^static func strip_bbcode\(.*?\n(.*?)(?=^static func)',text,'strip markup',re.M|re.S)[0].replace('static func','func')
    write_json(build/'probe-input.json',dict(nodes=nodes,description=raw['description'],font_path=str(ex.upstream/'Fonts/EBMain.ttf'),hint_font_path=str(ex.upstream/'Fonts/BottleRocket.ttf')))
    (build/'project.godot').write_text('config_version=4\n[logging]\nfile_logging/enable_logging=false\n')
    (build/'probe.gd').write_text(PROBE+linebreak+strip)
    result=subprocess.run([str(godot.resolve()),'--path',str(build.resolve()),'-s','probe.gd'],capture_output=True,text=True,timeout=25,env=dict(os.environ,XDG_DATA_HOME=str((build/'userdata').resolve())))
    REPORT.mkdir(parents=True,exist_ok=True);(REPORT/'godot-layout.log').write_text(result.stdout+result.stderr)
    require(result.returncode==0 and 'ITEMS_NATIVE_LAYOUT_COMPLETE' in result.stdout,'Native Items layout reference failed')
    reference=read_json(build/'reference.json');write_json(REPORT/'native-layout.json',reference)
    require(reference['nodes']['PlayerInfo/PlayerInfoVbox']['minimum'][1]==68,'Source info hide offset changed')
    require(reference['description']==raw['description'],'Unexpected initial description wrapping')
    return reference

def rotate_frames(image,grid,quarter_turns):
    require(quarter_turns in [1,3] and grid==[3,1] and image.size==(24,8),'Unreviewed frame rotation')
    result=Image.new('RGBA',image.size)
    for i in range(3):
        frame=image.crop((i*8,0,i*8+8,8)).transpose(Image.Transpose.ROTATE_90 if quarter_turns==1 else Image.Transpose.ROTATE_270)
        result.paste(frame,(i*8,0))
    return result

def compile_assets(ex,recipe,tex3ds,reference,build):
    validate_source(ex.upstream,recipe,ex.lock);OUT.mkdir(parents=True,exist_ok=True)
    dependencies={};resources=[];outputs={}
    for r in recipe['resources']:
        target=safe_path(ROOT/'romfs',r['output']);width,height=r['size']
        if r['reuse']:
            receiptpath='romfs/round-preview/source.json' if r['name']=='cursor' else 'romfs/battle-preview/source.json'
            receipt=read_json(ROOT/receiptpath);dependencies[receiptpath]=sha(ROOT/receiptpath)
            output=receipt['outputs'][target.name];require(sha(target)==output['sha256'] and target.stat().st_size==output['bytes'],'Changed reused Items resource')
            source=next(x for x in receipt['resources'] if x['name']==('dialogue_cursor' if r['name']=='cursor' else r['name']))
            require(source['width']==width and source['height']==height,'Reused Items resource geometry mismatch')
        else:
            if 'glyph' in r:
                metrics=reference['hint_metrics'];font=ImageFont.truetype(str(ex.upstream/r['source']),metrics['size'])
                # DynamicFont extra_spacing_char is between characters, not
                # after the final single-character source button label.
                require(abs(font.getlength('L')-metrics['advance'])<.001,'Hint font advance differs from native')
                image=Image.new('RGBA',r['size']);draw=ImageDraw.Draw(image)
                draw.fontmode='1';draw.text((1,1),r['glyph'],font=font,fill='white',stroke_width=1,stroke_fill='black')
            else:
                image=Image.open(ex.upstream/r['source']).convert('RGBA');require(list(image.size)==r['size'],'Changed Items image dimensions')
                if 'crop' in r:x,y,width,height=r['crop'];image=image.crop((x,y,x+width,y+height))
                if 'quarter_turns' in r:image=rotate_frames(image,r['grid'],r['quarter_turns'])
            png=build/(r['name']+'.png');image.save(png)
            subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(png)],check=True)
        resources.append(dict(id=r['id'],path=r['output'],kind=1,width=width,height=height,columns=r['grid'][0],rows=r['grid'][1],sha256=sha(target)))
        if not r['reuse']:outputs[target.name]=dict(bytes=target.stat().st_size,sha256=sha(target))
    receipt=dict(schema=1,recipe=recipe,dependencies=dependencies,resources=resources,outputs=outputs,tex3ds_sha256=sha(tex3ds),native_reference_sha256=sha(REPORT/'native-layout.json'),native_helper_sha256=sha(build/'probe.gd'),native_input_sha256=sha(build/'probe-input.json'),limits='Source-pinned lossless textures and native headless layout/font evidence only; no GPU, emulator, audibility, or hardware claim')
    write_json(OUT/'source.json',receipt)
    return resources,dependencies

def export_ir(ex,definitions,raw,reference,resources,dependencies,write=True):
    layouts=[]
    def add(role,kind,rectangle,parent=NIL,resource=NIL,anchor=(0,0),frame=0,flags=1,color=(1,1,1,1),patches=(0,0,0,0)):
        index=len(layouts);layouts.append(dict(id=index+1,role=role,parent=parent,kind=kind,resource=resource,frame=frame,flags=flags,anchor=list(anchor),rect=list(rectangle),color=list(color),patch=list(patches)));return index
    # Anchors extend the original320x180 canvas while retaining1:1 native pixels.
    panel=add('Panel','NinePatch',rect(raw['layout']['ItemsBox']),resource=0,anchor=(.5,0),patches=patch(raw['layout']['ItemsBox']))
    grid=add('Grid','Container',reference['nodes']['ItemsBox/GridContainer']['rect'],panel)
    label_scene=ex.text('Nodes/Ui/HighlightLabel.tscn');marker=sn(label_scene,'Equipped_spr');marker_size=resources[3]['width'],resources[3]['height']
    for i in range(raw['columns']*raw['rows']):
        label=add('ItemLabel','Text',reference['nodes']['ItemsBox/GridContainer/Item'+str(i)]['rect'],grid,9,frame=i)
        add('Equipped','Sprite',[*marker['position'],*marker_size],label,3,frame=0,flags=5)
    native_cursor=reference['cursor_position'];panel_rect=layouts[panel]['rect']
    cursor=add('Cursor','Sprite',[native_cursor[0]-panel_rect[0],native_cursor[1]-panel_rect[1],8,8],panel,1,flags=5)
    back=raw['layout']['ItemsBox/Arrow/ColorRect'];r=rect(back)
    # The backplate is behind the centered cursor, and must be drawn before it.
    cursor_back=add('Cursor','Rectangle',r,cursor,color=back['color'],flags=9)
    scroll=add('Scrollbar','Container',rect(raw['layout']['ItemsBox/Scrollbar']),panel)
    scrollbar=ex.text('Nodes/Ui/Reusables/Scrollbar.tscn');scrollbg=sn(scrollbar,'ScrollBG');thumb=sn(scrollbar,'ScrollBG/Thumb')
    sr=layouts[scroll]['rect'];br=rect(scrollbg);br[2]+=sr[2];br[3]+=sr[3]
    sb=add('ScrollBackground','NinePatch',br,scroll,5,color=scrollbg['self_modulate'],patches=patch(scrollbg))
    tr=rect(thumb);tr[2]+=br[2];add('ScrollThumb','NinePatch',tr,sb,6,patches=patch(thumb))
    scroll_arrows=[]
    for source,y,res in [('UpArrow/arrow',4,10),('DownArrow/arrow',sr[3]-4,11)]:
        angle=sn(scrollbar,source)['rotation'];require(abs(abs(angle)-math.pi/2)<.00001,'Unreviewed scrollbar arrow rotation')
        scroll_arrows.append(add('Cursor','Sprite',[sr[2]/2,y,8,8],scroll,res,frame=0,flags=5))
    info=add('InfoPanel','NinePatch',rect(raw['layout']['InfoBox']),resource=0,anchor=(.5,1),patches=patch(raw['layout']['InfoBox']))
    desc=reference['nodes'];prefix='InfoBox/Description'
    d=desc[prefix]['rect'];h=desc[prefix+'/HBox']['rect'];im=desc[prefix+'/HBox/TextureRect']['rect']
    iconframe=add('Panel','NinePatch',[d[0]+h[0]+im[0],d[1]+h[1]+im[1],im[2],im[3]],info,4,patches=patch(sn(ex.text('Nodes/Ui/Description.tscn'),'HBox/TextureRect')))
    iw,ih=resources[2]['width'],resources[2]['height'];add('ItemIcon','Sprite',[im[2]/2,im[3]/2,iw,ih],iconframe,2,flags=5)
    mc=desc[prefix+'/HBox/MarginContainer']['rect'];tx=desc[prefix+'/HBox/MarginContainer/Desc']['rect']
    line_height=reference['font_height']+1
    for i,line in enumerate(reference['description'].split('\n')):
        add('Description','Text',[d[0]+h[0]+mc[0]+tx[0],d[1]+h[1]+mc[1]+tx[1]+i*line_height,tx[2],reference['font_height']],info,9,frame=i)
    # Native platform binding is explicit port data; glyph still uses sourcefont.
    hr=reference['nodes']['InfoBox/PPCost']['rect']
    hintbox=add('Hint','NinePatch',hr,info,7,patches=(4,4,4,4))
    hm=reference['nodes']['InfoBox/PPCost/MarginContainer']['rect'];hl=reference['nodes']['InfoBox/PPCost/MarginContainer/ButtonText']['rect']
    add('Hint','Sprite',[hm[0]+hl[0]-1,hm[1]+hl[1]-1,16,16],hintbox,8)
    clips=[]
    for source in raw['clips']:
        tracks=[]
        for trk in source['tracks']:
            require(trk['type']=='value' and trk['interp']==1 and trk['path'] in ['.:rect_position','.:visible'],'Unreviewed Items animation property')
            keys=[]
            for t,e,v in zip(trk['keys']['times'],trk['keys']['transitions'],trk['keys']['values']):
                value=v if isinstance(v,list) else [int(v)]
                keys.append(dict(time=t,ease=e,value=value+[0]*(4-len(value))))
            tracks.append(dict(target=panel,property='Position' if trk['path']=='.:rect_position' else 'Visible',interpolation=trk['keys']['update'],keys=keys))
        clips.append(dict(id=len(clips)+1,role=source['name'],duration=source['length'],loop=source['loop'],tracks=tracks))
    frames=raw['cursor_idle']['frames'];fps=raw['cursor_idle']['fps']
    clips.append(dict(id=len(clips)+1,role='CursorIdle',duration=len(frames)/fps,loop=True,tracks=[dict(target=target,property='Frame',interpolation=1,keys=[dict(time=i/fps,ease=1,value=[frame,0,0,0]) for i,frame in enumerate(frames)]) for target in [cursor]+scroll_arrows]))
    cursor_src=ex.text('Scripts/UI/cursor.gd');timer=sn(ex.text('Nodes/Ui/arrow.tscn'),'Timer')['wait_time']
    move=float(one(r'^const TWEEN_LENGTH := ([0-9.]+)',cursor_src,'cursor move tween')[1])
    viewport=[int(one(r'^window/size/'+k+r'=(\d+)$',ex.text('project.godot'),'viewport'+k)[1]) for k in ['width','height']]
    pitchx=reference['nodes']['ItemsBox/GridContainer/Item1']['rect'][0]
    pitchy=reference['nodes']['ItemsBox/GridContainer/Item2']['rect'][1]
    cursor_offset=raw['layout']['ItemsBox/Arrow']['cursor_offset']
    adapter=read_json(ROOT/'content/platform-input.json')
    require(set(adapter)=={'schema','reason','menu_repeat'} and adapter['schema']==1
            and isinstance(adapter['reason'],str) and adapter['reason'], 'Invalid platform input adapter')
    timing=adapter['menu_repeat']
    require(isinstance(timing,list) and len(timing)==4 and all(isinstance(v,(int,float)) for v in timing)
            and 0 < timing[1] <= timing[0] <= 10 and timing[2:]==[0,0], 'Invalid platform menu repeat timing')
    params=dict(SourceViewport=viewport+[0,0],PlatformViewport=[400,240,0,0],GridShape=[raw['columns'],raw['rows'],pitchx,pitchy],LabelSize=reference['nodes']['ItemsBox/GridContainer/Item0']['rect'][2:]+[0,0],CursorOffset=[f32(cursor_offset[0]+f32(-8/6)),cursor_offset[1]+4,8,8],CursorMotion=[move,timer,fps,0],InfoMotion=[.1,reference['nodes']['PlayerInfo/PlayerInfoVbox']['minimum'][1],0,0],DisabledColor=rgba(raw['flavor'][2]),NormalColor=[1,1,1,1],ScrollColor=scrollbg['self_modulate'],InputBinding=[6,16777238,512,0],InputRepeat=timing)
    require(set(params)==set(PARAMETERS),'Incomplete Items parameters')
    definitions[0]['description']=reference['description']
    sounds=[dict(event=event,path=raw['audio_paths'][name],audio_id=ident) for event,name,ident in [('Open','cursor2',1102),('Move','cursor1',1101),('Close','back',1103),('Disabled','restricted',1104),('Confirm','cursor2',1102)]]
    result=dict(schema=1,kind='encore.native-items.source-ir',commit=PIN,sources=ex.sources,dependencies=dependencies,scope='Bounded initial single-owner inventory: equipped Baseball Cap, source list/info/cancel/restricted feedback only; no consumable action, random UID creation, saving, or additional inventory source. Pixel geometry retains320x180 with centered-top/bottom anchoring at400x240. Owner/item/instance IDs are deterministic adapter identities. Native L maps original ui_scope left trigger.',capacity=raw['capacity'],owner=1,definitions=definitions,initial_inventory=[dict(id=1,definition=0,equipped=True,doses=raw['doses'])],resources=resources,layouts=layouts,parameters=params,clips=clips,sounds=sounds)
    if write:
        write_json(IR,result)
        write_json(REPORT/'source-review.json',dict(schema=1,commit=PIN,sources=ex.sources,dependencies=dependencies,raw=raw,native_reference_sha256=sha(REPORT/'native-layout.json'),resource_roles={r[0]:i for i,r in enumerate(IMAGE_SPECS)}|{'hint_glyph':8,'font':9,'cursor_up':10,'cursor_down':11},layout_bindings=dict(menu=panel,grid=grid,cursor=cursor,cursor_back=cursor_back,scrollbar=scroll,info=info,hint=hintbox),semantics={'disabled_color':'uiManager.get_flavor_color(3) is one-based NEWCOLOR3','close_visibility':'ItemsBox.hide schedules Close then BattleMenuBox.hide immediately hides; retain source clip but do not promise a visible close tween','cursor_position':'Native float vector operations; source centered origin retained with Centered flag; cursor backplate retains show_behind_parent','scroll_arrows':'Source +/-1.5708 rotations mapped to nearest-sampled lossless90-degree per-frame rotation; headless initial single-row scrollbar hidden','description':'Original TextTools.add_line_breaks and strip_bbcode executed with native font in isolated headless helper','scope_binding':'Source joypadbutton6 left trigger → native3DS L512; explicit adaptation','hint':'Source ButtonText custom BottleRocket font and native Panel/MarginContainer/Label layout; platform scope labelL rasterized monochrome with source outline1; noGPU raster equality claim'},limits=['No GUI/screenshot capture','No pixel or audibility comparison','No hardware validation']))
    return result

def verify(root=ROOT/'upstream/MOTHER-Encore'):
    recipe=read_json(RECIPE);validate_source(root,recipe,read_json(ROOT/'upstream.lock'))
    receipt=read_json(OUT/'source.json');require(receipt['schema']==1 and receipt['recipe']==recipe,'Stale Items receipt')
    expected={Path(r['output']).name for r in recipe['resources'] if not r['reuse']}
    require(set(receipt['outputs'])==expected and {p.name for p in OUT.iterdir()}==expected|{'source.json'},'Missing/unexpected Items output')
    for path,digest in receipt['dependencies'].items():require(sha(ROOT/path)==digest,'Changed Items dependency '+path)
    for r in receipt['resources']:require(sha(safe_path(ROOT/'romfs',r['path']))==r['sha256'],'Changed Items resource '+r['path'])
    for name,r in receipt['outputs'].items():require((OUT/name).stat().st_size==r['bytes'],'Changed Items output length')
    ir=read_json(IR);report=read_json(REPORT/'source-review.json')
    require(ir['sources']==recipe['sources']==report['sources'] and ir['dependencies']==report['dependencies']==receipt['dependencies'],'Stale Items provenance')
    require(ir['resources']==receipt['resources'] and set(ir['parameters'])==set(PARAMETERS),'Stale Items resource/parameter data')
    require(sha(REPORT/'native-layout.json')==receipt['native_reference_sha256']==report['native_reference_sha256'],'Changed native Items reference')
    # Regenerate semantics from the pinned sources rather than trusting editedIR.
    ex,definitions,raw=reviewed_extract();reference=read_json(REPORT/'native-layout.json')
    require(ex.sources==recipe['sources'] and raw==report['raw'],'Changed Items extraction')
    require(reference['engine']['hash']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8','Unknown Items native reference version')
    expected=export_ir(ex,definitions,raw,reference,receipt['resources'],receipt['dependencies'],write=False)
    require(expected==ir,'Items IR differs from reviewed source-derived content')
    print('Verified source-pinned Items art, IR dependencies and native layout provenance')

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('action',choices=['compile','verify']);parser.add_argument('--tex3ds',type=Path);parser.add_argument('--godot',type=Path);args=parser.parse_args()
    try:
        if args.action=='verify':verify();return 0
        require(args.tex3ds is not None and args.godot is not None,'Need official tex3ds and pinned headless Godot')
        ex,definitions,raw=reviewed_extract();build=ROOT/'build/items-assets'
        reference=run_native_reference(ex,raw,args.godot,build)
        recipe=make_recipe(ex);write_json(RECIPE,recipe)
        resources,deps=compile_assets(ex,recipe,args.tex3ds,reference,build)
        export_ir(ex,definitions,raw,reference,resources,deps);verify()
        print('Compiled source-derived initial Items menu IR and original assets')
        return 0
    except (ValueError,OSError,KeyError,TypeError,subprocess.SubprocessError) as error:
        print('ITEMS ASSET ERROR:',error,file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
