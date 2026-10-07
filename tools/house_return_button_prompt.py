#!/usr/bin/env python3
"""Original House ButtonPrompt sources and actual native leaves, one typed pack."""
from pathlib import Path
import argparse,hashlib,json,re,struct,sys,zlib
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require,decode,stable
from tools.extract_battle_entry import Extractor,node
from tools import field_prompts as prompts,prompt_native
from tools.house_node_tree import SCENE,node_id
TREE=ROOT/'content/native-house-node-tree.json'
NATIVE=ROOT/'reports/cloud-world/house-exact.json'
SOURCE=ROOT/'reports/cloud-world/house-source.json'
IR=ROOT/'content/native-house-return-button-prompt.json'
REVIEW=ROOT/'reports/house-return-button-prompt/source-review.json'
PACK=ROOT/'romfs/data/house-return.encbuttonprompt'
CONFIG=prompts.PromptScene(SCENE,node_id('.'),node_id,23,TREE,'records')
FAMILY=0x454e0080

def canonical(d):return hashlib.sha256(json.dumps(d,sort_keys=True,separators=(',',':'),ensure_ascii=False).encode()).hexdigest()

def source_instances(tree):
 d,s=read(NATIVE),read(SOURCE)
 require(d['source']=='res://'+SCENE and s['scene']==SCENE and s['commit']==PIN and len(d['nodes'])==497 and sha(NATIVE)==tree['native_sha256'] and sha(SOURCE)==tree['source_receipt_sha256'],'House Prompt actual native/source export differs')
 names={n['path']:n for n in d['nodes']};resources={r['id']:r for r in d['resources']};states={v['source'][6:]:v for v in d['scene_states']};roots=[]
 def visit(root,filename):
  require(filename in states,'House Prompt missing instantiated source state '+filename);roots.append((root,filename))
  for n in states[filename]['nodes']:
   if n['instance'] is not None:
    p=n['path'][2:]if n['path'].startswith('./')else n['path'];target=p if root=='.'else root+'/'+p
    visit(target,resources[n['instance']['id']]['path'][6:])
 visit('.',SCENE);overrides={}
 for root,filename in sorted(roots,key=lambda r:r[0].count('/')if r[0]!='.'else-1,reverse=True):
  for n in states[filename]['nodes']:
   local=n['path'][2:]if n['path'].startswith('./')else n['path'];path=root if local=='.'else local if root=='.'else root+'/'+local
   if path in names:overrides.setdefault(path,{}).update(decode(n['properties']))
 records=[]
 for row in sorted(tree['records'],key=lambda r:r['ready']):
  if row['script']!=prompts.SCRIPT:continue
  require(tree['classes'][row['class_index']]=='Node2D' and row['id']==node_id(row['node']) and row['script_sha']==tree['sources'][prompts.SCRIPT],'House Prompt actual source binding differs')
  records.append(dict(stable_id=row['id'],node=row['node'],ready_ordinal=row['ready'],overrides=overrides[row['node']],native=names[row['node']]))
 require(len(records)==23,'House Prompt full original coverage differs')
 proof=dict(native_export_sha256=sha(NATIVE),source_receipt_sha256=sha(SOURCE),source_files={p:v['sha256']for p,v in s['files'].items()})
 return records,names,proof

