#!/usr/bin/env python3
"""Complete source House Canvas art; data admission never enters the scene."""
from __future__ import annotations
import argparse,json,re,struct,sys,subprocess,zlib
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require,decode,stable
from tools import house_geometry as house,house_node_tree as tree,field_canvas_art as canvas
IR=ROOT/'content/native-house-return-canvas.json'
DETAIL=ROOT/'reports/house-return-canvas/native-detail.json'
CAPTURE=ROOT/'reports/house-return-canvas/native-capture.json'
REVIEW=ROOT/'reports/house-return-canvas/source-review.json'
PACK=ROOT/'romfs/data/house-return.enccanvas'
RECEIPT=ROOT/'content/asset-receipts/graphics/house-return-canvas/source.json'
OWNERS=canvas.OWNERS+['Sparkles']
SCRIPTS={**canvas.SCRIPTS,'Scripts/misc/sparkles.gd':19}
TRANSPARENT=['Scripts/Main/RoomTypes/AreaRoom.gd','Maps/podunk/Ninten_s room.gd']

# Opaque texture exports do not contain AtlasTexture margin/filter_clip or the
# AnimatedSprite current frame. Read those actual native fields outside a tree.
EXPORT_SCRIPT=r'''extends SceneTree
var records=[]
var failed=false
func scalar(x): return "%.17f" % x
func vec(p): return [scalar(p.x),scalar(p.y)]
func rect(r): return [scalar(r.position.x),scalar(r.position.y),scalar(r.size.x),scalar(r.size.y)]
func value(x):
    match typeof(x):
        TYPE_NIL,TYPE_BOOL,TYPE_INT,TYPE_STRING: return x
        TYPE_REAL: return {"type":"real","value":scalar(x)}
        TYPE_VECTOR2: return {"type":"Vector2","x":scalar(x.x),"y":scalar(x.y)}
        TYPE_COLOR: return {"type":"Color","r":scalar(x.r),"g":scalar(x.g),"b":scalar(x.b),"a":scalar(x.a)}
        TYPE_OBJECT:
            if x is Texture: return texture(x)
    failed=true
    printerr("Unsupported House Canvas native value "+str(typeof(x)))
    return null
func texture(t):
    if t==null: return null
    var r={"class":t.get_class(),"path":t.resource_path,"size":vec(t.get_size()),"flags":t.get_flags()}
    if t is AtlasTexture:
        r["atlas"]=texture(t.atlas)
        r["region"]=rect(t.region)
        r["margin"]=rect(t.margin)
        r["filter_clip"]=t.filter_clip
    elif not t is StreamTexture:
        failed=true
        printerr("Unknown House Canvas native texture "+t.get_class())
    return r
func material(n):
    var m=n.material
    if n.use_parent_material:
        var p=n.get_parent()
        if p is CanvasItem: return material(p)
    if m==null: return null
    if not m is ShaderMaterial:
        failed=true
        printerr("Unknown House Canvas native material")
        return null
    var params=[]
    for p in m.get_property_list():
        if p.name.begins_with("shader_param/"):
            params.append({"name":p.name.substr(13),"type":p.type,"value":value(m.get(p.name))})
    return {"path":m.resource_path,"shader_path":m.shader.resource_path,"shader_code":m.shader.code,"local_to_scene":m.resource_local_to_scene,"priority":m.render_priority,"params":params}
func visit(n,root):
    if n.get_script()!=null:
        failed=true
        printerr("House source quarantine retained original script")
    if n is Sprite or n is TextureRect or n is AnimatedSprite or n is ColorRect:
        var r={"node":str(root.get_path_to(n)),"class":n.get_class(),"material":material(n)}
        if n is Sprite or n is TextureRect: r["texture"]=texture(n.texture)
        if n is AnimatedSprite:
            r["frame"]=n.frame
            r["animation"]=n.animation
            r["speed_scale"]=scalar(n.speed_scale)
            r["playing"]=n.playing
            r["frames_path"]=n.frames.resource_path
            var animations=[]
            for name in n.frames.get_animation_names():
                var frames=[]
                for i in range(n.frames.get_frame_count(name)): frames.append(texture(n.frames.get_frame(name,i)))
                animations.append({"name":name,"speed":scalar(n.frames.get_animation_speed(name)),"loop":n.frames.get_animation_loop(name),"frames":frames})
            r["animations"]=animations
        if n is ColorRect:
            r["color"]=value(n.color)
            r["size"]=vec(n.rect_size)
        records.append(r)
    for child in n.get_children(): visit(child,root)
func _init():
    var packed=load("res://Maps/podunk/Nintens House.tscn")
    var root=packed.instance()
    visit(root,root)
    if failed:
        root.free()
        quit(2)
        return
    var f=File.new()
    if f.open("res://house_return_canvas_detail.json",File.WRITE)!=OK:
        root.free()
        quit(3)
        return
    f.store_string(JSON.print({"schema":1,"scene":"Maps/podunk/Nintens House.tscn","engine":Engine.get_version_info(),"scene_entered":false,"original_scripts_retained":false,"records":records}))
    f.close()
    root.free()
    quit(0)
'''

