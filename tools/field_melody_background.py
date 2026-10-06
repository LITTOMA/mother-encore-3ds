#!/usr/bin/env python3
"""One actual Podunk melodyBG, bounded tiled vertical GPU shader and lifecycle."""
from __future__ import annotations
import argparse,hashlib,json,math,re,struct,subprocess,sys,zlib
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import decode,stable,SCENE,PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor,node,animation,properties
from tools.asset_receipts import receipt_path
SCRIPT='Nodes/Ui/effects/melodyBG.gd';AREA='Nodes/Ui/effects/melodyBG.tscn';ART='Graphics/UI/melody bg.png';SHADER='Shaders/Distortionator.shader'
IR=ROOT/'content/native-field-melody-background.json';REVIEW=ROOT/'reports/field-melody-background/source-review.json';PACK=ROOT/'romfs/data/podunk-melody-background.encmelody'
ENGINE={'drivers/gles2/shaders/canvas.glsl':'ccd4c11ab519a59fc54982c0c8efa2eef9d7ec03b911fad696114b4109d0faa7','drivers/gles2/rasterizer_canvas_gles2.cpp':'26b64e991440fb8f9aa415258b02c997ba4fd8cd7c7bdd2a6529f6e9f1f71c26','scene/gui/texture_rect.cpp':'17498ec0735199c1cc7c3e9b509e328554e7aa1247076ef17cee97c4372ce149','scene/animation/animation_player.cpp':'72c1a32819a8d2b7cd1464d7f14057ebea3db4513f615d6fa594e6ce071e4e51','scene/animation/scene_tree_tween.cpp':'c0272f420342bf8606602c31ea935d9d05b6093190a77128889ec362483693a4','scene/animation/scene_tree_tween.h':'5b83aae7581d3ad61e723904ac3dba65054d6da48f66edc6a1cdb6391218a2f5'}
def source_instances(native,receipt,upstream):
 d=read(native);s=read(receipt);require(d['source']=='res://'+SCENE and d['schema']==1 and d['native_compatible'] is False,'Incomplete CutsceneArea native scene')
 require(s['commit']==PIN and s['scene']==SCENE,'Unreviewed CutsceneArea native receipt')
 inventory=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for name,record in s['files'].items():require(sha(upstream/name)==record['sha256']==inventory[name]['sha256'],'Changed NPC closure '+name)
 names={n['path']:n for n in d['nodes']};require(len(names)==8686,'Incomplete Podunk enemy scene');require(decode(names['Objects']['world_transform'])==[[1,0],[0,1],[0,0]],'NPC Objects transform requires a source adapter')
 resources={r['id']:r for r in d['resources']};states={s['source'][6:]:s for s in d['scene_states']};roots=[]
 def visit_instances(path,filename):
  roots.append((path,filename))
  for n in states[filename]['nodes']:
   if n['instance'] is not None:
    local=n['path'][2:] if n['path'].startswith('./') else n['path'];target=local if path=='.' else path+'/'+local
    visit_instances(target,resources[n['instance']['id']]['path'][6:])
 visit_instances('.',SCENE);bindings={};overrides={}
 for root,filename in sorted(roots,key=lambda r:r[0].count('/') if r[0]!='.' else -1,reverse=True):
  for n in states[filename]['nodes']:
   local=n['path'][2:] if n['path'].startswith('./') else n['path'];path=root if local=='.' else local if root=='.' else root+'/'+local
   if path in names:overrides.setdefault(path,{}).update(decode(n['properties']))
  for a in s['script_attachments']:
   if a['source']!=filename:continue
   name=re.search(r'\bname="([^"]+)"',a['node_declaration'])[1];parent=re.search(r'\bparent="([^"]+)"',a['node_declaration'])
   local='.' if parent is None else name if parent[1]=='.' else parent[1]+'/'+name;path=root if local=='.' else local if root=='.' else root+'/'+local
   require(path in names,'Unresolved NPC binding '+path);bindings[path]=a['script']
 children={p:[] for p in names}
 for path in names:
  if path!='.':children[path.rsplit('/',1)[0] if '/' in path else '.'].append(path)
 ready=[]
 def visit(path):
  for child in children[path]:visit(child)
  ready.append(path)
 visit('.');ordinal={p:i for i,p in enumerate(ready)};out=[]
 for path in sorted(bindings,key=lambda p:ordinal[p]):
  if bindings[path]!=SCRIPT:continue
  out.append(dict(stable_id=stable(path),node=path,ready_ordinal=ordinal[path],script=bindings[path],overrides=overrides[path],native=names[path],children=[names[p] for p in children[path]]))
 require(len(out)==1,'Podunk MelodyBackground binding loss');return out,names,resources,dict(native_export_sha256=sha(native),source_receipt_sha256=sha(receipt),source_files={name:r['sha256'] for name,r in s['files'].items()})