def native(core,tree):
 ex=Extractor(ROOT);text=ex.text(prompts.SOURCE);script=ex.text(prompts.SCRIPT)
 global_script=ex.text('Scripts/global/globalData.gd');ex.text('Scripts/global/text_tools.gd');player=ex.text('Scripts/Main/party/Player.gd')
 require('var button_prompts' in global_script and re.search(r'func is_paused\([^)]*\)[^\n]*:\s*\n\s*return _paused',player),'Prompt same global/player observation source differs')
 ex.data('Fonts/BottleRocket.tres');ex.data('Graphics/UI/select_arrow.png')
 box,label,arrow,ap=[node(text,p)for p in('HBoxContainer','HBoxContainer/Label','Arrow','AnimationPlayer')]
 require(box['alignment']==label['align']==1 and arrow['stretch_mode']==3 and arrow['rect_rotation']==90 and arrow['use_parent_material']is True and label['text']=='A','Original Prompt native layout differs')
 require(set(ap)=={'anims/Float','anims/Hide','anims/Press','anims/RESET','anims/Show'},'Original Prompt AP native fields differ')
 bypath={v['node']:v for v in tree['records']};records=[]
 for prompt in core['records']:
  for role,suffix,klass in [(1,'HBoxContainer','HBoxContainer'),(2,'HBoxContainer/Label','Label'),(3,'Arrow','TextureRect'),(4,'AnimationPlayer','AnimationPlayer')]:
   path=prompt['node']+'/'+suffix;n=bypath[path]
   require(tree['classes'][n['class_index']]==klass and not n['script'] and n['id']==node_id(path),'House Prompt native leaf source differs '+path)
   records.append(dict(id=n['id'],prompt=prompt['id'],parent=n['parent'],role=role,path=path,native_class=klass))
 inputs=[dict(v)for v in read(ROOT/'content/story-input-bindings.json')['bindings']];equipment=read(ROOT/'content/native-field-equipment.json')
 from tools.field_equipment import PARAMETERS
 require(len(inputs)==3 and all(equipment['parameters'][v['parameter']]==v['mask']for v in inputs),'Prompt actual input/equipment source differs')
 for v in inputs:v['parameter_id']=PARAMETERS.index(v['parameter'])+1
 return dict(schema=1,capability=1,rules=1,family=0x454e006b,commit=PIN,scene=SCENE,scene_id=tree['scene_id'],source_sha256=tree['source_sha256'],tree_sha256=sha(TREE),prompt_sha256=canonical(core),sources=ex.sources,
  settings_member='button_prompts',paused_member='_paused',font='Fonts/BottleRocket.tres',internal_group='idle_process_internal',started_signal='animation_started',finished_signal='animation_finished',
  box_size=[box['margin_right']-box['margin_left'],box['margin_bottom']-box['margin_top']],box_alignment=box['alignment'],label_alignment=label['align'],arrow_rotation=arrow['rect_rotation'],arrow_stretch=arrow['stretch_mode'],
  controls=[dict(role=role,position=[v['margin_left'],v['margin_top']],size=[v['margin_right']-v['margin_left'],v['margin_bottom']-v['margin_top']],text=v.get('text',''))for role,v in[(1,box),(2,label),(3,arrow)]],inputs=inputs,adapter_sha256=sha(ROOT/'content/story-input-bindings.json'),equipment_sha256=sha(ROOT/'content/native-field-equipment.json'),records=records)

