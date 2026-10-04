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
from tools.asset_receipts import receipt_path, receipt_entries

RECIPE = ROOT / 'content/items-assets.json'
IR = ROOT / 'content/native-items.json'
OUT = ROOT / 'romfs/graphics/ui/items'
REPORT = ROOT / 'reports/items-menu-source'
NIL = 0xffffffff
PARAMETERS = ['SourceViewport','PlatformViewport','GridShape','LabelSize','CursorOffset','CursorMotion','InfoMotion','DisabledColor','NormalColor','ScrollColor','InputBinding','InputRepeat']
LICENSE_REVIEW = 'Pinned LICENSE permits assets/music in game-related forks and modifications. Original art/font/audio are used only by this Mother: Encore port and retain upstream terms; they are not relicensed as MIT.'

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def f32(value): return struct.unpack('<f', struct.pack('<f', value))[0]
def sn(text,path): return node(text.replace('Rect2(', 'Color('),path)
def rect(props):
    x,y=props.get('margin_left',0),props.get('margin_top',0)
    return [x,y,props.get('margin_right',0)-x,props.get('margin_bottom',0)-y]
def patch(props): return [props.get('patch_margin_'+side,0) for side in ['left','top','right','bottom']]
def rgba(hexcode): return [int(hexcode[i:i+2],16)/255 for i in [0,2,4]]+[1]