def build(records,nodes,resources,provenance,engine):
 ex=Extractor(ROOT);ex.sources.update(provenance['source_files']);script=ex.text(SCRIPT);scene=ex.text(AREA);shader=ex.text(SHADER);project=ex.text('project.godot');ex.text('Scripts/global/uiManager.gd');ex.text('Scripts/global/global.gd');ex.text('LICENSE');size=ex.png_size(ART);imp=ex.text(ART+'.import')
 require(size==[16,10]and 'flags/filter=false'in imp and 'quality/driver/driver_name="GLES2"'in project,'Melody texture/source renderer')
 for p,h in ENGINE.items():require(sha(engine/p.replace('/','-'))==h,'Unreviewed actual engine source '+p)
 for fact in ['global_position = global.currentCamera.get_camera_screen_center()','var actors = uiManager.get_dialogue_actors()','for i in actors:','print(i)','actor.get("position") != null','movedObjects.append([actor, actor.get_parent()])','actor.position -= position','actor.get_parent().remove_child(actor)','add_child(actor)','global.talker != null','movedObjects.append([global.talker, global.talker.get_parent()])','global.talker.position -= position','global.talker.get_parent().remove_child(global.talker)','add_child(global.talker)','i[0].get("active") != null','i[0].active = true','i[0].position += position','remove_child(i[0])','i[1].add_child(i[0])','movedObjects.clear()']:require(fact in script,'Unknown MelodyBG source '+fact)
 a=records[0];v=decode(a['native']['properties']);bg=decode(nodes[a['node']+'/BG']['properties']);ap=decode(nodes[a['node']+'/AnimationPlayer']['properties']);require(v['sort_enabled']and v['scale']==[1,1]and v['rotation']==0 and v['pause_mode']==0 and v['process_priority']==0,'Melody YSort source scope');require(bg['stretch_mode']==2 and bg['expand']and not bg['flip_h']and not bg['flip_v']and bg['rect_rotation']==0 and bg['rect_scale']==[1,1],'Melody source tile capability');require(ap['playback_process_mode']==1 and ap['playback_speed']==1 and ap['autoplay']==''and ap['playback_default_blend_time']==0 and not ap['blend_times'],'Melody AnimationPlayer source capability')
 mat=properties(re.search(r'^\[sub_resource type="ShaderMaterial" id=3\]\n(.*?)(?=^\[)',scene,re.M|re.S)[1]);params={k[13:]:x for k,x in mat.items()if k.startswith('shader_param/')}
 for k in ['ping_pong_speed','osc_amp_ping_pong','compression_amplitude','compression_frequency','compression_speed','comp_amp_ping_pong','comp_trans_ping_pong','interlaced_amplitude','interlaced_frequency','interlaced_speed','inter_amp_ping_pong','inter_trans_ping_pong']:require(params[k]==[0,0],'Unreviewed additional melody shader '+k)
 require(params['oscillation_amplitude'][0]==params['oscillation_frequency'][0]==params['oscillation_speed'][0]==params['osc_trans_ping_pong'][0]==params['move'][0]==0 and params['barrel']is False and params['palette_shifting']is False,'Melody vertical-only shader capability');require('uniform int blending = 0;'in shader,'Unsupported source default screen blending');require('newuv.y += oscillation_amplitude.y * cos((oscillation_frequency.y * newuv.x) + osc_time.y * oscillation_speed.y)'in shader and 'newuv.y += TIME * move.y/0.5;'in shader,'Unknown source UV mechanism');divisor=float(re.search(r'newuv.y \+= TIME \* move.y/([0-9.]+);',shader)[1])
 clip=animation(scene,node(scene,'AnimationPlayer')['anims/Melody']['SubResource'],AREA,'Melody');require(clip['loop']and len(clip['tracks'])==1,'Melody source animation topology');tr=clip['tracks'][0];require(tr['path']=='BG:modulate'and tr['type']=='value'and tr['interp']==1 and tr['loop_wrap']and tr['keys']['update']==0 and all(x==1 for x in tr['keys']['transitions']),'Melody color animation capability')
 colors=[dict(time=t,color=c)for t,c in zip(tr['keys']['times'],tr['keys']['values'])]
 fadein=re.search(r'tween_property\(\$BG, "self_modulate", Color8\(([^)]+)\), ([\d.]+)\)',script);fadeout=list(re.finditer(r'tween_property\(\$BG, "self_modulate", Color8\(([^)]+)\), ([\d.]+)\)',script))[1];fromcolor=re.search(r'\.from\(Color8\(([^)]+)\)\)',script)[1];to_color=lambda s:[int(x.strip())/255 for x in s.split(',')]
 require(script.count('.set_ease(Tween.EASE_OUT)')==2 and 'default_transition = Tween::TRANS_LINEAR'in (engine/'scene-animation-scene_tree_tween.h').read_text(),'Melody source linear fade')
 binding=dict(id=a['stable_id'],node=a['node'],ready_ordinal=a['ready_ordinal'],bg_id=stable(a['node']+'/BG'),bg_ready=a['ready_ordinal']-2,animation_id=stable(a['node']+'/AnimationPlayer'),animation_ready=a['ready_ordinal']-1,position=v['position'],initial_modulate=[bg['modulate'][k]for k in ['r','g','b','a']],initial_self=[bg['self_modulate'][k]for k in ['r','g','b','a']])
 ready_alpha=float(re.search(r'\$BG.self_modulate.a = ([\d.]+)',script)[1]);rect=[bg[k]for k in ['margin_left','margin_top','margin_right','margin_bottom']]
 return dict(schema=1,kind='encore.field-melody-background.source-ir',commit=PIN,scene=SCENE,script=SCRIPT,shader=SHADER,source_viewport=params['screen_size'],native_viewport=[400,240],rect=rect,ready_alpha=ready_alpha,clip=dict(name=clip['name'],length=clip['length'],keys=colors),shader_params=params,vertical=[params['opacity'],params['oscillation_amplitude'][1],params['oscillation_frequency'][1],params['oscillation_speed'][1],params['osc_trans_ping_pong'][1],params['move'][1],divisor],fade_in=dict(duration=float(fadein[2]),from_color=to_color(fromcolor),to_color=to_color(fadein[1])),fade_out=dict(duration=float(fadeout[2]),to_color=to_color(fadeout[1])),bindings=[binding],asset=dict(source=ART,path='graphics/effects/melody-background/tiled.t3x',tile=size,prepared=[size[0],240]),sources=dict(sorted(ex.sources.items())),provenance=provenance,engine=[dict(url='https://raw.githubusercontent.com/godotengine/godot/3.6.2-stable/'+p,sha256=h)for p,h in ENGINE.items()],semantics=['One actual Podunk melodyBG YSort and its TextureRect/AnimationPlayer source Ready: alpha0 and show even serialized hidden root','Actual 16x10 nearest tile texture; original STRETCH_TILE generates UV in texture-pixel units and GLES2 forces repeat despite import repeat=false','Bounded source vertical oscillation cos(2*UV.x +2*cos(2*TIME)) amplitude.5 and scroll -2*TIME; source global renderer TIME, not animation-local clock','Actual canvas final_modulate multiplies shader output after custom fragment; retain animation color and self-modulate fade alpha','GPU adapter extends coverage to400x240 preserving original320x180 centered UV origin, pixels1:1;320x180 is explicit reference mode','Dialogue actor dictionary iteration order is retained including repeated object aliases; after actor loop global talker is queried and moved independently even if same object','Each move caches actual current parent identity then subtracts current root LOCAL position, immediately remove_child/add_child; no actor active=false assignment (source commented code is not executed)','Disappearance only after its SceneTreeTween finished: stop animation/reset clock without restoring color; for every cached entry active existence enables true, position plus current root local position, remove root then add cached parent; clear list after loop','Repeated aliases may produce known Godot parent mismatch remove/add diagnostics; preserve no-effect source operations and continue, never deduplicate or fabricate successful reparent','create_tween is bound to actual node, source global idle insertion order and Tree lifecycle; tween_property fadeout reads current value when its first source step starts, fadein explicit from0; TRANS_LINEAR with EASE_OUT remains linear','Multiple concurrent tweens remain independent tokens and property writers in actual scene-global order; animation idle and tween pass clocks remain separate, no per-component bulk advancement','GPU consumes cached tiled art with at most two 1px vertical strips per column; no CPU frame texture generation, no dynamic whole-surface upload'],unsupported=['Actual YSort tree reparent/actor local-transform/camera/dialogue dictionary/talker and typed active setters must be admitted before mutations','Source unknown animation/tween external listeners or arbitrary shader/material features require reviewed typed capabilities','Cached actor/parent freed during wait follows actual source invalid-object errors; do not silently discard references or restore to invented parents'])