def numbers(v):
 if isinstance(v,list):return [numbers(x)for x in v]
 if isinstance(v,dict):
  if v.get('type')=='real':return float(v['value'])
  if v.get('type')=='Vector2':return [float(v['x']),float(v['y'])]
  if v.get('type')=='Color':return [float(v[k])for k in ('r','g','b','a')]
  return {k:numbers(x)for k,x in v.items()}
 return v

def derive():
 t=tree.load();native=read(house.NATIVE);extra=read(DETAIL);inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];sources=dict(t['sources'])
 capture=read(CAPTURE);require(capture['schema']==1 and capture['commit']==PIN and capture['scene']==house.SCENE and capture['original_native_sha256']==sha(house.NATIVE)and capture['original_source_receipt_sha256']==sha(house.SOURCE)and capture['exporter_sha256']==__import__('hashlib').sha256(EXPORT_SCRIPT.encode()).hexdigest()and capture['native_detail_sha256']==sha(DETAIL)and capture['scene_entered']is False and capture['original_scripts_retained']is False and capture['script_errors']==0,'House Canvas actual source capture provenance differs')
 originals=read(house.SOURCE)['files'];payloads=capture['quarantined_source_payloads'];require(set(payloads)==set(originals),'House Canvas quarantined source closure differs')
 for p,s in originals.items():
  proof=payloads[p];require(proof['original_sha256']==s['sha256'],'House Canvas native payload original source differs')
  if p.endswith('.gd'):require(proof['original_script_absent']is True,'House Canvas native retained original script')
  else:require(proof['original_quarantine_bytes_checked']is True and proof['native_payload_sha256']==s.get('reference_sha256',s['sha256']),'House Canvas exact quarantined native payload differs')
 require(extra['schema']==1 and extra['scene']==house.SCENE and extra['scene_entered']is False and extra['original_scripts_retained']is False and [extra['engine'][k]for k in ('major','minor','patch','status')]==[3,6,2,'stable'],'House Canvas supplemental native source differs')
 ns={n['path']:n for n in native['nodes']};rs={r['id']:r for r in native['resources']};rows={r['id']:r for r in t['records']};detail={r['node']:r for r in extra['records']};textures={};records=[];boundaries=[]
 selected=[r for r in t['records']if ns[r['node']]['class']in ('Sprite','TextureRect','AnimatedSprite','ColorRect')]
 require(len(detail)==len(extra['records'])==len(selected)==67 and set(detail)=={r['node']for r in selected},'House Canvas full 38 Sprite/24 TextureRect/4 AnimatedSprite/1 ColorRect scope differs')
 def source(p):
  require(p in inv and sha(ROOT/'upstream/MOTHER-Encore'/p)==inv[p]['sha256'],'House Canvas changed original source '+p);sources[p]=inv[p]['sha256']
 def texture(v):
  if v is None:return dict(texture=0,region=[0,0,0,0],margin=[0,0,0,0],filter_clip=False)
  if v['class']=='AtlasTexture':
   require(v['path'].startswith('res://')and '::'in v['path'],'House Canvas atlas source missing');source(v['path'][6:].split('::')[0]);base=texture(v['atlas']);region=list(map(float,v['region']));margin=list(map(float,v['margin']));size=list(map(float,v['size']))
   require(size==[region[2]+margin[2],region[3]+margin[3]] and min(region[:2])>=0 and min(region[2:])>0 and type(v['filter_clip'])is bool,'House Canvas actual AtlasTexture extent differs')
   require(region[0]+region[2]<=textures[base['texture']]['size'][0] and region[1]+region[3]<=textures[base['texture']]['size'][1],'House Canvas AtlasTexture out of original PNG')
   return dict(texture=base['texture'],region=region,margin=margin,filter_clip=v['filter_clip'],atlas_source=v['path'][6:])
  require(v['class']=='StreamTexture'and v['path'].startswith('res://'),'House Canvas unsupported original texture '+str(v))
  src=v['path'][6:];source(src);source(src+'.import');imp=(ROOT/'upstream/MOTHER-Encore'/(src+'.import')).read_text()
  require(all(s in imp for s in ['flags/filter=false','flags/mipmaps=false','flags/repeat=0','process/premult_alpha=false','process/invert_color=false']),'House Canvas source sampling/process differs')
  matches=[r for r in rs.values()if r['class']=='StreamTexture'and r['path']=='res://'+src];require(len(matches)<=1,'House Canvas original texture resource is not unique '+src)
  size=list(struct.unpack('>II',(ROOT/'upstream/MOTHER-Encore'/src).read_bytes()[16:24]));require(size==list(map(float,v['size'])) and all(0<s<=65535 for s in size),'House Canvas native/original PNG extent differs')
  # The old opaque AtlasTexture codec did not recurse into its atlas. Preserve
  # a checked source-asset stable ID for that actual getter-only PNG instead of
  # inventing an ID in the earlier native resource inventory.
  if matches:require(size==decode(matches[0]['size']),'House Canvas original native texture extent differs')
  tid=stable('house-canvas-texture:'+src)
  item=dict(id=tid,source=src,size=size,source_sha256=sources[src],native_resource_id=matches[0]['id']if matches else None,nearest=True,repeat=False,native_flags=v.get('flags'),pages=[dict(crop=[x,y,min(1024,size[0]-x),min(1024,size[1]-y)],path='graphics/sprites/house-return-canvas/'+str(tid)+'-'+str(x)+'-'+str(y)+'.t3x')for y in range(0,size[1],1024)for x in range(0,size[0],1024)])
  require(tid not in textures or textures[tid]==item,'House Canvas original texture ID collision');textures[tid]=item
  return dict(texture=tid,region=[0,0,*size],margin=[0,0,0,0],filter_clip=False)
 def owner(row):
  q=row
  while q:
   script=q['script']
   if script in SCRIPTS:return dict(owner=SCRIPTS[script],owner_id=q['id'],owner_script=script,owner_sha=q['script_sha'])
   require(not script or script in TRANSPARENT,'House Canvas unreviewed source appearance ancestor '+script+' at '+row['node']);q=rows.get(q['parent'])
  return dict(owner=0,owner_id=0,owner_script='',owner_sha='0'*64)
 def material(v,o):
  if v is None:return dict(shader=0,shader_source='',material=None)
  require(v['priority']==0 and type(v['local_to_scene'])is bool,'House Canvas unreviewed native material priority/local flag')
  src=v['shader_path'];require(src.startswith('res://'),'House Canvas native shader source absent');src=src[6:];base=src.split('::')[0];source(base)
  if '::'in src:
   body=re.search(r'\[sub_resource type="Shader" id='+re.escape(src.split('::')[1])+r'\]\n(.*?)(?=\n\[|\Z)',(ROOT/'upstream/MOTHER-Encore'/base).read_text(),re.S);require(body,'House Canvas original embedded shader absent');code=json.JSONDecoder(strict=False).raw_decode(body[1].split('code = ',1)[1])[0];source('Shaders/Flash.tres');flash=json.JSONDecoder(strict=False).raw_decode((ROOT/'upstream/MOTHER-Encore/Shaders/Flash.tres').read_text().split('code = ',1)[1])[0];require(code==flash and v['shader_code']in ('',code) and o['owner']!=0,'House Canvas unknown source shader');shader=3
  else:
   code=(ROOT/'upstream/MOTHER-Encore'/src).read_text();require(src in ['Shaders/Outline.shader','Shaders/Distortionator.shader'] and v['shader_code']in ('',code),'House Canvas unsupported source shader '+src);shader=1 if src=='Shaders/Outline.shader'else 2;require((shader==1 and o['owner']==11)or(shader==2 and o['owner']==15),'House Canvas shader actual owner differs')
  # The official headless dummy VisualServer returns an empty shader parameter
  # list. Close original values through the unique checked serialized material
  # declaration, preserving uninitialized shader declarations explicitly.
  candidates=[r for r in rs.values()if r['class']=='ShaderMaterial'and r['path'].startswith('res://')and rs[decode(r['properties'])['shader']['id']]['path']=='res://'+src]
  origin=v['path'][6:]if v['path'].startswith('res://')else ''
  if not origin:require(len(candidates)==1,'House Canvas local material origin ambiguous');origin=candidates[0]['path'][6:]
  material_file,separator,sub=origin.partition('::');source(material_file);text=(ROOT/'upstream/MOTHER-Encore'/material_file).read_text()
  pattern=r'\[sub_resource type="ShaderMaterial" id='+re.escape(sub)+r'\]\n(.*?)(?=\n\[|\Z)'if separator else r'\[resource\]\n(.*?)(?=\n\[|\Z)'
  block=re.search(pattern,text,re.S);require(block,'House Canvas original material declaration absent '+origin)
  def literal(value):
   value=value.strip()
   if value in ('true','false'):return value=='true'
   if re.fullmatch(r'[-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?',value):return float(value)if any(c in value for c in '.eE')else int(value)
   match=re.fullmatch(r'(Color|Vector2|vec2|vec4)\(\s*(.*?)\s*\)',value)
   require(match,'House Canvas unreviewed source uniform literal '+value);items=[literal(p)for p in match[2].split(',')];size=4 if match[1]in('Color','vec4')else 2
   if match[1].startswith('vec')and len(items)==1:items*=size
   require(len(items)==size and all(isinstance(x,(int,float))for x in items),'House Canvas source uniform vector differs');return items
  overrides={name:literal(value)for name,value in re.findall(r'^shader_param/(\w+)\s*=\s*(.+)$',block[1],re.M)};params=[]
  declarations=re.findall(r'^uniform\s+(bool|int|float|vec2|vec4|sampler2D)\s+(\w+)(?:\s*:\s*[^=;]+)?\s*(?:=\s*([^;]+))?;',code,re.M)
  require(declarations and len({d[1]for d in declarations})==len(declarations),'House Canvas missing/duplicate original shader declaration')
  require(set(overrides)<={d[1]for d in declarations},'House Canvas unknown serialized material uniform')
  for kind,name,default in declarations:
   params.append(dict(name=name,type=kind,value=overrides[name]if name in overrides else literal(default)if default else None,provenance='serialized_material'if name in overrides else 'shader_default'if default else 'source_uninitialized',material_source=origin))
  pm={p['name']:p for p in params}
  for actual in v['params']:
   require(actual['name']in pm and numbers(actual['value'])==pm[actual['name']]['value'],'House Canvas native/serialized uniform differs')
  return dict(shader=shader,shader_source=src,material=dict(source=origin,native_path=v['path'],local_to_scene=v['local_to_scene'],priority=v['priority'],params=params,shader_code_sha256=__import__('hashlib').sha256(code.encode()).hexdigest(),native_shader_code_available=bool(v['shader_code']),uniform_proof='checked original material declaration plus complete shader declaration; dummy native shader code/parameter getter unavailable'))
 for row in t['records']:
  n=ns[row['node']];p=decode(n['properties']);klass=n['class']
  if row['flags']&1:require(not p.get('rect_clip_content',False) and p['light_mask']==1 and (not(row['flags']&128)or row['z']==0),'House Canvas unsupported source clipping/light/YSort')
  if klass in ('Label','HBoxContainer','Control'):
   q=row
   while q and not q['script']:q=rows.get(q['parent'])
   require(q is not None,'House Control source owner absent')
   boundary=dict(id=row['id'],node=row['node'],class_name=klass,flags=row['flags'],owner_id=q['id'],owner_script=q['script'],owner_sha=q['script_sha'],native_properties=p,consumer='HouseNativeControl',source_admitted=False)
   if klass=='Label':
    font=rs[p['custom_fonts/font']['id']];require(font['class']=='DynamicFont'and font['path'].startswith('res://'),'House Label actual native font missing');font_source=font['path'][6:];source(font_source)
    boundary.update(text=p['text'],font_source=font_source,font_source_sha256=sources[font_source])
   boundaries.append(boundary);continue
  if klass not in ('Sprite','TextureRect','AnimatedSprite','ColorRect'):continue
  q=detail[row['node']];require(q['class']==klass,'House Canvas supplemental class differs');o=owner(row);m=material(q['material'],o);r=dict(id=row['id'],node=row['node'],kind=('Sprite','TextureRect','AnimatedSprite','ColorRect').index(klass),flags=row['flags'],**o,**m)
  if klass in ('Sprite','TextureRect'):
   tex=texture(q['texture']);require((q['texture']is None)==(p['texture']is None),'House Canvas original texture null differs')
   if p['texture']:require(q['texture']['path']==rs[p['texture']['id']]['path'],'House Canvas original texture path differs')
   r.update(**tex,hframes=p['hframes']if klass=='Sprite'else 1,vframes=p['vframes']if klass=='Sprite'else 1,frame=p['frame']if klass=='Sprite'else 0,offset=p['offset']if klass=='Sprite'else [0,0],centered=p['centered']if klass=='Sprite'else False,flip_h=p['flip_h'],flip_v=p['flip_v'],stretch=p['stretch_mode']if klass=='TextureRect'else 0,size=[p['margin_right']-p['margin_left'],p['margin_bottom']-p['margin_top']]if klass=='TextureRect'else [0,0])
   if klass=='Sprite':require(not p['region_enabled']and not p['region_filter_clip']and r['hframes']>0 and r['vframes']>0 and 0<=r['frame']<r['hframes']*r['vframes'],'House Canvas unsupported native Sprite region/frame')
   else:require(r['stretch']in (2,3)and(r['stretch']!=2 or o['owner']==15),'House Canvas unsupported native TextureRect stretch')
  elif klass=='AnimatedSprite':
   require(o['owner']==19 and o['owner_id']==row['id'] and q['frames_path']==rs[p['frames']['id']]['path']and q['animation']==p['animation']and float(q['speed_scale'])==p['speed_scale']and q['playing']==p['playing'],'House Canvas Sparkles source native owner/frame binding differs')
   source(o['owner_script']);text=(ROOT/'upstream/MOTHER-Encore'/o['owner_script']).read_text();require(re.fullmatch(r'\s*tool\s+extends AnimatedSprite\s+func _ready\(\):\s*frame = int\(rand_range\(0, 47\)\)\s*',text),'House Sparkles original Ready changed')
   animations=[dict(name=a['name'],speed=float(a['speed']),loop=a['loop'],frames=[texture(f)for f in a['frames']])for a in q['animations']]
   require(len(animations)==1 and animations[0]['name']=='Sparkle On'and len(animations[0]['frames'])==40 and animations[0]['speed']==12 and animations[0]['loop']is True and q['frame']==22,'House Sparkles full actual 40-frame pre-Ready animation differs')
   r.update(texture=0,hframes=1,vframes=1,frame=q['frame'],offset=p['offset'],centered=p['centered'],flip_h=p['flip_h'],flip_v=p['flip_v'],stretch=0,size=[0,0],animation=q['animation'],speed_scale=float(q['speed_scale']),playing=q['playing'],animations=animations,ready_rng=dict(method='_ready',distribution='rand_range',cast='int',minimum=0,maximum=47,script_sha256=o['owner_sha']))
  else:
   color=numbers(q['color']);require(color==[p['color'][k]for k in ('r','g','b','a')]and list(map(float,q['size']))==[p['margin_right']-p['margin_left'],p['margin_bottom']-p['margin_top']],'House ColorRect native/source color/extent differs')
   r.update(texture=0,hframes=1,vframes=1,frame=0,offset=[0,0],centered=False,flip_h=False,flip_v=False,stretch=0,size=list(map(float,q['size'])),color=color,consumer='HouseNativeControl',source_admitted=False)
  records.append(r)
 # Original exported Openable texture setters and parent NPC setup run after
 # the quarantine snapshot. Close both real source asset graphs before Ready.
 sprite_ir=ROOT/'content/native-house-return-sprite.json';sprite=read(sprite_ir);sprite_review=read(ROOT/'reports/house-return-sprite/source-review.json')
 require(sprite['commit']==PIN and sprite['scene_id']==t['scene_id']and sprite['house_tree_ir_sha256']==sha(tree.IR)and sprite_review['ir_sha256']==sha(sprite_ir)and sprite_review['producer_sha256']==sha(ROOT/'tools/house_return_sprite.py')and sprite_review['shared_producer_sha256']==sha(ROOT/'tools/field_sprite_bridge.py'),'House Canvas checked CharacterSprite source closure differs')
 for p,h in sprite['sources'].items():source(p);require(sources[p]==h,'House Canvas CharacterSprite source differs')
 preload=[dict(source=r['path'][6:],size=decode(r['size']),proof='actual native resource inventory')for r in rs.values()if r['class']=='StreamTexture']+ [dict(source=r['path'],size=r['size'],proof='checked original House CharacterSprite/parent NPC setup')for r in sprite['textures']]
 for image in preload:
  if any(t['source']==image['source']for t in textures.values()):continue
  texture(dict(**{'class':'StreamTexture'},path='res://'+image['source'],size=image['size']))
 require(len(records)==67 and len(boundaries)==47 and sum(r['kind']==2 for r in records)==4,'House Canvas complete rendered/Control boundary scope differs')
 for p in ['LICENSE','project.godot',*TRANSPARENT]:source(p)
 require('2d/snapping/use_gpu_pixel_snap=true'in(ROOT/'upstream/MOTHER-Encore/project.godot').read_text(),'House source GPU pixel snap differs')
 # This room script has only its source cutscene-finished callback; it never
 # mutates appearance. Keep a specific reviewed transparent ancestor proof.
 room=(ROOT/'upstream/MOTHER-Encore'/TRANSPARENT[1]).read_text();require('extends AreaRoom'in room and 'func _on_AnimationPlayer_animation_finished(anim_name):'in room and 'uiManager.start_battle()'in room,'House actual Room source changed')
 return dict(schema=2,kind='encore.field-canvas-art.source-ir',format=2,capabilities=2,rules=1,commit=PIN,scene=house.SCENE,scene_id=t['scene_id'],scene_sha256=t['source_sha256'],native_sha256=sha(house.NATIVE),detail_sha256=sha(DETAIL),capture_sha256=sha(CAPTURE),exporter_sha256=__import__('hashlib').sha256(EXPORT_SCRIPT.encode()).hexdigest(),tree_ir_sha256=sha(tree.IR),sprite_ir_sha256=sha(sprite_ir),source_receipt_sha256=sha(house.SOURCE),producer_sha256=sha(Path(__file__)),sources=dict(sorted(sources.items())),engine_tag='3.6.2-stable',engine_sources=canvas.ENGINE_V2,owners=OWNERS,transparent_appearance_ancestors={p:sources[p]for p in TRANSPARENT},records=records,textures=sorted(textures.values(),key=lambda r:r['id']),control_boundaries=boundaries,program=dict(source=canvas.PROGRAM,path='shaders/field-canvas-art.shbin',source_sha256=sha(ROOT/canvas.PROGRAM)),pixel_snap=True,y_epsilon=0.00001,alpha_prune=0.007,scene_admitted=False)