def reviewed_extract(bindings=None):
    from tools.items_presentation_bindings import load
    b=load(ROOT,bindings);BATTLE=b['source_refs']['battle']
    ex=Extractor(ROOT)
    for path in b['sources']: ex.data(path)
    battle=ex.text(BATTLE); items=ex.text(b['source_refs']['items_script'])
    cap=ex.yaml(b['source_refs']['item_data']); save=ex.yaml(b['source_refs']['save'])
    require(save['party']==[b['item']['party']] and save[b['item']['party']]['inventory']==[{'item_name':b['item']['source_name'],'equipped':True}], 'Unreviewed initial inventory')
    require(cap['actions']==[{'function':'equip'}] and cap['can_use']==[b['item']['party']] and cap['slot']==b['item']['slot'] and not cap['keyitem'] and not cap['HPrecover'] and not cap['PPrecover'] and not cap.get('battle_action') and not cap.get('status_heals'), 'Initial item requires unsupported behavior')
    columns=int(one(r'^const ITEM_PAGE_SIZE_X := (\d+)$',items,'item columns')[1])
    rows=int(one(r'^const ITEM_PAGE_SIZE_Y := (\d+)$',items,'item rows')[1])
    require((columns,rows)==(b['grid']['columns'],b['grid']['rows']),'Unreviewed item grid')
    require('item.is_battle_usable() and _user.character.can_use_item(item)' in items and 'else uiManager.get_flavor_color(3)' in items,'Unreviewed selectability/color')
    inventory=ex.text(b['source_refs']['inventory'])
    capacity=int(one(r'^const _MAX_INVENTORY_SIZE := (\d+)$',inventory,'inventory capacity')[1])
    slots=json.loads(one(r'^const SLOTS := (\[[^\n]+\])$',inventory,'equipment slots')[1])
    dose=int(one(r'self.doses = get_data\(\).get\("doses", (\d+)\)',ex.text(b['source_refs']['item']),'item default doses')[1])
    table={row[0]:row[1] for row in csv.reader(io.StringIO(ex.text(b['source_refs']['item_text']))) if len(row)>1}
    item_value=next((value for value in cap['boost'].values() if value>0),0)
    description=table[cap['description']].replace('\\n','\n').replace('[ItemValue]',str(item_value))
    require('[' not in description and ']' not in description,'Unknown description tag')
    common={row[0]:row[1] for row in csv.reader(io.StringIO(ex.text(b['source_refs']['menu_text']))) if len(row)>1}
    require(common['WORD_SEPARATOR']==' ','Unreviewed English word separator')
    flavor=json.loads(one(r'var menuFlavors := \[\s*(\[[^\n]+?\]),\s*# Plain',ex.text(b['source_refs']['flavor']),'plain flavor',re.S)[1])
    clipnodes=sn(battle,b['nodes']['animation_player'])
    clips=[animation(battle,clipnodes['anims/'+key]['SubResource'],BATTLE,key) for key in b['clips']['names']]
    require(all(c['length']==b['clips']['duration'] and not c['loop'] for c in clips),'Unreviewed menu duration')
    arrow=ex.text(b['source_refs']['arrow'])
    frame_resource=sn(arrow,'.')['frames']['SubResource']
    body=one(r'^\[sub_resource type="SpriteFrames" id='+str(frame_resource)+r'\]\n(.*?)(?=^\[|\Z)',arrow,'arrow frames',re.M|re.S)[1]
    idle=properties(body)['animations'];require(len(idle)==1 and idle[0]['name']==b['clips']['cursor_animation'] and idle[0]['loop'],'Unreviewed cursor frames')
    frames=[]
    for reference in idle[0]['frames']:
        rid=reference['SubResource'];match=one(r'^\[sub_resource type="AtlasTexture" id='+str(rid)+r'\]\n(.*?)(?=^\[|\Z)',arrow,'arrow atlas',re.M|re.S)
        region=properties(match[1].replace('Rect2(', 'Color('))['region']
        require(region[1:]==[0]+b['cursor']['frame_size'] and region[0]%b['cursor']['frame_size'][0]==0,'Unreviewed cursor atlas');frames.append(int(region[0]/b['cursor']['frame_size'][0]))
    info=ex.text(b['source_refs']['info_script'])
    require('var _is_visible := true' in info and 'event.is_action_pressed("'+b['platform']['scope_action']+'")' in info,'Unreviewed info toggle')
    require(info.count('.set_trans(Tween.TRANS_'+b['info']['trans']+').set_ease(Tween.EASE_'+b['info']['ease']+')')==2,'Unreviewed info tween')
    paths={};manager=ex.text(b['source_refs']['audio']);system=ex.text(b['source_refs']['system'])
    for sound in b['sounds']:
        key=sound['key'];text=ex.text(b['source_refs'][sound['source']])
        paths[key]=one(r'"'+re.escape(key)+r'": load\("res://([^"\n]+)"\)',text,key+' audio')[1]
    for path in paths.values():ex.data(path);ex.data(path+'.import')
    definitions=[dict(id=b['item']['definition_id'],source=b['item']['source_name'],name=table[cap['name']],description=description,icon=b['asset_indices']['item'],equipment_slot=slots.index(cap['slot']),heal_hp=cap['HPrecover'],heal_pp=cap['PPrecover'],max_hp_boost=cap['boost']['maxhp'],max_pp_boost=cap['boost']['maxpp'],flags=b['item']['flags'],can_use=b['item']['can_use'])]
    raw=dict(layout={p:sn(battle,p) for p in [b['nodes']['panel'],b['nodes']['grid'],b['nodes']['cursor'],b['nodes']['cursor_back'],b['nodes']['scroll'],b['nodes']['info'],b['nodes']['description'],b['nodes']['hint'],b['nodes']['hint_margin'],b['nodes']['hint_label'],b['nodes']['party_info']]},clips=clips,cursor_idle=dict(frames=frames,fps=idle[0]['speed']),item_value=item_value,cap=cap,audio_paths=paths,description=description,columns=columns,rows=rows,capacity=capacity,doses=dose,flavor=flavor)
    return ex,definitions,raw

def make_recipe(ex):
    from tools.items_presentation_bindings import load
    b=load(ROOT)
    return dict(schema=1,commit=PIN,game_version=ex.lock['game_version'],licence_review=LICENSE_REVIEW,sources=ex.sources,resources=b['assets'])