def validate(d):
 require(d['schema']==1 and d['kind']=='encore.field-melody-background.source-ir'and d['commit']==PIN and d['scene']==SCENE and d['script']==SCRIPT and d['shader']==SHADER,'Melody source version');require(len(d['bindings'])==1 and d['source_viewport']==[320,180]and d['native_viewport']==[400,240]and d['ready_alpha']==0,'Melody source scope/adapter')
 a=d['asset'];require(a['source']==ART and a['tile']==[16,10]and a['prepared']==[16,240]and a['path']=='graphics/effects/melody-background/tiled.t3x','Melody GPU asset recipe')
 params=d['shader_params'];require(params['screen_size']==d['source_viewport']and params['opacity']==d['vertical'][0]and params['oscillation_amplitude']==[0,d['vertical'][1]]and params['oscillation_frequency']==[0,d['vertical'][2]]and params['oscillation_speed']==[0,d['vertical'][3]]and params['osc_trans_ping_pong']==[0,d['vertical'][4]]and params['move']==[0,d['vertical'][5]]and params['barrel']is False and params['palette_shifting']is False,'Melody shader/source parameter cross-binding')
 for k in ['ping_pong_speed','osc_amp_ping_pong','compression_amplitude','compression_frequency','compression_speed','comp_amp_ping_pong','comp_trans_ping_pong','interlaced_amplitude','interlaced_frequency','interlaced_speed','inter_amp_ping_pong','inter_trans_ping_pong']:require(params[k]==[0,0],'Unsupported active melody shader feature '+k)
 require(len(d['rect'])==4 and all(type(x)in(int,float)and math.isfinite(x)for x in d['rect'])and d['rect'][2]-d['rect'][0]==d['source_viewport'][0]and d['rect'][3]-d['rect'][1]==d['source_viewport'][1],'Melody source TextureRect bounds')
 require(len(d['vertical'])==7 and all(type(x)in(int,float)and math.isfinite(x)and abs(x)<=100 for x in d['vertical'])and d['vertical'][-1]>0,'Melody shader parameters');require(d['vertical']==[1,.5,2,2,2,-1,.5],'Unknown current vertical shader tuning')
 require(d['engine']==[dict(url='https://raw.githubusercontent.com/godotengine/godot/3.6.2-stable/'+p,sha256=h)for p,h in ENGINE.items()],'Melody engine review')
 c=d['clip'];require(c['length']>0 and 1<=len(c['keys'])<=128 and c['keys'][0]['time']==0,'Melody color clip');last=-1
 for k in c['keys']:require(last<k['time']<c['length']and len(k['color'])==4 and all(type(x)in(int,float)and math.isfinite(x)and 0<=x<=1 for x in k['color']),'Melody color key');last=k['time']
 for f in ['fade_in','fade_out']:
  require(0<d[f]['duration']<=60 and len(d[f]['to_color'])==4 and all(0<=x<=1 for x in d[f]['to_color']),'Melody fade tuning')
 require(len(d['fade_in']['from_color'])==4 and all(0<=x<=1 for x in d['fade_in']['from_color']),'Melody fade source from')
 for b in d['bindings']:
  require(b['id']==stable(b['node'])and b['bg_id']==stable(b['node']+'/BG')and b['animation_id']==stable(b['node']+'/AnimationPlayer')and b['bg_ready']+1==b['animation_ready']and b['animation_ready']+1==b['ready_ordinal'],'Melody source lifecycle identities')
  require(len(b['position'])==2 and all(len(b[k])==4 and all(type(x)in(int,float)and math.isfinite(x)and 0<=x<=1 for x in b[k])for k in ['initial_modulate','initial_self']),'Melody source property values')
 return d