def derive():
 tree=read(TREE);require(tree['commit']==PIN and tree['scene']==SCENE and tree['scene_id']==node_id('.') and len(tree['records'])==497 and tree['scene_admitted']is False,'House Prompt complete source tree differs')
 records,names,proof=source_instances(tree);core=prompts.build(records,names,proof,CONFIG);prompts.validate(core,CONFIG)
 n=native(core,tree);ex=Extractor(ROOT);script=ex.text(prompts.SCRIPT);scene=ex.text(prompts.SOURCE);global_source='Scripts/global/global.gd';global_text=ex.text(global_source);player_source='Scripts/Main/party/Player.gd';player=ex.text(player_source)
 connections=[]
 require(re.search(r'func get_player\(\)[^\n]*:\s*\n\s*return partyObjects\[0\]',global_text),'Original global.get_player first real party object differs')
 for role,emitter,signal,method,args,bind in [(1,1,'event_detector_entered','_on_player_nearby',1,1),(2,1,'event_detector_exited','_on_player_nearby',1,0),(3,2,'inputs_changed','_set_key_name',0,2),(4,2,'locale_changed','_set_key_name',0,2),(5,1,'paused','_on_player_toggle_pause',0,2),(6,1,'unpaused','_on_player_toggle_pause',0,2)]:
  require(re.search(r'connect\("'+signal+r'", self, "'+method+r'"',script),'Actual Prompt Ready connection differs '+signal)
  declaration=re.search(r'^signal\s+'+signal+r'\s*(?:\(([^\n]*)\))?\s*$',player if emitter==1 else global_text,re.M)
  require(declaration is not None and len([]if not declaration[1]else declaration[1].split(','))==args,'Actual Prompt source signal arity differs')
  connections.append(dict(role=role,emitter=emitter,signal=signal,method=method,arguments=args,flags=0,bind=bind))
 m=re.search(r'\[connection signal="([^"]+)" from="AnimationPlayer" to="\." method="([^"]+)"\]',scene)
 require(m is not None and m[1]=='animation_finished' and m[2]=='_on_AnimationPlayer_animation_finished','Actual Prompt persistent finished source differs')
 require(re.search(r'func '+re.escape(m[2])+r'\(anim_name: String\)',script) and 'anim_name == "Show" and visible' in script and re.search(r'yield\(\$AnimationPlayer,\s*"animation_finished"\)',script) and 'emit_signal("hide")'in script,'Actual Prompt persistent/yield source order differs')
 connections.append(dict(role=7,emitter=3,signal=m[1],method=m[2],arguments=1,flags=2,bind=2))
 abi=read(ROOT/'content/native-field-scene-signal-callbacks.json');symbols={v['role']:v for v in abi['symbols']}
 require(abi['commit']==PIN and abi['wait_flags']==4 and symbols[13]['name']=='_signal_callback' and symbols[14]['name']=='GDScriptFunctionState' and set(abi['engine'])=={'gdscript_function.cpp','object.cpp'},'Actual engine FunctionState ABI proof differs')
 sources=dict(core['sources']);sources.update(n['sources']);sources.update(ex.sources)
 methods=['press_button','set_enabled','force_show','force_hide','unforce_show_hide']
 for method in methods:require(re.search(r'^func '+method+r'\(',script,re.M),'Actual Prompt source method missing '+method)
 material_text=ex.text('Shaders/Flash.tres');inline=re.search(r'\[sub_resource type="Shader" id=(\d+)\]\s+code = ',material_text)
 require(inline is not None,'Actual Prompt embedded shader declaration missing')
 code=json.JSONDecoder(strict=False).raw_decode(material_text[inline.end():])[0]
 require(code==ex.text('Shaders/Flash.shader'),'Actual embedded/file Flash shader semantics differ')
 properties=prompts.properties(material_text.split('[resource]\n')[1]);require(properties['resource_local_to_scene']is True,'Actual Prompt local material clone changed')
 native_export=read(NATIVE);by_node={v['path']:v for v in native_export['nodes']};native_resources={v['id']:v for v in native_export['resources']};clones={}
 for prompt in core['records']:
  ref=by_node[prompt['node']]['properties']['material'];require(ref['type']=='ResourceReference','Actual Prompt original material missing');v=native_resources[ref['id']]
  require(v['class']=='ShaderMaterial'and v['properties']['resource_local_to_scene']is True,'Actual Prompt unique native material type/local flag differs');clones[str(prompt['id'])]=ref['id']
 require(len(set(clones.values()))==len(core['records']),'Actual Prompt local materials unexpectedly shared')
 font=ROOT/'romfs/fonts/source-fonts.encfont';font_ir=ROOT/'content/asset-receipts/fonts/source.json'
 require(font.is_file() and font_ir.is_file(),'Actual retained source font catalog missing')
 font_receipt=read(font_ir);require(font_receipt['binary']['sha256']==sha(font) and font_receipt['binary']['bytes']==font.stat().st_size,'Actual retained font receipt/catalog differs')
 faces=[v for v in font_receipt['faces']if v['source']==n['font']];require(len(faces)==1,'Actual BottleRocket font face absent/ambiguous')
 face=faces[0];pages=font_receipt['pages'][face['first_page']:face['first_page']+face['page_count']]
 for page in pages:require(sha(ROOT/'romfs/fonts'/page['path'])==page['sha256'] and (ROOT/'romfs/fonts'/page['path']).stat().st_size==page['file_bytes'],'Actual BottleRocket font page differs')
 return dict(schema=1,format=1,capability=1,rules=1,family=FAMILY,commit=PIN,scene=SCENE,scene_id=tree['scene_id'],source_sha256=tree['source_sha256'],tree_ir_sha256=sha(TREE),sources=dict(sorted(sources.items())),core=core,native=n,connections=connections,
  hide_signal='hide',offset_member='offset',enabled_member='enabled',material_source='Shaders/Flash.tres',shader_source='Shaders/Flash.tres',shader_declaration=int(inline[1]),shader_code_sha256=hashlib.sha256(code.encode()).hexdigest(),material_glow=properties['shader_param/glow_color'],material_local_to_scene=True,native_material_clones=clones,methods=methods,wait=dict(id=stable('house-button-prompt-function-state'),flags=4,native_class=symbols[14]['name'],method=symbols[13]['name'],source=prompts.SCRIPT,source_sha256=sources[prompts.SCRIPT],engine=abi['engine'],abi_ir_sha256=sha(ROOT/'content/native-field-scene-signal-callbacks.json')),
  font=dict(path=font.relative_to(ROOT/'romfs').as_posix(),sha256=sha(font),bytes=font.stat().st_size,ir_sha256=sha(font_ir),source_path=face['fallback_chain'][0],source=sources[face['fallback_chain'][0]],face=face,pages=pages,license_review_sha256=sha(ROOT/'content/source-fonts-review.json')),scene_admitted=False)