def validate_source(root,recipe,lock):
    from tools.items_presentation_bindings import load
    b=load(ROOT)
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
        if 'glyph' in r:require(r['glyph']==b['platform']['hint_glyph'] and r['source']==b['source_refs']['hint_font'] and not r['reuse'],'Unreviewed hint glyph')
        if 'quarter_turns' in r:require(r['quarter_turns'] in [1,3] and r['grid']==[3,1] and r['size']==[24,8] and not r['reuse'],'Unreviewed cursor rotation')
        require(r['reuse'] == (not r['output'].startswith('graphics/ui/items/')),'Invalid Items reuse destination')

PROBE = '''extends SceneTree
var config
var native_nodes = {}
func _init(): call_deferred("run")
func run():
 var file = File.new()
 assert(file.open("res://probe-input.json", File.READ) == OK)
 config = JSON.parse(file.get_as_text()).result
 assert(Engine.get_version_info().hash == config.probe.engine)
 file.close()
 var data = DynamicFontData.new()
 data.font_path = config.font_path
 var font = DynamicFont.new()
 font.font_data = data
 font.size = config.probe.main_font_size
 font.extra_spacing_top = config.probe.main_font_spacing_top
 font.extra_spacing_bottom = config.probe.main_font_spacing_bottom
 var hint_data = DynamicFontData.new()
 hint_data.antialiased = config.probe.hint_antialiased
 hint_data.font_path = config.hint_font_path
 var hint_font = DynamicFont.new()
 hint_font.font_data = hint_data
 hint_font.outline_size = config.probe.hint_outline
 hint_font.extra_spacing_char = config.probe.hint_spacing_char
 hint_font.extra_spacing_space = config.probe.hint_spacing_space
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
   n.add_font_override("font",hint_font if row.path == config.node_roles.hint_label else font)
   if row.path == config.node_roles.hint_label: n.text = config.platform.hint_glyph
  if n is RichTextLabel: n.add_font_override("normal_font",font)
  if n is PanelContainer:
   var style = StyleBoxEmpty.new()
   for side in 4: style.set_default_margin(side,config.platform.hint_patch[side])
   n.add_stylebox_override("panel",style)
 for _frame in range(4): yield(self,"idle_frame")
 var desc = native_nodes[config.node_roles.description+"/"+config.node_roles.desc_text]
 desc.bbcode_text = add_line_breaks(config.description,desc)
 for _frame in range(4): yield(self,"idle_frame")
 var result = {"engine":Engine.get_version_info(),"nodes":{},"description":desc.bbcode_text,"font_height":font.get_height(),"font_ascent":font.get_ascent(),"line_widths":[]}
 for path in native_nodes:
  var n = native_nodes[path]
  result.nodes[path] = {"rect":[n.rect_position.x,n.rect_position.y,n.rect_size.x,n.rect_size.y],"minimum":[n.get_combined_minimum_size().x,n.get_combined_minimum_size().y]}
 for line in desc.bbcode_text.split("\\n"): result.line_widths.append(font.get_string_size(line).x)
 var menu_item = native_nodes[config.node_roles.label_prefix+"0"]
 var cursor_size = Vector2(config.cursor.frame_size[0],config.cursor.frame_size[1])
 cursor_size.y = cursor_size.y/config.cursor_y_divisor
 cursor_size.x = -cursor_size.x/config.cursor_x_divisor
 var cursor_pos = menu_item.rect_global_position + Vector2(config.cursor_offset[0],config.cursor_offset[1]) + cursor_size
 result.cursor_position = [cursor_pos.x,cursor_pos.y]
 result.hint_metrics = {"size":hint_font.size,"height":hint_font.get_height(),"ascent":hint_font.get_ascent(),"advance":hint_font.get_string_size(config.platform.hint_glyph).x}
 assert(file.open("res://reference.json",File.WRITE)==OK)
 file.store_string(JSON.print(result,"  "))
 file.close()
 print("ITEMS_NATIVE_LAYOUT_COMPLETE")
 quit()
func _tr(_key) -> String: return " "
'''

