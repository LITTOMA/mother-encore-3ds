#!/usr/bin/env python3
"""Audited static House prompt + source settings preview; no game-script runtime."""
import argparse,hashlib,json,math,os,re,struct,subprocess,sys,zlib
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.ui_presentation_bindings import load as load_ui_bindings
from tools.extract_battle_entry import Extractor,node,require
IR=ROOT/'content/native-house-button-prompts.json';PACK=ROOT/'romfs/data/opening.encprompts';REPORT=ROOT/'reports/new-game-settings/prompts';WORK=ROOT/'build/house-button-prompts'

def checked_bindings():return load_ui_bindings('prompts-presentation-bindings.json',BINDING_SCHEMA,ROOT,expected_checks=41,expected_contracts=102)
BINDING_SCHEMA={'source_0':str,'source_1':str,'source_2':str,'source_3':str,'source_4':str,'source_5':str,'source_6':str,'source_7':str,'source_8':str,'source_9':str,'source_10':str,'source_11':str,'source_12':str,'source_13':str,'source_14':str,'source_15':str,'source_16':str,'source_17':str,'source_18':str,'node_0':str,'node_1':str,'node_2':str,'node_3':str,'node_4':str,'node_5':str,'node_6':str,'node_7':str,'preview_rows':[[str,str,int],[str,str,int]],'preview_root':str,'preview_prompt_suffix':str,'choice_masks':[int,int,int,int],'choices':[str,str,str,str],'house_target_rows':[[int,str,int],[int,str,int]],'phone_kind':int,'phone_category':int,'door_origin':float,'color':int,'resource_root':str,'resource_suffix':str,'accept_name':str,'label':str,'arrow_rotation':int}
b=checked_bindings()

PROBE='''extends SceneTree
func _init():call_deferred("run")
func run():
 var f=File.new()
 assert(f.open("res://input.json",File.READ)==OK)
 var c=JSON.parse(f.get_as_text()).result
 f.close()
 var fd=DynamicFontData.new()
 fd.font_path=c.font
 fd.antialiased=false
 var font=DynamicFont.new()
 font.font_data=fd
 font.outline_size=c.outline
 font.extra_spacing_char=c.char_spacing
 font.extra_spacing_space=c.space_spacing
 var root=Control.new()
 get_root().add_child(root)
 var box=HBoxContainer.new()
 root.add_child(box)
 box.rect_position=Vector2(c.box[0],c.box[1])
 box.rect_size=Vector2(c.box[2],c.box[3])
 box.alignment=c.alignment
 var label=Label.new()
 label.text=c.label
 label.align=c.align
 label.add_font_override("font",font)
 box.add_child(label)
 for _i in range(4):yield(self,"idle_frame")
 var out={"engine":Engine.get_version_info(),"size":font.size,"height":font.get_height(),"ascent":font.get_ascent(),"advance":font.get_string_size(c.label).x,"label":[label.rect_global_position.x,label.rect_global_position.y,label.rect_size.x,label.rect_size.y]}
 assert(f.open("res://reference.json",File.WRITE)==OK)
 f.store_string(JSON.print(out,"  "))
 f.close()
 print("HOUSE_PROMPT_LAYOUT_OK")
 quit()
'''
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def rect(p):
 x,y=p.get('margin_left',0),p.get('margin_top',0);return [x,y,p.get('margin_right',0)-x,p.get('margin_bottom',0)-y]