def review(d):
 return dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),shared_producers={p:sha(ROOT/p)for p in('tools/field_prompts.py','tools/prompt_native.py')},tree_ir_sha256=d['tree_ir_sha256'],sources=d['sources'],engine_sources=d['wait']['engine'],font=d['font'],prompt_count=23,native_leaf_count=92,connection_count=7,scene_admitted=False,semantics=['Complete House source scope, inherited properties merged from inner instance to outer overrides; original 23 postorder Ready roots','Embedded original ENCFPR01 and ENCPRN01 execute the same prompt clip clock, native HBox/Label/Arrow and font/GPU owners','Original offset is a source field; serialized InteractDialog setget writes it before Enter/Ready, not a substitute Canvas position','Actual AP persistent finished connection precedes actual one-shot FunctionState Press resume; missing/wrong/reentrant/duplicate receipts reject','All source signals and callback strings are authoring data; data admission grants no construction, Enter, Ready or drawing'])

def load():
 d=read(IR);require(d==derive() and read(REVIEW)==review(d),'House ButtonPrompt source/IR/review stale');return d

def encode(d):
 require(d==derive(),'House ButtonPrompt encode source differs');core=prompts.encode(d['core'],CONFIG);native_raw=prompt_native.encode(d['native']);out=bytearray()
 def u(*v):out.extend(struct.pack('<'+'I'*len(v),*v))
 def s(v):b=v.encode();u(len(b));out.extend(b)
 out.extend(bytes.fromhex(d['tree_ir_sha256']));out.extend(hashlib.sha256(core).digest());out.extend(hashlib.sha256(native_raw).digest())
 for key in('hide_signal','offset_member','enabled_member','material_source','shader_source'):s(d[key])
 u(d['shader_declaration'],int(d['material_local_to_scene']));out.extend(bytes.fromhex(d['shader_code_sha256']));out.extend(struct.pack('<4f',*d['material_glow']))
 u(len(d['methods']))
 for name in d['methods']:s(name)
 w=d['wait'];s(w['source']);out.extend(bytes.fromhex(w['source_sha256']));u(w['id'],w['flags']);s(w['native_class']);s(w['method'])
 f=d['font'];s(f['path']);out.extend(bytes.fromhex(f['sha256']));u(f['bytes']);out.extend(bytes.fromhex(f['ir_sha256']));s(f['source_path']);out.extend(bytes.fromhex(f['source']))
 u(len(d['sources']))
 for p,h in sorted(d['sources'].items()):s(p);out.extend(bytes.fromhex(h))
 u(len(d['connections']))
 for c in d['connections']:u(c['role'],c['emitter']);s(c['signal']);s(c['method']);u(c['arguments'],c['flags'],c['bind'])
 u(len(core));out.extend(core);u(len(native_raw));out.extend(native_raw)
 return b'ENCHBPR1'+struct.pack('<7I',1,1,1,FAMILY,d['scene_id'],len(out),zlib.crc32(out))+bytes.fromhex(PIN)+bytes.fromhex(d['source_sha256'])+bytes.fromhex(sha(IR))+out

def stage_files(root):
 d=load();relative=Path('data/house-return.encbuttonprompt');raw=encode(d);require((Path(root)/relative).read_bytes()==raw,'House ButtonPrompt staged pack differs');files={relative:raw}
 for path,h in [(d['core']['art']['path'],d['core']['art']['sha256']),(d['font']['path'],d['font']['sha256'])]:
  data=(Path(root)/path).read_bytes();require(hashlib.sha256(data).hexdigest()==h,'House ButtonPrompt actual asset bytes differ '+path);files[Path(path)]=data
 for page in d['font']['pages']:
  path=Path('fonts')/page['path'];data=(Path(root)/path).read_bytes();require(len(data)==page['file_bytes']and hashlib.sha256(data).hexdigest()==page['sha256'],'House ButtonPrompt actual font texture differs');files[path]=data
 return files

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);a=p.parse_args()
 if a.action=='extract':d=derive();write(IR,d);write(REVIEW,review(d))
 else:PACK.parent.mkdir(parents=True,exist_ok=True);raw=encode(load());PACK.write_bytes(raw);print('House ButtonPrompt: 23 roots/92 native leaves/7 source connections;',len(raw),'bytes; no Ready admitted')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('HOUSE BUTTON PROMPT ERROR: '+str(e))