def run_native_reference(ex,raw,godot,build):
    from tools.items_presentation_bindings import load
    b=load(ROOT);BATTLE=b['source_refs']['battle']
    build.mkdir(parents=True,exist_ok=True)
    desc=ex.text(b['source_refs']['description']);battle=ex.text(BATTLE);label=ex.text(b['source_refs']['label'])
    nodes=[]
    def add(path,kind,props):
        selected={k:v for k,v in props.items() if k.startswith(('margin_','anchor_','rect_min_size','custom_constants/','size_flags_')) or k in ['columns','alignment','align','valign','fit_content_height','scroll_active','bbcode_enabled','max_lines_visible','grow_horizontal','grow_vertical']}
        if 'rect_min_size' in selected:selected['rect_min_size']=[float(v) for v in selected['rect_min_size']]
        nodes.append(dict(path=path,name=path.split('/')[-1],parent=path.rsplit('/',1)[0] if '/' in path else '',type=kind,props=selected))
    for p,t in [(b['nodes']['panel'],'NinePatchRect'),(b['nodes']['grid'],'GridContainer')]:add(p,t,sn(battle,p))
    for i in range(raw['columns']*raw['rows']):
        props=sn(label,'.');props.update(sn(battle,b['nodes']['label_prefix']+str(i)))
        add(b['nodes']['label_prefix']+str(i),'Label',props)
    for p,t in [(b['nodes']['info'],'NinePatchRect'),(b['nodes']['description'],'Control')]:
        props=sn(battle,p)
        if p==b['nodes']['description']:base=sn(desc,'.');base.update(props);props=base
        add(p,t,props)
    for p,t in [(b['nodes']['desc_hbox'],'HBoxContainer'),(b['nodes']['desc_icon'],'NinePatchRect'),(b['nodes']['desc_margin'],'MarginContainer'),(b['nodes']['desc_text'],'RichTextLabel')]:add(b['nodes']['description']+'/'+p,t,sn(desc,p))
    for p,t in [(b['nodes']['hint'],'PanelContainer'),(b['nodes']['hint_margin'],'MarginContainer'),(b['nodes']['hint_label'],'Label')]:
        props=sn(battle,p)
        if p==b['nodes']['hint_label']:base=sn(ex.text(b['source_refs']['button']),'.');base.update(props);props=base
        add(p,t,props)
    add(b['nodes']['player_info'],'Control',{})
    add(b['nodes']['party_vbox'],'VBoxContainer',sn(battle,b['nodes']['party_vbox']))
    add(b['nodes']['party_info'],'Control',sn(battle,b['nodes']['party_info']))
    text=ex.text(b['source_refs']['text_tools'])
    linebreak=one(r'^static func add_line_breaks\(.*?\n(.*?)(?=^static func)',text,'line wrapping',re.M|re.S)[0].replace('static func','func')
    strip=one(r'^static func strip_bbcode\(.*?\n(.*?)(?=^static func)',text,'strip markup',re.M|re.S)[0].replace('static func','func')
    write_json(build/'probe-input.json',dict(probe=b['probe'],platform=b['platform'],cursor=b['cursor'],cursor_x_divisor=b['cursor_x_divisor'],cursor_y_divisor=b['cursor_y_divisor'],cursor_offset=raw['layout'][b['nodes']['cursor']]['cursor_offset'],nodes=nodes,node_roles=b['nodes'],description=raw['description'],font_path=str(ex.upstream/b['source_refs']['main_font']),hint_font_path=str(ex.upstream/b['source_refs']['hint_font'])))
    (build/b['source_refs']['project']).write_text('config_version=4\n[logging]\nfile_logging/enable_logging=false\n')
    (build/'probe.gd').write_text(PROBE+linebreak+strip)
    result=subprocess.run([str(godot.resolve()),'--path',str(build.resolve()),'-s','probe.gd'],capture_output=True,text=True,timeout=25,env=dict(os.environ,XDG_DATA_HOME=str((build/'userdata').resolve())))
    REPORT.mkdir(parents=True,exist_ok=True);(REPORT/'godot-layout.log').write_text(result.stdout+result.stderr)
    require(result.returncode==0 and 'ITEMS_NATIVE_LAYOUT_COMPLETE' in result.stdout,'Native Items layout reference failed')
    reference=read_json(build/'reference.json');write_json(REPORT/'native-layout.json',reference)
    require(reference['nodes'][b['nodes']['party_vbox']]['minimum'][1]==68,'Source info hide offset changed')
    require(reference['description']==raw['description'],'Unexpected initial description wrapping')
    return reference