def review(d):
 return dict(schema=2,commit=PIN,ir_sha256=sha(IR),scene_id=d['scene_id'],scene_sha256=d['scene_sha256'],native_sha256=d['native_sha256'],detail_sha256=d['detail_sha256'],capture_sha256=d['capture_sha256'],exporter_sha256=d['exporter_sha256'],tree_ir_sha256=d['tree_ir_sha256'],producer_sha256=d['producer_sha256'],engine_sources=d['engine_sources'],sources=d['sources'],counts=dict(records=len(d['records']),textures=len(d['textures']),pages=sum(len(t['pages'])for t in d['textures']),control_boundaries=len(d['control_boundaries'])),scene_admitted=False,semantics=['All actual 67 House Sprite/TextureRect/AnimatedSprite/ColorRect records, with full original native IDs and source owner ancestry','Original material uniform declarations/defaults retained in full and source Flash/Distortion code checked; official dummy shader native code/property getters unavailable, concrete mapped material owners remain required','Actual full Sparkles atlas region/margin/filter-clip and 40-frame SpriteFrames sequence; original Ready RNG command is source data and is not executed by loading','Godot AnimatedSprite native set_frame clamp/reset_timeout/frame_changed; source Ready truncates actual rand_range before that setter; actual sole idle_internal callback consumes process delta/update_pending, animation_finished precedes frame_changed at wrap; no separate clock','Actual 624x1104 background remains whole through exact source PNG crop pages and real tex3ds outputs','All 23 Label/23 HBoxContainer/1 Control boundaries retain complete native source property digests; Labels additionally retain exact original text and DynamicFont file fingerprint and require actual Control delegate; no silent discard or Ready grant'])

