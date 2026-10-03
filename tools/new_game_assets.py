#!/usr/bin/env python3
"""Source-audited six startup naming fields and indexed live battle-name bindings.

This does not claim settings/final confirmation/intro. Unsupported
non-ASCII input cells retain source positions and are dimmed, never remapped.
"""
from __future__ import annotations
import argparse,csv,hashlib,io,json,os,re,struct,subprocess,sys,zlib
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require,one,node,properties
from tools.upstream import read_json,write_json
from tools.menu_audio_binding import source_sound
IR=ROOT/'content/native-new-game.json';PACK=ROOT/'romfs/data/opening.encnewgame';REPORT=ROOT/'reports/new-game-setup'
SCENE='Maps/Naming screen.tscn';SCRIPT='Scripts/UI/NamingScreen/Naming screen.gd'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def rect(p):
 x,y=p.get('margin_left',0),p.get('margin_top',0);return [x,y,p.get('margin_right',0)-x,p.get('margin_bottom',0)-y]
def rgba(v):return sum(round(x*255)<<(8*i)for i,x in enumerate(v))
def translate(ex,file):return {r['key']:r['en']for r in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/'+file+' - sheet.csv')))}
PROBE='''extends SceneTree
func _init(): call_deferred("run")
func run():
 var f=File.new()
 assert(f.open("res://input.json",File.READ)==OK)
 var c=JSON.parse(f.get_as_text()).result
 f.close()
 var fd=DynamicFontData.new()
 fd.font_path=c.font
 var font=DynamicFont.new()
 font.font_data=fd
 font.extra_spacing_top=-1
 font.extra_spacing_bottom=-1
 var root=Control.new()
 root.rect_size=Vector2(320,180)
 get_root().add_child(root)
 var nodes={"":root}
 for row in c.nodes:
  var n
  if row.type=="GridContainer": n=GridContainer.new()
  elif row.type=="Label": n=Label.new()
  else: n=Control.new()
  nodes[row.parent].add_child(n)
  for key in row.props: n.set(key, row.props[key])
  if n is Label:
   n.add_font_override("font",font)
   n.text=row.text
  nodes[row.path]=n
 for i in range(5): yield(self,"idle_frame")
 var out={"engine":Engine.get_version_info(),"keys":[],"height":font.get_height(),"ascent":font.get_ascent()}
 for path in c.keys:
  var n=nodes[path]
  out.keys.append([n.rect_global_position.x,n.rect_global_position.y,n.rect_size.x,n.rect_size.y])
 assert(f.open("res://reference.json",File.WRITE)==OK)
 f.store_string(JSON.print(out,"  "))
 f.close()
 print("NEW_GAME_LAYOUT_OK")
 quit()
'''
def base():
 ex=Extractor(ROOT);scene=ex.text(SCENE);script=ex.text(SCRIPT);sequence=ex.yaml('Data/NamingSequences/intro.yaml');save=ex.yaml('Data/save_new_game.yaml')
 ex.data('Scripts/UI/Title screen.gd');ex.data('Scripts/global/audioManager.gd');ex.data('Scripts/UI/cursor.gd');ex.data('Nodes/Ui/arrow.tscn');ex.data('Scripts/global/PartyMember.gd');ex.data('Scripts/global/global.gd');ex.data('Scripts/global/text_tools.gd');ex.data('LICENSE')
 menus=translate(ex,'menus');keyboard=translate(ex,'keyboard')
 steps=sequence['scenario'];targets=[s.get('target')for s in steps];require(targets==['ninten','ana','lloyd','pippi','teddy','favorite_food',None,None],'New naming stages require review')
 require('var index = 0' in script and 'index = fmod(i + 1, DONT_CARE_CHOICES)'in script and 'name_to_check.to_lower().strip_edges()'in script,'Naming rules changed')
 require('if index != _current_step and checkname == _input_results[index].to_lower().strip_edges()'in script,'Duplicate validation changed')
 require('if _input_text != "" or _pinyin.text != ""'in script and '\t\t_set_prev_step()'in script,'Cancel behavior changed')
 require(save['ninten']['nickname']=='' and all(save[t]['nickname']==''for t in targets[1:5]),'New game initial naming changed')
 first=steps[0];count=int(one(r'const DONT_CARE_CHOICES := (\d+)',script,'default choices')[1]);require(count==7,'New default cycle')
 groups=['CanvasLayer/NamingBox/Grid/GridContainer','CanvasLayer/NamingBox/Grid/GridContainer2','CanvasLayer/NamingBox/Grid/GridContainer3','CanvasLayer/NamingBox/CommandGrid/GridContainer4','CanvasLayer/NamingBox/CommandGrid/GridContainer5']
 records=[];key_paths=[];key_groups=[]
 for match in re.finditer(r'^\[node name="(.*?)"[^\n]*parent="([^"]+)"[^\n]*\]\n(.*?)(?=^\[|\Z)',scene,re.M|re.S):
  name,parent,body=match.groups();path=parent+'/'+name
  if path=='CanvasLayer/NamingBox' or path.startswith('CanvasLayer/NamingBox/Grid') or path.startswith('CanvasLayer/NamingBox/CommandGrid'):
   if path not in ['CanvasLayer/NamingBox','CanvasLayer/NamingBox/Grid','CanvasLayer/NamingBox/CommandGrid']+groups and parent not in groups and name!='Empty':continue
   p=properties(body)
   typ='Label'if parent in groups else('GridContainer'if path.endswith(('Grid','GridContainer','GridContainer2','GridContainer3','GridContainer4','GridContainer5'))else'Control')
   props={k:v for k,v in p.items()if k.startswith(('margin_','custom_constants/','size_flags_'))or k in ['columns','align']}
   if 'rect_min_size'in p:props['rect_min_size']=p['rect_min_size'] # converted by probe setup below
   text=menus.get(p.get('text',''),p.get('text','')) if parent in groups[3:] else 'A'
   records.append(dict(path=path,parent=''if path=='CanvasLayer/NamingBox'else parent,type=typ,props=props,text=text))
   if parent in groups:key_paths.append(path);key_groups.append(groups.index(parent))
 require([key_groups.count(i)for i in range(5)]==[30,30,25,1,2],'Keyboard node topology changed')
 # JSON vector conversion is handled explicitly in the probe, not by Godot coercion.
 probe=dict(font=str(ex.upstream/'Fonts/EBMain.ttf'),nodes=records,keys=key_paths)
 save_ir=read_json(ROOT/'content/native-save-menu.json');font=read_json(ROOT/'romfs/battle-preview/source.json');cps={g['codepoint']for g in font['glyphs'] if g['advance']>0}
 panels=[]
 for panel in range(2):
  chars=[]
  for group in range(3):
   text=keyboard[f'KEYBOARD_PANEL{panel}_GRID{group}'];chars.extend(list(text)+['']*(key_groups.count(group)-len(text)))
  require(len(chars)==85,'Keyboard translation overflow');panels.append([dict(codepoint=ord(c)if c else 0,value=c if len(c)==1 and 32<=ord(c)<=126 and ord(c)in cps else '',kind=0)for c in chars]+[dict(codepoint=0,value=menus[k],kind=i+1)for i,k in enumerate(['MENU_DONT_CARE','MENU_BACKSPACE','MENU_OK'])])
 resources=[]
 for idx in [save_ir['flavors'][0]['confirm_resource'],save_ir['arrow']]:
  a=save_ir['resources'][idx];resources.append({k:a[k]for k in ['path','width','height','columns','rows']})
 opening=read_json(ROOT/'content/native-opening.json');profile=opening['sections']['ActorProfile'][0]
 for index in [profile['primary_resource'],profile['shadow_resource']]:
  a=opening['sections']['Resource'][index];resources.append(dict(path=opening['strings'][a['path_string']],**{k:a[k]for k in ['width','height','columns','rows']}))
 npc=ex.text('Nodes/Reusables/npc.tscn');ex.data('Scripts/Main/npc.gd');ex.data('Scripts/Main/character_sprite.gd');anim=ex.yaml('Data/Animations/PartyMember.yaml');walk=anim['animations']['Walk'];require(walk['type']==0 and anim['size']==[10,20],'Naming actor animation changed')
 timeline=[];t=walk['directions'][0][0]
 for frame,duration in walk['directions'][0][1:]:timeline.append([t,frame-1]);t+=duration
 sprite=node(npc,'CharacterSprite');origin=[a+b for a,b in zip(node(scene,'Objects/Actors')['position'],node(npc,'.')['position'])];frameh=resources[2]['height']/resources[2]['rows'];offset=[origin[0]+sprite['position'][0]+anim['offset'][0],origin[1]+sprite['position'][1]-int(frameh/2)+anim['offset'][1]]
 texts=[menus[first['prompt']],menus['NAME_BLOCKED'],menus['NAME_DUPLICATED'],menus['SYMBOL_BULLET_NAMING'],menus['SYMBOL_DOT'],keyboard['KEYBOARD_PANEL1_NAME'],keyboard['KEYBOARD_PANEL0_NAME']]
 r=dict(schema=1,commit=ex.lock['commit'],license_review=save_ir['licence_review'],scope='First original Ninten naming stage only; remaining roster/food/settings/final confirmation/intro are not ported. ASCII input subset retains source keyboard cells; unsupported glyphs are dimmed and skipped. Successful OK continues existing playable opening.',target=first['target'],maximum=len(menus[first['longest_name']]),initial=save['ninten']['nickname'],other_initial=[save[t]['nickname']for t in targets[1:5]]+[save['favoritefood']],defaults=[menus[first['dont_care']+str(i)]for i in range(count)],blacklist=menus['BLACKLISTED_NAMES'].split(',')+[''],texts=texts,panels=panels,groups=key_groups,columns=[node(scene,g)['columns']for g in groups],resources=resources,layouts=[rect(node(scene,p))for p in ['CanvasLayer/NameBox','CanvasLayer/NameBox/Label','CanvasLayer/NameBox/name','CanvasLayer/NameBox/name/Label','CanvasLayer/NamingBox']],patch=[node(scene,'CanvasLayer/NameBox')['patch_margin_'+s]for s in ['left','top','right','bottom']],colors=[rgba(node(scene,'Background')['color']),save_ir['text_color'],rgba(node(scene,'CanvasLayer/NameBox/name')['color']),rgba(node(scene,'CanvasLayer/NameBox/name/Label')['custom_colors/font_color']),0xff866c7a],actor=dict(position=offset,shadow=[origin[i]+node(npc,'Shadow')['position'][i]for i in range(2)],length=t,keys=timeline),arrow=dict(offset=save_ir['layouts'][18][:2],keys=save_ir['arrow_keys'],length=save_ir['arrow_loop'],move=save_ir['arrow_move']),error_duration=float(one(r'const BLINK_DURATION := ([\d.]+)',script,'error blink')[1])*4+.8,cancel_delay=float(one(r'create_timer\(([\d.]+)\)',script,'cancel delay')[1]),sounds=[source_sound(s)for s in ['cursor1','cursor2','back']],sources=ex.sources,dependencies={p:sha(ROOT/p)for p in ['content/native-save-menu.json','content/native-opening.json','content/native-session.json','romfs/battle-preview/source.json']})
 r['layouts'][3][2]+=r['layouts'][2][2];r['layouts'][3][3]+=r['layouts'][2][3]
 r['schema']=2
 r['scope']='Six source startup naming fields; only Ninten is playable. Remaining settings/final confirmation/intro and transition choreography are not ported. Food is a real session field. ASCII subset retains source keyboard cells.'
 # Keep the first six indices stable for the established font/keyboard renderer.
 # Newly needed original sprite atlases follow those two keyboard layers.
 r['field_assets']=[];r['fields']=[]
 for index,step in enumerate(steps[:6]):
  food=step['target']=='favorite_food'
  field=dict(target=step['target'],kind=int(food),maximum=len(menus[step['longest_name']]),initial='' if food else save[step['target']]['nickname'],defaults=[menus[step['dont_care']+str(i)]for i in range(count)],prompt=menus[step['prompt']],resource=2 if index==0 else 5+index,shadow=not food,actor=r['actor'])
  if food:
   require(save['favoritefood']=='','Initial source favorite food changed')
   asset='Graphics/UI/Misc/FavFoodPlate.png';grid=[1,1]
   field['actor']=dict(position=node(scene,'Objects/Actors')['position'],shadow=[0,0],length=1,keys=[[0,0]])
  elif index:
   asset='Graphics/Character Sprites/'+step['sprite']+'/main.png';grid=anim['size']
  if index:
   size=ex.png_size(asset);ex.data(asset+'.import')
   require(size[0]%grid[0]==0 and size[1]%grid[1]==0,'Naming actor grid mismatch')
   if not food:
    field['actor']=dict(r['actor'],position=[offset[0],origin[1]+sprite['position'][1]-int(size[1]/grid[1]/2)+anim['offset'][1]])
   r['field_assets'].append(dict(source=asset,path='new-game-preview/'+step['target']+'.t3x',width=size[0],height=size[1],columns=grid[0],rows=grid[1]))
  r['fields'].append(field)
 return ex,r,probe

def resolve_bindings(ex,r):
 trans={}
 for name in ['battletext','battlers','battleskills','menus']:trans.update(translate(ex,name))
 default=read_json(ROOT/'content/native-session.json')['defaults']['characters'][0]['nickname'];bindings=[]
 # Source templates and explicitly checked English articles identify the actor;
 # no runtime substring replacement of a literal person's name is performed.
 party=trans['ARTICLES_NINTEN'].split(',')
 for path in ['content/native-round.json','content/doll-round.json','content/pillow-round.json']:
  ir=read_json(ROOT/path);r['dependencies'][path]=sha(ROOT/path)
  for index,row in enumerate(ir['texts']):
   raw=row['source_text'];expected=row['text'];key=row['key']
   if key:require(trans.get(key,key)==raw,'Battle translation source changed: '+key)
   marker='{nickname}';template=expected
   if default in expected:
    # These are the reviewed upstream uses of PartyMember.get_nickname().
    if row['role']in [3,9,10,12] and '{name}'in raw and expected.startswith(party[0]+default):
     position=len(party[0]);token='name'
    elif row['role']in [3,7] and '{target}'in raw and expected.count(default)==1:
     position=expected.index(default);token='target'
    else:raise ValueError('Unreviewed player-name binding '+repr(row))
    # Assert the source contains a real tag, not an authored literal name.
    require(raw.count('{'+token+'}')==1 and default not in raw and expected.count(default)==1,'Ambiguous source-name binding')
    template=expected[:position]+marker+expected[position+len(default):]
   bindings.append(dict(battle_id=ir['binding']['battle_id'],index=index,key=key,source_text=raw,expected=expected,parts=template.split(marker)))
 r['bindings']=bindings;r['sources']=ex.sources

def graph(panel,groups,columns,rects):
 bygroup=[[i for i,g in enumerate(groups)if g==n]for n in range(5)]
 valid=lambda i:bool(panel[i]['value'])
 def setidx(g,index,current):
  indices=bygroup[g]
  if index<0 or index>=len(indices):return current
  for j in range(index,-1,-1):
   if valid(indices[j]):return indices[j]
  return current
 def closest(cur,g):
  a=rects[cur];point=[a[0]+8-8/6,a[1]+1+4];best=bygroup[g][0]
  def dist(i):b=rects[i];return sum((point[j]-b[j]-rects[best][j+2]/2)**2 for j in range(2))
  for i in bygroup[g]:
   if valid(i)and dist(i)<dist(best):best=i
  return best if valid(best)else cur
 def move(cur,dx,dy):
  g=groups[cur];ids=bygroup[g];local=ids.index(cur);cols=columns[g];row,col=divmod(local,cols);total=(len(ids)+cols-1)//cols;rows=(len(ids)-col+cols-1)//cols
  index=None
  if col==0 and dx:
   if dx==1 and cols>1:index=local+1
  elif col==cols-1 and dx==1:pass
  elif row==rows-1 and dy==1:
   if rows<total:index=len(ids)-1
  elif row==0 and dy==-1:pass
  else:index=local+dx+dy*cols
  if index is not None:
   step=abs(index-local)
   if step:
    sign=1 if index>local else -1
    for j in range(index,len(ids)if sign>0 else -1,sign*step):
     if 0<=j<len(ids)and valid(ids[j]):return ids[j]
  # Naming screen _on_arrow_failed_move, on the actual source subgrids.
  if g==0:
   if dx<0:
    n=closest(cur,2);return setidx(2,bygroup[2].index(n)+columns[2]-1,n)
   if dx>0:return closest(cur,1)
   return closest(cur,3)
  if g==1:
   if dx<0:return closest(cur,0)
   if dx>0:return closest(cur,2)
   n=closest(cur,4 if col>=cols/2 else 3)
   return bygroup[4][0]if dy<0 and col>=cols/2 else n
  if g==2:
   if dx<0:return closest(cur,1)
   if dx>0:
    n=closest(cur,0);return setidx(0,bygroup[0].index(n)-columns[0]+1,n)
   return closest(cur,4)
  if g==3:
   if dx:
    n=closest(cur,4);return setidx(4,bygroup[4].index(n)+columns[4]-1,n)if dx<0 else n
   return bygroup[0][0]if dy>0 else closest(cur,0)
  if dx:return closest(cur,3)
  n=closest(cur,2)
  if dy>0:return setidx(2,0 if bygroup[2].index(n)==0 else columns[2]-1,n)
  return n
 return [[move(i,*d)if valid(i)else i for d in [(-1,0),(1,0),(0,-1),(0,1)]]for i in range(len(panel))]

def extract_assets(tex3ds,godot):
 ex,r,probe=base();build=ROOT/'build/new-game-assets';build.mkdir(parents=True,exist_ok=True);REPORT.mkdir(parents=True,exist_ok=True)
 write_json(build/'input.json',probe);(build/'project.godot').write_text('config_version=4\n[logging]\nfile_logging/enable_logging=false\n')
 code=PROBE.replace('for key in row.props: n.set(key, row.props[key])','for key in row.props:\n   if key=="rect_min_size": n.set(key, Vector2(row.props[key][0],row.props[key][1]))\n   else: n.set(key, row.props[key])')
 (build/'probe.gd').write_text(code);run=subprocess.run([str(godot),'--path',str(build),'-s','probe.gd'],text=True,capture_output=True,timeout=30,env=dict(os.environ,XDG_DATA_HOME=str(build/'userdata')));(REPORT/'native-layout.log').write_text(run.stdout+run.stderr);require(run.returncode==0 and 'NEW_GAME_LAYOUT_OK'in run.stdout,'Native layout probe failed')
 ref=read_json(build/'reference.json');require(ref['engine']['hash']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8','Unreviewed Godot');write_json(REPORT/'native-layout.json',ref)
 r['rects']=ref['keys'];r['font_height']=ref['height'];r['native_layout_sha256']=sha(REPORT/'native-layout.json')
 font=ImageFont.truetype(str(ex.upstream/'Fonts/EBMain.ttf'),16);ex.data('Fonts/EBMain.ttf');ex.data('Fonts/EBMain.tres');outputs={}
 for panel,keys in enumerate(r['panels']):
  image=Image.new('RGBA',(320,180));draw=ImageDraw.Draw(image);draw.fontmode='1'
  for i,key in enumerate(keys[:85]):
   if not key['codepoint']:continue
   c=chr(key['codepoint']);box=r['rects'][i];x=box[0]+(box[2]-font.getlength(c))/2
   draw.text((int(x+.5),box[1]+ref['ascent']-font.getmetrics()[0]),c,font=font,fill='white'if key['value']else(122,108,134,255))
  path=f'new-game-preview/keyboard-{panel}.t3x';png=build/f'keyboard-{panel}.png';image.save(png);out=ROOT/'romfs'/path;out.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(out),str(png)],check=True)
  r['resources'].append(dict(path=path,width=320,height=180,columns=1,rows=1));outputs[path]=dict(sha256=sha(out),bytes=out.stat().st_size)
  for key,neighbors,box in zip(keys,graph(keys,r['groups'],r['columns'],r['rects']),r['rects']):key.update(neighbors=neighbors,rect=box)
 for asset in r['field_assets']:
  out=ROOT/'romfs'/asset['path'];subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(out),str(ex.upstream/asset['source'])],check=True)
  r['resources'].append({k:asset[k]for k in ['path','width','height','columns','rows']})
 resolve_bindings(ex,r)
 for asset in r['resources']:
  if asset['path']not in outputs:outputs[asset['path']]=dict(sha256=sha(ROOT/'romfs'/asset['path']),bytes=(ROOT/'romfs'/asset['path']).stat().st_size)
 r['outputs']=outputs;write_json(IR,r);PACK.write_bytes(encode(r));print('New Game pack',PACK.stat().st_size,'bytes; six naming fields and',len(r['bindings']),'source-keyed text bindings')