def build(tex3ds,godot):
 global b;b=checked_bindings()
 ex=Extractor(ROOT);scene=ex.text(b['source_2']);script=ex.text(b['source_3']);player=ex.text(b['source_4']);gd=ex.text(b['source_5']);house=ex.text(b['source_6']);npc=ex.text(b['source_7']);phone=ex.text(b['source_8']);door=ex.text(b['source_9']);ds=ex.text(b['source_10']);menu=ex.text(b['source_11']);ex.text(b['source_12']);ex.text(b['source_13']);ex.data(b['source_14'])
 require('globaldata.button_prompts in [type, "Both"] and enabled'in script and '_can_show() and _player_nearby and !global.get_player().is_paused()'in script,'Unreviewed prompt visibility')
 require('_set_event_collider(eventRayCaster.get_collider())'in player and 'eventRayCaster.rotation = _direction.angle() - TAU/4'in player,'Unreviewed detector')
 require('$interact/ButtonPrompt.enabled = false'in ds and '$interact/ButtonPrompt.enabled = true'in ds,'Unreviewed door enablement')
 choices=json.loads(re.search(r'const BUTTON_PROMPTS := (\[[^\n]+\])',gd)[1]);require(set(choices)==set(b['choices']),'Unreviewed prompt choices');masks=dict(zip(b['choices'],b['choice_masks']))
 source_font=ex.text(b['source_15']);font_file=b['source_0'];ex.data(font_file)
 hp=node(scene,b['node_0']);label=node(scene,b['node_1']);arrow=node(scene,b['node_2']);require(label['text']==b['label'] and arrow['rect_rotation']==b['arrow_rotation'],'Unreviewed CTR prompt glyph/arrow')
 config=dict(font=str(ex.upstream/font_file),outline=int(re.search(r'outline_size = (\d+)',source_font)[1]),char_spacing=int(re.search(r'extra_spacing_char = (-?\d+)',source_font)[1]),space_spacing=int(re.search(r'extra_spacing_space = (-?\d+)',source_font)[1]),box=rect(hp),alignment=hp['alignment'],label=label['text'],align=label['align'])
 WORK.mkdir(parents=True,exist_ok=True);REPORT.mkdir(parents=True,exist_ok=True);(WORK/'input.json').write_text(json.dumps(config));(WORK/b['source_18']).write_text('config_version=4\n[logging]\nfile_logging/enable_logging=false\n');(WORK/'probe.gd').write_text(PROBE)
 result=subprocess.run([str(godot),'--path',str(WORK),'-s','probe.gd'],capture_output=True,text=True,env=dict(os.environ,XDG_DATA_HOME=str(WORK/'userdata')),timeout=30);(REPORT/'native-layout.log').write_text(result.stdout+result.stderr);require(result.returncode==0 and 'HOUSE_PROMPT_LAYOUT_OK'in result.stdout,'Native font/layout probe failed')
 ref=json.loads((WORK/'reference.json').read_text());require(ref['engine']['hash']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8','Unreviewed Godot');(REPORT/'native-layout.json').write_text(json.dumps(ref,indent=2)+'\n')
 f=ImageFont.truetype(str(ex.upstream/font_file),ref['size']);require(abs(f.getlength(config['label'])-ref['advance'])<.001,'Source font advance mismatch')
 arrow_path=b['source_1'];arrow_image=Image.open(ex.upstream/arrow_path).convert('RGBA');ex.data(arrow_path);ar=rect(arrow);require(list(arrow_image.size)==ar[2:],'Arrow source dimensions changed');arrow_image=arrow_image.transpose(Image.Transpose.ROTATE_270);ax,ay=ar[0]-ar[3],ar[1]
 lx,ly,lw,lh=ref['label'];outline=config['outline'];left=math.floor(min(lx-outline,ax));top=math.floor(min(ly-outline,ay));right=math.ceil(max(lx+lw+outline,ax+arrow_image.width));bottom=math.ceil(max(ly+lh+outline,ay+arrow_image.height));image=Image.new('RGBA',(right-left,bottom-top));draw=ImageDraw.Draw(image);draw.fontmode='1';draw.text((lx-left,ly-top+ref['ascent']-f.getmetrics()[0]),config['label'],font=f,fill='white',stroke_width=outline,stroke_fill='black');image.alpha_composite(arrow_image,(int(ax-left),int(ay-top)))
 resources=[]
 def asset(name,image):
  path=b['resource_root']+name+b['resource_suffix'];out=ROOT/'romfs'/path;out.parent.mkdir(parents=True,exist_ok=True);png=WORK/(name+'.png');image.save(png);subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(out),str(png)],check=True);resources.append(dict(path=path,width=image.width,height=image.height,sha256=sha(out)));return len(resources)-1
 asset(b['accept_name'],image);previews=[]
 for name,source,category in b['preview_rows']:
  p=node(menu,b['preview_root']+name);prompt=node(menu,b['preview_root']+name+b['preview_prompt_suffix']);im=Image.open(ex.upstream/source).convert('RGBA');ex.data(source);w,h=im.width//p.get('hframes',1),im.height//p.get('vframes',1);frame=p.get('frame',0);x,y=frame%p.get('hframes',1)*w,frame//p.get('hframes',1)*h;idx=asset(name.lower(),im.crop((x,y,x+w,y+h)));previews.append(dict(resource=idx,category=category,position=p['position'],offset=prompt['offset']))
 h=json.loads((ROOT/'content/native-house.json').read_text());p=json.loads((ROOT/'content/phone-stage/presentation.json').read_text());targets=[]
 for kind,key,category in b['house_target_rows']:
  for i,row in enumerate(h[key]):
   if kind==1:base=node(npc,b['node_5']).get('position',[0,0]);off=node(npc,b['node_3'])['offset'];offset=[base[j]+off[j] for j in range(2)]
   else:off=node(door,b['node_4'])['offset'];base_y=node(house,row['source_path']).get('door_offset',node(door,b['node_7'])['door_offset'])[1]+b['door_origin'];offset=[off[0],base_y+off[1]]
   targets.append(dict(kind=kind,index=i,category=category,source_path=row['source_path'],position=row['position'],center=row['interact_center'],extents=row['interact_extents'],offset=offset))
 for i,row in enumerate(p['objects']):targets.append(dict(kind=b['phone_kind'],index=i,category=b['phone_category'],source_path=row['source_path'],position=row['position'],center=row['interaction']['center'],extents=row['interaction']['effective_extents'],offset=node(phone,b['node_6'])['offset']))
 data=dict(schema=1,capability=1,commit=ex.lock['commit'],sources=ex.sources,dependencies={p:sha(ROOT/p) for p in ['content/native-house.json','content/phone-stage/presentation.json']},choices=choices,choice_masks=[masks[c] for c in choices],ray_origin=h['interaction']['ray_origin'],ray_length=h['interaction']['ray_length'],collision_mask=h['interaction']['collision_mask'],resources=resources,prompt_rect=[left,top,image.width,image.height],color=b['color'],previews=previews,targets=targets,scope='Static source visible A+arrow pose, CTR A maps ui_accept; current supported NPC/door/phone ray rectangles. Additional occlusion supplied by caller; no general Godot broadphase or Show/Float/Hide/Press choreography.',license_review=json.loads((ROOT/'content/native-save-menu.json').read_text())['licence_review'])
 IR.write_text(json.dumps(data,indent=2)+'\n');return data