def rotate_frames(image,grid,quarter_turns):
    require(type(quarter_turns)is int and quarter_turns in [1,3] and isinstance(grid,list) and len(grid)==2 and type(grid[0])is int and 0<grid[0]<=16 and grid[1]==1 and image.width%grid[0]==0 and image.width//grid[0]==image.height,'Unreviewed frame rotation')
    result=Image.new('RGBA',image.size)
    size=image.height
    for i in range(grid[0]):
        frame=image.crop((i*size,0,(i+1)*size,size)).transpose(Image.Transpose.ROTATE_90 if quarter_turns==1 else Image.Transpose.ROTATE_270)
        result.paste(frame,(i*size,0))
    return result

def compile_assets(ex,recipe,tex3ds,reference,build):
    from tools.items_presentation_bindings import load
    b=load(ROOT);BATTLE=b['source_refs']['battle']
    validate_source(ex.upstream,recipe,ex.lock);OUT.mkdir(parents=True,exist_ok=True)
    dependencies={};resources=[];outputs={}
    for r in recipe['resources']:
        target=safe_path(ROOT/'romfs',r['output']);width,height=r['size']
        if r['reuse']:
            binding=b['reuse'][r['name']];receiptpath=binding['receipt']
            receipt=read_json(ROOT/receiptpath);dependencies[receiptpath]=sha(ROOT/receiptpath)
            output=receipt['outputs'][target.name];require(sha(target)==output['sha256'] and target.stat().st_size==output['bytes'],'Changed reused Items resource')
            source=next(x for x in receipt['resources'] if x['name']==binding['name'])
            require(source['width']==width and source['height']==height,'Reused Items resource geometry mismatch')
        else:
            if 'glyph' in r:
                metrics=reference['hint_metrics'];font=ImageFont.truetype(str(ex.upstream/r['source']),metrics['size'])
                # DynamicFont extra_spacing_char is between characters, not
                # after the final single-character source button label.
                require(abs(font.getlength(r['glyph'])-metrics['advance'])<.001,'Hint font advance differs from native')
                image=Image.new('RGBA',r['size']);draw=ImageDraw.Draw(image)
                draw.fontmode='1';draw.text(tuple(b['platform']['glyph_origin']),r['glyph'],font=font,fill='white',stroke_width=b['probe']['hint_outline'],stroke_fill='black')
            else:
                image=Image.open(ex.upstream/r['source']).convert('RGBA');require(list(image.size)==r['size'],'Changed Items image dimensions')
                if 'crop' in r:x,y,width,height=r['crop'];image=image.crop((x,y,x+width,y+height))
                if 'quarter_turns' in r:image=rotate_frames(image,r['grid'],r['quarter_turns'])
            png=build/(r['name']+'.png');image.save(png)
            subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(png)],check=True)
        resources.append(dict(id=r['id'],path=r['output'],kind=1,width=width,height=height,columns=r['grid'][0],rows=r['grid'][1],sha256=sha(target)))
        if not r['reuse']:outputs[target.name]=dict(bytes=target.stat().st_size,sha256=sha(target))
    receipt=dict(schema=1,recipe=recipe,dependencies=dependencies,resources=resources,outputs=outputs,tex3ds_sha256=sha(tex3ds),native_reference_sha256=sha(REPORT/'native-layout.json'),native_helper_sha256=sha(build/'probe.gd'),native_input_sha256=sha(build/'probe-input.json'),limits='Source-pinned lossless textures and native headless layout/font evidence only; no GPU, emulator, audibility, or hardware claim')
    write_json(receipt_path(OUT, ROOT),receipt)
    return resources,dependencies