def encode(r):
 raw=bytearray();put=lambda f,*v:raw.extend(struct.pack('<'+f,*v))
 def text(s):b=s.encode('utf8');put('I',len(b));raw.extend(b)
 def texts(v):put('I',len(v));[text(s)for s in v]
 put('I',r['maximum']);text(r['target']);text(r['initial']);texts(r['other_initial']);texts(r['defaults']);texts(r['blacklist']);texts(r['texts']);texts(r['sounds']);put('ddf',r['cancel_delay'],r['error_duration'],r['font_height'])
 put('I',len(r['layouts']))
 for b in r['layouts']:put('4f',*b)
 put('4I5I',*r['patch'],*r['colors']);put('I',len(r['resources']))
 for a in r['resources']:text(a['path']);put('4I',*[a[k]for k in ['width','height','columns','rows']])
 put('I',len(r['panels']))
 for panel in r['panels']:
  put('I',len(panel))
  for k in panel:put('II',k['codepoint'],k['kind']);text(k['value']);put('4f4I',*k['rect'],*k['neighbors'])
 a=r['actor'];put('4fdI',*a['position'],*a['shadow'],a['length'],len(a['keys']))
 for t,f in a['keys']:put('dI',t,f)
 a=r['arrow'];put('2fddI',*a['offset'],a['length'],a['move'],len(a['keys']))
 for k in a['keys']:put('dI',k['time'],k['frame'])
 put('I',len(r['bindings']))
 for b in r['bindings']:put('2I',b['battle_id'],b['index']);text(b['key']);text(b['source_text']);text(b['expected']);texts(b['parts'])
 put('I',len(r['fields']))
 for f in r['fields']:
  put('III',f['kind'],f['maximum'],f['resource']);text(f['target']);text(f['initial']);text(f['prompt']);texts(f['defaults']);put('I',int(f['shadow']))
  a=f['actor'];put('4fdI',*a['position'],*a['shadow'],a['length'],len(a['keys']))
  for t,frame in a['keys']:put('dI',t,frame)
 return struct.pack('<8s4I',b'ENCNAMES',2,24+len(raw),zlib.crc32(raw),2)+raw