def encode(d):
 require(d['schema']==1 and d['capability']==1,'Prompt schema');b=bytearray()
 def integers(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def floats(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def text(s):v=s.encode();integers(len(v));b.extend(v)
 floats(*d['ray_origin'],d['ray_length']);integers(d['collision_mask'],len(d['choice_masks']),*d['choice_masks']);integers(len(d['resources']))
 for r in d['resources']:text(r['path']);integers(r['width'],r['height'])
 floats(*d['prompt_rect']);integers(d['color'],len(d['previews']))
 for p in d['previews']:integers(p['resource'],p['category']);floats(*p['position'],*p['offset'])
 integers(len(d['targets']))
 for t in d['targets']:integers(t['kind'],t['index'],t['category']);text(t['source_path']);floats(*t['position'],*t['center'],*t['extents'],*t['offset'])
 return b'ENCPRMPT'+struct.pack('<4I',1,24+len(b),zlib.crc32(b)&0xffffffff,1)+b

def verify(d):
 global b;b=checked_bindings()
 ex=Extractor(ROOT)
 require(d['schema']==1 and d['capability']==1 and d['commit']==ex.lock['commit'],'Prompt schema/pin mismatch')
 for path,digest in d['sources'].items():require(sha(ex.upstream/path)==digest,'Prompt source changed '+path)
 for path,digest in d['dependencies'].items():require(sha(ROOT/path)==digest,'Prompt dependency changed '+path)
 for r in d['resources']:
  path=Path(r['path']);require(not path.is_absolute() and '..' not in path.parts and r['path'].startswith(b['resource_root']) and '\\' not in r['path'] and ':' not in r['path'],'Unsafe prompt resource')
  require(sha(ROOT/'romfs'/path)==r['sha256'],'Prompt resource changed')
 require(len(d['resources'])==3 and len(d['previews'])==2,'Prompt asset scope changed')
 gd=ex.text(b['source_5']);choices=json.loads(re.search(r'const BUTTON_PROMPTS := (\[[^\n]+\])',gd)[1]);masks=dict(zip(b['choices'],b['choice_masks']))
 require(d['choices']==choices and d['choice_masks']==[masks[c] for c in choices],'Prompt source choices changed')
 h=json.loads((ROOT/'content/native-house.json').read_text());p=json.loads((ROOT/'content/phone-stage/presentation.json').read_text())
 require(all(d[k]==h['interaction'][k] for k in ['ray_origin','ray_length','collision_mask']),'Prompt source ray changed')
 expected=[];npc=ex.text(b['source_7']);door=ex.text(b['source_9']);house=ex.text(b['source_6']);phone=ex.text(b['source_8'])
 for kind,key,category in b['house_target_rows']:
  for i,row in enumerate(h[key]):
   if kind==1:offset=[a+b for a,b in zip(node(npc,b['node_5']).get('position',[0,0]),node(npc,b['node_3'])['offset'])]
   else:off=node(door,b['node_4'])['offset'];base_y=node(house,row['source_path']).get('door_offset',node(door,b['node_7'])['door_offset'])[1]+b['door_origin'];offset=[off[0],base_y+off[1]]
   expected.append(dict(kind=kind,index=i,category=category,source_path=row['source_path'],position=row['position'],center=row['interact_center'],extents=row['interact_extents'],offset=offset))
 for i,row in enumerate(p['objects']):expected.append(dict(kind=b['phone_kind'],index=i,category=b['phone_category'],source_path=row['source_path'],position=row['position'],center=row['interaction']['center'],extents=row['interaction']['effective_extents'],offset=node(phone,b['node_6'])['offset']))
 require(d['targets']==expected,'Prompt source target geometry changed')
 menu=ex.text(b['source_11'])
 for row,name,category in zip(d['previews'],[r[0] for r in b['preview_rows']],[r[2] for r in b['preview_rows']]):require(row['position']==node(menu,b['preview_root']+name)['position'] and row['offset']==node(menu,b['preview_root']+name+b['preview_prompt_suffix'])['offset'] and row['category']==category,'Prompt preview source changed')

def stage_files(source_root):
 global b;b=checked_bindings()
 d=json.loads(IR.read_text());verify(d);root=Path(source_root);relative=Path('data/opening.encprompts');binary=(root/relative).read_bytes();require(binary==encode(d),'Stale staged prompt pack');files={relative:binary}
 for r in d['resources']:
  path=Path(r['path']);raw=(root/path).read_bytes();require(hashlib.sha256(raw).hexdigest()==r['sha256'],'Staged prompt texture mismatch');files[path]=raw
 return files

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['assets','compile','verify']);p.add_argument('--tex3ds',type=Path);p.add_argument('--godot',type=Path);a=p.parse_args();d=build(a.tex3ds,a.godot) if a.action=='assets' else json.loads(IR.read_text());verify(d);blob=encode(d)
 if a.action=='verify':require(PACK.read_bytes()==blob,'Prompt pack stale')
 else:PACK.write_bytes(blob)
 REPORT.mkdir(parents=True,exist_ok=True);(REPORT/'build.json').write_text(json.dumps(dict(bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),scope=d['scope']),indent=2)+'\n');print('House prompts:',len(blob),'bytes')
if __name__=='__main__':main()