def extract():
 d=derive();write(IR,d);write(REVIEW,review(d));return d

def load():
 d=read(IR);require(d==derive() and read(REVIEW)==review(d),'House Canvas source/review stale');return d

def assets(d,tex3ds,work):
 from PIL import Image
 from io import BytesIO
 require(Path(tex3ds).is_file(),'House Canvas actual tex3ds required');work.mkdir(parents=True,exist_ok=True);pages=[]
 for t in d['textures']:
  image=Image.open(ROOT/'upstream/MOTHER-Encore'/t['source']).convert('RGBA');require(list(image.size)==t['size'],'House Canvas source PNG extent differs')
  for page in t['pages']:
   x,y,w,h=page['crop'];blob=BytesIO();image.crop((x,y,x+w,y+h)).save(blob,format='PNG');raw=blob.getvalue();png=work/(Path(page['path']).stem+'.png');png.write_bytes(raw)
   pages.append(dict(texture_id=t['id'],source=t['source'],source_sha256=t['source_sha256'],crop=page['crop'],path=page['path'],crop_png_sha256=sha(png),png=str(png)))
 def convert(a):
  p=ROOT/'romfs'/a['path'];p.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(p),a['png']],check=True);a.update(bytes=p.stat().st_size,crc=zlib.crc32(p.read_bytes()),output_sha256=sha(p));del a['png']
 with ThreadPoolExecutor(max_workers=4)as pool:list(pool.map(convert,pages))
 program=ROOT/'romfs'/d['program']['path'];require(program.is_file(),'House Canvas requires actual existing reviewed PICA output')
 old=read(canvas.RECEIPT);require(old['program']['source_sha256']==d['program']['source_sha256']and old['program']['output_sha256']==sha(program)and old['program']['bytes']==program.stat().st_size and old['program']['crc']==zlib.crc32(program.read_bytes()),'House Canvas actual PICA program differs')
 write(RECEIPT,dict(schema=2,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),shared_producer_sha256=sha(ROOT/'tools/field_canvas_art.py'),tex3ds_sha256=sha(tex3ds),workers=4,license_sha256=d['sources']['LICENSE'],license_review='Pinned upstream graphics conditions: game-related fork, modification or translation only; this Mother: Encore port retains original source attribution and conditions.',pages=pages,program=old['program']))