def verify(r):
 ex,current,_=base()
 for key in ['target','maximum','initial','other_initial','defaults','blacklist','texts','layouts','patch','colors','groups','columns','actor','arrow','cancel_delay','error_duration','sounds','license_review','fields','field_assets']:
  require(r[key]==current[key],'Changed source naming '+key)
 require(r['schema']==2 and r['commit']==ex.lock['commit'],'Name schema/pin')
 inventory=read_json(ROOT/'compatibility/upstream-inventory.json')
 for p,d in r['sources'].items():require(inventory['files'][p]['sha256']==d and sha(ex.upstream/p)==d,'Changed name source '+p)
 for p,d in r['dependencies'].items():require(sha(ROOT/p)==d,'Changed name dependency '+p)
 require(sha(REPORT/'native-layout.json')==r['native_layout_sha256'],'Changed native layout reference')
 require(r['rects']==read_json(REPORT/'native-layout.json')['keys'],'Changed native keyboard geometry')
 require(r['resources'][:4]==current['resources'],'Changed source naming resource')
 require(r['resources'][6:]==[{k:a[k]for k in ['path','width','height','columns','rows']}for a in current['field_assets']] and len(r['resources'])==11 and set(r['outputs'])=={a['path']for a in r['resources']},'Changed naming output inventory')
 require(len(r['panels'])==len(current['panels']),'Changed keyboard panel count')
 for panel,source in zip(r['panels'],current['panels']):
  require([{k:v for k,v in row.items()if k in ['codepoint','value','kind']}for row in panel]==source,'Changed source keyboard characters')
  require([k['rect']for k in panel]==r['rects'],'Changed keyboard layout')
  require([k['neighbors']for k in panel]==graph(panel,r['groups'],r['columns'],r['rects']),'Changed keyboard topology')
 check=dict(dependencies={});resolve_bindings(ex,check);require(check['bindings']==r['bindings'],'Changed battle name binding')
 for p,v in r['outputs'].items():require(sha(ROOT/'romfs'/p)==v['sha256'],'Changed naming texture '+p)

def stage_files(root):
 r=read_json(IR);verify(r);files={Path('data/opening.encnewgame'):(Path(root)/'data/opening.encnewgame').read_bytes()};require(files[Path('data/opening.encnewgame')]==encode(r),'Stale naming pack')
 for p,v in r['outputs'].items():
  path=Path(p);require(not path.is_absolute()and'..'not in path.parts,'Unsafe name resource');raw=(Path(root)/path).read_bytes();require(hashlib.sha256(raw).hexdigest()==v['sha256'],'Staged naming texture mismatch');files[path]=raw
 return files
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('action',choices=['assets','compile','verify']);ap.add_argument('--tex3ds');ap.add_argument('--godot');a=ap.parse_args()
 if a.action=='assets':extract_assets(Path(a.tex3ds),Path(a.godot))
 else:
  r=read_json(IR);verify(r);raw=encode(r)
  if a.action=='compile':PACK.write_bytes(raw)
  else:require(PACK.read_bytes()==raw,'Stale naming pack')
  print('Checked startup naming pack:',len(raw),'bytes')