def load():
 d=validate(read(IR));r=read(REVIEW);require(r['commit']==PIN and r['ir_sha256']==sha(IR)and r['sources']==d['sources']and r['engine']==d['engine'],'Melody semantic review stale');inv=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for p,h in d['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h==inv[p]['sha256'],'Changed melody source '+p)
 return d

def assets(tex):
 from PIL import Image
 d=load();a=d['asset'];out=ROOT/'romfs'/a['path'];out.parent.mkdir(parents=True,exist_ok=True);work=ROOT/'build/field-melody-background';work.mkdir(parents=True,exist_ok=True);im=Image.open(ROOT/'upstream/MOTHER-Encore'/a['source']).convert('RGBA');require(list(im.size)==a['tile'],'Melody source pixel dimensions');canvas=Image.new('RGBA',tuple(a['prepared']))
 for y in range(0,a['prepared'][1],im.height):canvas.paste(im,(0,y))
 png=work/'tiled.png';canvas.save(png);subprocess.run([str(tex),'-f','rgba8','-z','none','-o',str(out),str(png)],check=True)
 write(receipt_path(out.parent,ROOT),dict(schema=1,commit=PIN,recipe_sha256=sha(IR),producer_sha256=sha(Path(__file__)),tex3ds_sha256=sha(tex),source_sha256=d['sources'][a['source']],prepared_pixel_sha256=hashlib.sha256(canvas.tobytes()).hexdigest(),outputs={a['path']:dict(bytes=out.stat().st_size,sha256=sha(out))}))

def encode(d):
 validate(d);a=d['asset'];receipt=read(receipt_path(ROOT/'romfs'/Path(a['path']).parent,ROOT));raw=(ROOT/'romfs'/a['path']).read_bytes();require(receipt['commit']==PIN and receipt['recipe_sha256']==sha(IR)and receipt['producer_sha256']==sha(Path(__file__))and receipt['source_sha256']==d['sources'][a['source']]and receipt['outputs'][a['path']]['sha256']==hashlib.sha256(raw).hexdigest(),'Melody genuine asset receipt')
 b=bytearray(64)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def text(v):r=v.encode();u(len(r));b.extend(r)
 text(d['scene']);text(d['script']);text(d['shader']);f(*d['source_viewport'],*d['native_viewport'],*d['rect'],d['ready_alpha'],*d['vertical']);text(d['clip']['name']);f(d['clip']['length']);u(len(d['clip']['keys']))
 for k in d['clip']['keys']:f(k['time'],*k['color'])
 f(d['fade_in']['duration'],*d['fade_in']['from_color'],*d['fade_in']['to_color'],d['fade_out']['duration'],*d['fade_out']['to_color']);text(a['source']);text(a['path']);u(*a['tile'],*a['prepared'],len(raw),zlib.crc32(raw)&0xffffffff);u(len(d['bindings']))
 for v in d['bindings']:u(*[v[k]for k in ['id','ready_ordinal','bg_id','bg_ready','animation_id','animation_ready']]);text(v['node']);f(*v['position'],*v['initial_modulate'],*v['initial_self'])
 u(len(d['sources']))
 for p,h in d['sources'].items():text(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20s12x',b,0,b'ENCMLB01',1,len(b),0,1,1,len(d['bindings']),bytes.fromhex(PIN));struct.pack_into('<I',b,16,zlib.crc32(b)&0xffffffff);return bytes(b)

def stage_files(source):
 d=load();b=encode(d);p=Path('data/podunk-melody-background.encmelody');require((Path(source)/p).read_bytes()==b,'Stale melody pack');art=Path(d['asset']['path']);v=(Path(source)/art).read_bytes();require(v==(ROOT/'romfs'/art).read_bytes(),'Stale melody art');return{p:b,art:v}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','assets']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);p.add_argument('--engine',type=Path);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
 if a.action=='extract':
  require(a.native and a.source and a.engine,'Actual Podunk and reviewed engine exports required');d=validate(build(*source_instances(a.native,a.source,ROOT/'upstream/MOTHER-Encore'),a.engine));write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],engine=d['engine'],semantics=d['semantics'],unsupported=d['unsupported']));print('MelodyBG source:1 root /16x10 tiled shader /6 source color keys');return
 if a.action=='assets':require(a.tex3ds and a.tex3ds.is_file(),'Genuine tex3ds required');assets(a.tex3ds);return
 b=encode(load());PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b);print('MelodyBG checked binary:',len(b),'bytes')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,OSError,TypeError,struct.error,subprocess.CalledProcessError)as e:sys.exit('FIELD MELODY BACKGROUND ERROR: '+str(e))