def export_ir(ex,definitions,raw,reference,resources,dependencies,write=True):
    from tools.items_presentation_bindings import load
    b=load(ROOT);BATTLE=b['source_refs']['battle']
    layouts=[]
    def add(role,kind,rectangle,parent=NIL,resource=NIL,anchor=(0,0),frame=0,flags=1,color=(1,1,1,1),patches=(0,0,0,0)):
        index=len(layouts);layouts.append(dict(id=index+1,role=role,parent=parent,kind=kind,resource=resource,frame=frame,flags=flags,anchor=list(anchor),rect=list(rectangle),color=list(color),patch=list(patches)));return index
    def bound(name,rectangle,parent=NIL,frame=0,color=None,patches=None):
        row=b['layouts'][name]
        return add(row['role'],row['kind'],rectangle,parent,b['asset_indices'][row['resource']]if row['resource']else NIL,row['anchor'],frame,row['flags'],color if color is not None else b['platform']['normal_color'],patches if patches is not None else [0,0,0,0])
    # Anchors extend the original320x180 canvas while retaining1:1 native pixels.
    panel=bound('panel',rect(raw['layout'][b['nodes']['panel']]),patches=patch(raw['layout'][b['nodes']['panel']]))
    grid=bound('grid',reference['nodes'][b['nodes']['grid']]['rect'],panel)
    label_scene=ex.text(b['source_refs']['label']);marker=sn(label_scene,b['nodes']['equipped']);marker_size=resources[b['asset_indices']['equipped']]['width'],resources[b['asset_indices']['equipped']]['height']
    for i in range(raw['columns']*raw['rows']):
        label=bound('label',reference['nodes'][b['nodes']['label_prefix']+str(i)]['rect'],grid,frame=i)
        bound('equipped',[*marker['position'],*marker_size],label)
    native_cursor=reference['cursor_position'];panel_rect=layouts[panel]['rect']
    cursor=bound('cursor',[native_cursor[0]-panel_rect[0],native_cursor[1]-panel_rect[1],*b['cursor']['frame_size']],panel)
    back=raw['layout'][b['nodes']['cursor_back']];r=rect(back)
    # The backplate is behind the centered cursor, and must be drawn before it.
    cursor_back=bound('cursor_back',r,cursor,color=back['color'])
    scroll=bound('scroll',rect(raw['layout'][b['nodes']['scroll']]),panel)
    scrollbar=ex.text(b['source_refs']['scrollbar']);scrollbg=sn(scrollbar,b['nodes']['scroll_background']);thumb=sn(scrollbar,b['nodes']['scroll_thumb'])
    sr=layouts[scroll]['rect'];br=rect(scrollbg);br[2]+=sr[2];br[3]+=sr[3]
    sb=bound('scroll_background',br,scroll,color=scrollbg['self_modulate'],patches=patch(scrollbg))
    tr=rect(thumb);tr[2]+=br[2];bound('scroll_thumb',tr,sb,patches=patch(thumb))
    scroll_arrows=[]
    for key,y in [('scroll_up',b['cursor']['scroll_inset']),('scroll_down',sr[3]-b['cursor']['scroll_inset'])]:
        source=b['nodes'][key]
        angle=sn(scrollbar,source)['rotation'];require(abs(abs(angle)-math.pi/2)<.00001,'Unreviewed scrollbar arrow rotation')
        scroll_arrows.append(bound(key,[sr[2]/2,y,*b['cursor']['frame_size']],scroll))
    info=bound('info',rect(raw['layout'][b['nodes']['info']]),patches=patch(raw['layout'][b['nodes']['info']]))
    desc=reference['nodes'];prefix=b['nodes']['description']
    d=desc[prefix]['rect'];h=desc[prefix+'/'+b['nodes']['desc_hbox']]['rect'];im=desc[prefix+'/'+b['nodes']['desc_icon']]['rect']
    iconframe=bound('icon_frame',[d[0]+h[0]+im[0],d[1]+h[1]+im[1],im[2],im[3]],info,patches=patch(sn(ex.text(b['source_refs']['description']),b['nodes']['desc_icon'])))
    iw,ih=resources[b['asset_indices']['item']]['width'],resources[b['asset_indices']['item']]['height'];bound('item',[im[2]/2,im[3]/2,iw,ih],iconframe)
    mc=desc[prefix+'/'+b['nodes']['desc_margin']]['rect'];tx=desc[prefix+'/'+b['nodes']['desc_text']]['rect']
    line_height=reference['font_height']+b['platform']['description_line_gap']
    for i,line in enumerate(reference['description'].split('\n')):
        bound('description',[d[0]+h[0]+mc[0]+tx[0],d[1]+h[1]+mc[1]+tx[1]+i*line_height,tx[2],reference['font_height']],info,frame=i)
    # Native platform binding is explicit port data; glyph still uses sourcefont.
    hr=reference['nodes'][b['nodes']['hint']]['rect']
    hintbox=bound('hint_panel',hr,info,patches=b['platform']['hint_patch'])
    hm=reference['nodes'][b['nodes']['hint_margin']]['rect'];hl=reference['nodes'][b['nodes']['hint_label']]['rect']
    bound('hint_glyph',[hm[0]+hl[0]+b['platform']['hint_offset'][0],hm[1]+hl[1]+b['platform']['hint_offset'][1],*b['platform']['hint_size']],hintbox)
    clips=[]
    for source in raw['clips']:
        tracks=[]
        for trk in source['tracks']:
            require(trk['type']=='value' and trk['interp']==1 and trk['path'] in b['clips']['properties'],'Unreviewed Items animation property')
            keys=[]
            for t,e,v in zip(trk['keys']['times'],trk['keys']['transitions'],trk['keys']['values']):
                value=v if isinstance(v,list) else [int(v)]
                keys.append(dict(time=t,ease=e,value=value+[0]*(4-len(value))))
            tracks.append(dict(target=panel,property=b['clips']['properties'][trk['path']],interpolation=trk['keys']['update'],keys=keys))
        clips.append(dict(id=len(clips)+1,role=source['name'],duration=source['length'],loop=source['loop'],tracks=tracks))
    frames=raw['cursor_idle']['frames'];fps=raw['cursor_idle']['fps']
    clips.append(dict(id=len(clips)+1,role=b['clips']['cursor_idle'],duration=len(frames)/fps,loop=True,tracks=[dict(target=target,property='Frame',interpolation=1,keys=[dict(time=i/fps,ease=1,value=[frame,0,0,0]) for i,frame in enumerate(frames)]) for target in [cursor]+scroll_arrows]))
    cursor_src=ex.text(b['source_refs']['cursor_script']);timer=sn(ex.text(b['source_refs']['arrow']),b['nodes']['cursor_timer'])['wait_time']
    move=float(one(r'^const TWEEN_LENGTH := ([0-9.]+)',cursor_src,'cursor move tween')[1])
    viewport=[int(one(r'^window/size/'+k+r'=(\d+)$',ex.text(b['source_refs']['project']),'viewport'+k)[1]) for k in ['width','height']]
    pitchx=reference['nodes'][b['nodes']['label_prefix']+'1']['rect'][0]
    pitchy=reference['nodes'][b['nodes']['label_prefix']+'2']['rect'][1]
    cursor_offset=raw['layout'][b['nodes']['cursor']]['cursor_offset']
    adapter=read_json(ROOT/b['platform']['input_adapter'])
    require(set(adapter)=={'schema','reason','menu_repeat'} and adapter['schema']==1
            and isinstance(adapter['reason'],str) and adapter['reason'], 'Invalid platform input adapter')
    timing=adapter['menu_repeat']
    require(isinstance(timing,list) and len(timing)==4 and all(isinstance(v,(int,float)) for v in timing)
            and 0 < timing[1] <= timing[0] <= 10 and timing[2:]==[0,0], 'Invalid platform menu repeat timing')
    params=dict(SourceViewport=viewport+[0,0],PlatformViewport=b['platform']['viewport'],GridShape=[raw['columns'],raw['rows'],pitchx,pitchy],LabelSize=reference['nodes'][b['nodes']['label_prefix']+'0']['rect'][2:]+[0,0],CursorOffset=[f32(cursor_offset[0]+f32(-b['cursor']['frame_size'][0]/b['cursor_x_divisor'])),cursor_offset[1]+b['cursor']['frame_size'][1]/b['cursor_y_divisor'],*b['cursor']['frame_size']],CursorMotion=[move,timer,fps,0],InfoMotion=[b['info_duration'],reference['nodes'][b['nodes']['party_vbox']]['minimum'][1],0,0],DisabledColor=rgba(raw['flavor'][2]),NormalColor=b['platform']['normal_color'],ScrollColor=scrollbg['self_modulate'],InputBinding=[b['scope_joy'],b['scope_key'],b['platform']['scope_button'],0],InputRepeat=timing)
    require(set(params)==set(PARAMETERS),'Incomplete Items parameters')
    definitions[0]['description']=reference['description']
    sounds=[dict(event=row['event'],path=raw['audio_paths'][row['key']],audio_id=row['audio_id'])for row in b['sounds']]
    result=dict(schema=1,kind='encore.native-items.source-ir',commit=PIN,sources=ex.sources,dependencies=dependencies,scope='Bounded initial single-owner inventory: equipped Baseball Cap, source list/info/cancel/restricted feedback only; no consumable action, random UID creation, saving, or additional inventory source. Pixel geometry retains320x180 with centered-top/bottom anchoring at400x240. Owner/item/instance IDs are deterministic adapter identities. Native L maps original ui_scope left trigger.',capacity=raw['capacity'],owner=b['item']['owner_id'],definitions=definitions,initial_inventory=[dict(id=b['item']['instance_id'],definition=0,equipped=True,doses=raw['doses'])],resources=resources,layouts=layouts,parameters=params,clips=clips,sounds=sounds)
    if write:
        write_json(IR,result)
        write_json(REPORT/'source-review.json',dict(schema=1,commit=PIN,sources=ex.sources,dependencies=dependencies,raw=raw,native_reference_sha256=sha(REPORT/'native-layout.json'),resource_roles={r['name']:i for i,r in enumerate(b['assets'])},layout_bindings=dict(menu=panel,grid=grid,cursor=cursor,cursor_back=cursor_back,scrollbar=scroll,info=info,hint=hintbox),semantics={'disabled_color':'uiManager.get_flavor_color(3) is one-based NEWCOLOR3','close_visibility':'ItemsBox.hide schedules Close then BattleMenuBox.hide immediately hides; retain source clip but do not promise a visible close tween','cursor_position':'Native float vector operations; source centered origin retained with Centered flag; cursor backplate retains show_behind_parent','scroll_arrows':'Source +/-1.5708 rotations mapped to nearest-sampled lossless90-degree per-frame rotation; headless initial single-row scrollbar hidden','description':'Original TextTools.add_line_breaks and strip_bbcode executed with native font in isolated headless helper','scope_binding':'Source joypadbutton6 left trigger → native3DS L512; explicit adaptation','hint':'Source ButtonText custom BottleRocket font and native Panel/MarginContainer/Label layout; platform scope labelL rasterized monochrome with source outline1; noGPU raster equality claim'},limits=['No GUI/screenshot capture','No pixel or audibility comparison','No hardware validation']))
    return result

def verify(root=ROOT/'upstream/MOTHER-Encore'):
    recipe=read_json(RECIPE);validate_source(root,recipe,read_json(ROOT/'upstream.lock'))
    receipt=read_json(receipt_path(OUT, ROOT));require(receipt['schema']==1 and receipt['recipe']==recipe,'Stale Items receipt')
    expected={Path(r['output']).name for r in recipe['resources'] if not r['reuse']}
    require(set(receipt['outputs'])==expected and {p.name for p in OUT.iterdir()}==expected|receipt_entries(OUT, ROOT),'Missing/unexpected Items output')
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