def checked_assets(d,root):
 from PIL import Image
 from io import BytesIO
 a=read(RECEIPT);require(a['schema']==2 and a['commit']==PIN and a['ir_sha256']==sha(IR)and a['producer_sha256']==sha(Path(__file__))and a['shared_producer_sha256']==sha(ROOT/'tools/field_canvas_art.py')and a['workers']==4 and a['license_sha256']==d['sources']['LICENSE'],'House Canvas actual conversion receipt differs')
 expected=[(t,p)for t in d['textures']for p in t['pages']];require(len(a['pages'])==len(expected),'House Canvas complete page receipt missing')
 for (t,p),proof in zip(expected,a['pages']):
  require(proof['texture_id']==t['id']and proof['source']==t['source']and proof['source_sha256']==t['source_sha256']and proof['crop']==p['crop']and proof['path']==p['path'],'House Canvas physical page source proof differs')
  image=Image.open(ROOT/'upstream/MOTHER-Encore'/t['source']).convert('RGBA');x,y,w,h=p['crop'];blob=BytesIO();image.crop((x,y,x+w,y+h)).save(blob,format='PNG')
  require(__import__('hashlib').sha256(blob.getvalue()).hexdigest()==proof['crop_png_sha256'],'House Canvas exact original PNG crop changed')
  output=Path(root)/p['path'];require(output.stat().st_size==proof['bytes']and sha(output)==proof['output_sha256']and zlib.crc32(output.read_bytes())==proof['crc'],'House Canvas converted physical page changed')
 output=Path(root)/d['program']['path'];proof=a['program'];require(proof['source_sha256']==d['program']['source_sha256']and output.stat().st_size==proof['bytes']and sha(output)==proof['output_sha256']and zlib.crc32(output.read_bytes())==proof['crc'],'House Canvas actual PICA program changed')
 return a

def compile():
 d=load();a=checked_assets(d,ROOT/'romfs');raw=canvas.encode(d,a,sha(IR));PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw);return raw

def stage_files(source):
 d=load();a=checked_assets(d,source);raw=canvas.encode(d,a,sha(IR));require((Path(source)/'data/house-return.enccanvas').read_bytes()==raw,'House Canvas staged pack differs')
 return {Path('data/house-return.enccanvas'):raw,**{Path(p['path']):(Path(source)/p['path']).read_bytes()for p in a['pages']},Path(d['program']['path']):(Path(source)/d['program']['path']).read_bytes()}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['export-script','extract','assets','compile']);p.add_argument('--out',type=Path);p.add_argument('--tex3ds',type=Path);p.add_argument('--work',type=Path,default=ROOT/'build/house-return-canvas-textures');a=p.parse_args()
 if a.action=='export-script':require(a.out is not None,'House Canvas requires explicit private exporter path');a.out.write_text(EXPORT_SCRIPT,encoding='utf8');return
 d=extract()if a.action=='extract'else load()
 if a.action=='assets':require(a.tex3ds is not None,'House Canvas actual tex3ds path required');assets(d,a.tex3ds,a.work);return
 if a.action=='compile':print('House Canvas actual v2 pack:',len(compile()),'bytes; no scene Ready');return
 print('House Canvas full source IR:',len(d['records']),'records;',len(d['control_boundaries']),'explicit Control boundaries; no scene Ready')

if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('HOUSE CANVAS ERROR: '+str(e))
