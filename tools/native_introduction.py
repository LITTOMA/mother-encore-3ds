#!/usr/bin/env python3
"""Pinned two-scene introduction adapter and checked binary compiler.

No generic scene/script VM: only reviewed image, border, text and cloud tracks.
The historical key at 116.5 remains in IR but is outside animation length 116.
"""
import argparse,hashlib,json,math,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,properties,node as source_node,one,require
IR=ROOT/'content/native-introduction.json'
PACK=ROOT/'romfs/data/opening.encintro'
RECEIPT=ROOT/'content/asset-receipts/graphics/cutscenes/introduction/source.json'
BINDINGS=ROOT/'content/introduction-bindings.json'
def node(text,path):
 # Godot serializes real multiline strings; normalize their JSON-compatible
 # representation before the shared bounded Variant parser sees the node.
 text=re.sub(r'"(?:\\.|[^"\\])*"',lambda m:json.dumps(json.loads(m[0],strict=False),ensure_ascii=False) if '\n' in m[0] else m[0],text)
 return source_node(text,path)

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def rgba(v):return sum(round(x*255)<<(24-i*8) for i,x in enumerate(v))
def safe(p):return isinstance(p,str) and p and ':' not in p and '\\' not in p and all(x not in ('','.','..') for x in p.split('/')) and not p.startswith('/')
def rect(n):return [n.get('margin_left',0),n.get('margin_top',0),n.get('margin_right',0)-n.get('margin_left',0),n.get('margin_bottom',0)-n.get('margin_top',0)]
def animation(text,name=None,ident=None):
 blocks=list(re.finditer(r'^\[sub_resource type="Animation" id=(\d+)\]\n(.*?)(?=^\[|\Z)',text,re.M|re.S))
 found=[properties(m[2]) for m in blocks if (ident is not None and int(m[1])==ident) or (name is not None and properties(m[2]).get('resource_name')==name)]
 require(len(found)==1,'Missing/ambiguous Introduction animation');p=found[0];tracks={}
 require(not p.get('loop',False),'Looping introduction animation rejected')
 for k,v in p.items():
  if k.startswith('tracks/'):
   _,i,field=k.split('/');tracks.setdefault(int(i),{})[field]=v
 require(set(tracks)==set(range(len(tracks))),'Noncontiguous intro track')
 for t in tracks.values():
  require(t['enabled'] and not t['imported'] and t['interp']==1 and t['type'] in ('value','method','animation'),'Unsupported intro track mechanism')
  keys=t['keys'];values=keys.get('values',keys.get('clips'));require(values is not None and len(values)==len(keys['times']),'Bad intro keys')
  require(all(math.isfinite(x) and x>=0 for x in keys['times']) and all(a<b for a,b in zip(keys['times'],keys['times'][1:])),'Unordered intro keys')
 return p.get('length',1.),list(tracks.values())
def track(t,target,index,prop):
 require(t['type']=='value' and t['keys']['update'] in (0,1),'Unsupported intro property track')
 k=t['keys'];require(len(k['times'])==len(k['values'])==len(k['transitions']),'Bad value keys')
 return dict(target=target,index=index,property=prop,discrete=k['update'],keys=[dict(time=time,value=(v if isinstance(v,list) else [float(v)])+[0.]*(4-(len(v) if isinstance(v,list) else 1)),transition=transition) for time,v,transition in zip(k['times'],k['values'],k['transitions'])])
def bindings():
 from tools.introduction_assets import read,validate as asset_validate
 b=read(BINDINGS)
 require(set(b)==set('schema kind commit asset_recipe source_refs scenes border_clips border_nodes cloud_clip cloud_node cloud_asset_id fade_kinds fade_clips fade_material_id fade_follow fade_rect doors effect_slots punctuation_key engine_defaults audio license_review'.split()),'Unknown/missing introduction binding fields')
 require(b['schema']==1 and b['kind']=='encore.introduction-bindings' and b['commit']==json.loads((ROOT/'upstream.lock').read_text())['commit'] and b['license_review'],'Unreviewed intro binding identity')
 require(safe(b['asset_recipe']) and len(b['scenes'])==2 and len(b['border_clips'])==5 and len(b['border_nodes'])==4 and len(b['doors'])==3 and set(b['effect_slots'].values())=={0,1},'Incomplete intro binding topology')
 a=read(ROOT/b['asset_recipe']);asset_validate(a)
 require(a['commit']==b['commit'],'Intro asset/binding pin differs')
 return b,a

def extract():
 b,a=bindings();ex=Extractor(ROOT);refs=b['source_refs']
 for p in refs.values():ex.data(p)
 for p in a['sources']:ex.data(p)
 texts=[ex.text(s['source']) for s in b['scenes']];scripts=[ex.text(refs[s['script_ref']]) for s in b['scenes']]
 border=ex.text(refs['border_scene']);cloud=ex.text(refs['cloud_scene']);fade=ex.text(refs['fade_scene'])
 receipt=json.loads(RECEIPT.read_text(encoding='utf8'));resources=[];resource_index={r['id']:i for i,r in enumerate(a['resources'])}
 require(len(receipt['resources'])==len(a['resources']),'Intro asset receipt incomplete')
 for binding,row in zip(a['resources'],receipt['resources']):
  require(row['path']==binding['output'] and row['source']==binding['source'],'Intro resource source mapping drift')
  resources.append({k:row[k] for k in ['path','source','width','height','columns','rows','frame_count','source_width','source_height','trim_x','trim_y','bytes','crc32','sha256']})
 from tools.introduction_assets import translations as source_translations
 translations=source_translations(ex.upstream,a);locales=[]
 for code in a['locales']:
  fonts=[next(f for f in a['fonts'] if f['locale']==code and f['role']==scene['role']) for scene in b['scenes']]
  hint_font=next(f for f in a['fonts'] if f['locale']==code and f['role']=='hint')
  definitions=fonts+[hint_font];keys=sum([f['text_keys'] for f in fonts],[])
  locales.append(dict(code=code,old_font=fonts[0]['source'],now_font=fonts[1]['source'],hint_font=hint_font['source'],character_spacing=[float(f['definition']['properties'].get('extra_spacing_char',0)) for f in definitions],punctuation=translations[b['punctuation_key']][code],skip=translations[hint_font['text_keys'][0]][code],texts=[translations[k][code] for k in keys]))
 scenes=[];text_sounds=[]
 for index,(binding,text,script) in enumerate(zip(b['scenes'],texts,scripts)):
  length,tracks=animation(text,binding['animation']);require(len(tracks)==binding['track_count'],'Introduction track scope changed')
  font=next(f for f in a['fonts'] if f['role']==binding['role']);source_keys=re.findall(r'"([^"]+)"',one(r'var text := (\[.*?\])',script,'caption source keys',re.S)[1]);require(source_keys==font['text_keys'],'Introduction script/font caption keys differ')
  images=[];indices=[];image_nodes={}
  for resource_id in binding['image_ids']:
   ri=resource_index[resource_id];asset=a['resources'][ri];require(asset['scene'] in (binding['source'],refs['cloud_scene']),'Cross-scene intro image binding');scene_node=binding['resource_nodes'].get(str(resource_id),asset['node']);n=node(text,scene_node);image_nodes[scene_node]=len(images);indices.append(ri);r=resources[ri]
   if r['frame_count']>1:
    root=node(cloud,'.');images.append(dict(path=r['path'],rect=[n['position'][0]-r['source_width']/2,n['position'][1]-r['source_height']/2,r['source_width'],r['source_height']],alpha=1,visible=n.get('visible',True),frame=root.get('frame',0),columns=r['columns']))
   else:images.append(dict(path=r['path'],rect=rect(n),alpha=n.get('modulate',[1,1,1,1])[3],visible=n.get('visible',True),frame=0,columns=r['columns']))
  events=[];values=[]
  for t in tracks:
   path=t['path'];k=t['keys']
   if t['type']=='method':
    require(path=='.','Unknown Intro method target')
    for time,v in zip(k['times'],k['values']):
     kinds={'next_text':0,'hide_text':1,'stop_music':2,'slow_down_text':3,'reset_text_speed':4,'play_sound':5};require(v['method'] in kinds,'Unsupported Intro method');args=v['args'];require((v['method']=='play_sound' and len(args)==2) or not args,'Unsupported Intro method args');require(not args or args[1] in b['effect_slots'],'Unbound named SFX')
     events.append(dict(time=time,kind=kinds[v['method']],arg=b['effect_slots'][args[1]] if args else 0,source=args[0] if args else '',name=args[1] if args else ''))
   elif t['type']=='animation':
    is_border=path==binding['nested_borders'];is_cloud=binding['nested_cloud'] is not None and path==binding['nested_cloud'];require(is_border or is_cloud,'Unknown Intro subanimation')
    for time,name in zip(k['times'],k['clips']):
     require(name in (b['border_clips'] if is_border else [b['cloud_clip']]),'Unknown nested clip')
     events.append(dict(time=time,kind=6 if is_border else 7,arg=b['border_clips'].index(name) if is_border else 0,source='',name=''))
   else:
    target,prop=path.rsplit(':',1)
    if target in image_nodes and prop in ('rect_position','visible','modulate'):values.append(track(t,0,image_nodes[target],{'rect_position':0,'visible':2,'modulate':1}[prop]))
    elif target==binding['text_clip'] and prop=='rect_position':values.append(track(t,2,0,0))
    else:raise ValueError('Unknown introduction property path '+path)
  number=lambda pattern:float(one(pattern,script,pattern)[1])
  speed=number(r'var text_speed := ([0-9.]+)');pause=node(text,binding['timer'])['wait_time'];delay=number(r'create_timer\(([0-9.]+)\)');hide=number(r'"rect_position:y", -?[0-9.]+, ([0-9.]+)')
  round_match=re.findall(r'for i in (\d+):',script);round_images=int(round_match[0]) if round_match else 0;require(len(round_match)<=1 and round_images<=len(images),'Unreviewed image pixel rounding')
  scenes.append(dict(delay=delay,length=length,speed=speed,slow=(number(r'func slow_down_text\(\):\s*text_speed = ([0-9.]+)') if index==0 else speed),pause=pause,hide=hide,hide_y=number(r'"rect_position:y", (-?[0-9.]+),'),pitch_min=(number(r'rand_range\(([0-9.]+),') if index else 1.),pitch_max=(number(r'rand_range\([0-9.]+,([0-9.]+)\)') if index else 1.),round_images=round_images,text_first=sum(x['text_count'] for x in scenes),text_count=len(source_keys),images=images,resources=indices,tracks=values,events=sorted(events,key=lambda e:e['time']),text_clip=rect(node(text,binding['text_clip']))))
  hint_scene=ex.text(refs[binding['hint_ref']]) if binding['hint_ref'] else text;default=b['engine_defaults']['label_font_color']
  backdrop=node(text,binding['background']);label=node(text,binding['text_label']);hint_label=node(hint_scene,binding['hint_label'])
  scenes[-1].update(background=dict(rect=rect(backdrop),color=rgba(backdrop['color'])),text_color=rgba(label.get('custom_colors/font_color',default)),hint_color=rgba(hint_label.get('custom_colors/font_color',default)),line_spacing=label['custom_constants/line_spacing'],hint_rect=rect(node(hint_scene,binding['hint_node'])))
  identity=node(text,binding['text_audio'])['stream']['ExtResource'];source=one(r'^\[ext_resource path="(res://[^"]+)" type="AudioStream" id='+str(identity)+r'\]',text,'text audio source')[1];text_sounds.append(source)
 borders=[]
 for name in b['border_clips']:
  length,tracks=animation(border,name);masks=[]
  for t in tracks:
   names=b['border_nodes'];target,prop=t['path'].rsplit(':',1);target=target.split('/')[-1];require(target in names and prop=='rect_position','Unknown border target');masks.append(track(t,1,names.index(target),0))
  borders.append(dict(length=length,tracks=masks))
 length,tracks=animation(cloud,b['cloud_clip']);require(len(tracks)==2 and tracks[0]['path']=='.:visible' and tracks[1]['path']=='.:frame','Cloud Form paths changed');cloud_index=b['scenes'][1]['image_ids'].index(b['cloud_asset_id']);cloud_clip=dict(length=length,tracks=[track(tracks[0],0,cloud_index,2),track(tracks[1],0,cloud_index,3)])
 fades=[];mostly=[]
 for binding in b['fade_clips']:
  length,tracks=animation(fade,binding['name']);require(len(tracks)==5 and tracks[0]['path']=='.:material:shader_param/cut' and tracks[1]['path']=='.:material:shader_param/fade' and tracks[4]['type']=='method','Unsupported fade topology');fades.append(dict(length=length,tracks=[track(tracks[0],3,0,0)]));mostly.append(tracks[4]['keys']['times'][0])
 door_script=ex.text(refs['door_script']);default_anim=json.loads(one(r'export .* var transit_in_anim := ("[^"]+")',door_script,'door default animation')[1]);default_in=float(one(r'export var fade_in_speed := ([0-9.]+)',door_script,'door fade in speed')[1]);default_out=float(one(r'export var fade_out_speed := ([0-9.]+)',door_script,'door fade out speed')[1]);offset=float(one(r'targetY - ([0-9.]+)',door_script,'door y offset')[1]);map_prefix=one(r'goto_scene\("([^"]+)" \+ targetScene',door_script,'target scene prefix')[1];map_suffix=one(r'targetScene \+ "([^"]+)"',door_script,'target scene suffix')[1]
 doors=[];kinds={k['name']:k['kind'] for k in b['fade_kinds']}
 for binding in b['doors']:
  text=ex.text(refs[binding['source_ref']]) if 'source_ref' in binding else texts[binding['scene']];n=node(text,binding['node']);in_anim=n.get('transit_in_anim',default_anim);out_anim=n.get('transit_out_anim',default_anim);require(in_anim in kinds and out_anim in kinds,'Unknown transition kind')
  doors.append(dict(destination=dict(scene=map_prefix+n['targetScene']+map_suffix,x=n['targetX'],y=n['targetY']-offset,dx=n['dir'][0],dy=n['dir'][1],set_respawn=n.get('set_respawn',False),unpause=n.get('unpause_player',True)),in_kind=kinds[in_anim],out_kind=kinds[out_anim],in_speed=n.get('fade_in_speed',default_in),out_speed=n.get('fade_out_speed',default_out)))
 naming=ex.text(refs['naming_script'])
 # The source audio manager constructs music paths from the serialized bgm.
 manager=ex.text(refs['audio_manager']);prefixes=re.findall(r'"(res://[^"\n]+/)"\s*\+\s*(?:intro|loop)',manager);require(prefixes and len(set(prefixes))==1,'Unreviewed music source root');music=prefixes[0]+ex.yaml(refs['naming_sequence'])['bgm']
 audio_sources=[music]+text_sounds+[e['source'] for s in scenes for e in s['events'] if e['kind']==5]
 require(set(audio_sources)=={r['source_path'] for r in b['audio'].values()},'Introduction audio role/source binding changed')
 for row in b['audio'].values():
  require(hashlib.sha256(ex.data(row['source_path'][6:])).hexdigest()==row['source_sha256'] and hashlib.sha256(ex.data(row['source_path'][6:]+'.import')).hexdigest()==row['import_sha256'],'Introduction audio source/import changed')
 hint=ex.text(refs['skip_script']);require('set_trans(Tween.TRANS_QUART)' in hint and '.set_ease(Tween.EASE_OUT)' in hint and '.set_ease(Tween.EASE_IN_OUT)' in hint,'Unsupported hint tween mechanism');hint_curve=[1./4,-4.];material=properties(one(r'^\[sub_resource type="ShaderMaterial" id='+str(b['fade_material_id'])+r'\]\n(.*?)(?=^\[|\Z)',fade,'fade material',re.M|re.S)[1]);follow=node(fade,b['fade_follow'])['position'];box=rect(node(fade,b['fade_rect']))
 from tools.blackbars_assets import extract as blackbar_extract
 bb=blackbar_extract();ex.sources.update(bb['sources']);project=ex.text(refs['project']);clear=[float(v) for v in one(r'^environment/default_clear_color=Color\( ([^)]*)\)',project,'background clear')[1].split(',')];canvas=[int(one(r'^window/size/'+key+r'=(\d+)$',project,'canvas '+key)[1]) for key in ('width','height')]
 finish_name=one(r'func finish_intro\(\):\s*stop_sound\("([^"]+)"\)',scripts[1],'finish named SFX')[1];require(finish_name in b['effect_slots'],'Unbound finish SFX')
 return dict(schema=1,commit=ex.lock['commit'],sources=ex.sources,bindings_sha256=sha(BINDINGS),asset_recipe_sha256=sha(ROOT/b['asset_recipe']),source_map=b,asset_receipt_sha256=sha(RECEIPT),font_catalog=a['font_catalog'],canvas=canvas,music=music,music_gain=float(one(r'set_audio_player_volume\(_music_id, ([0-9.]+)\)',naming,'Naming music gain')[1]),music_fade=float(one(r'fadeout_all_music\(([0-9.]+)\)',scripts[0],'Intro music fade')[1]),finish_stop_slot=b['effect_slots'][finish_name],hint_curve=hint_curve,hint_timing=[float(v) for v in re.findall(r'(?:Color.white, |Color.transparent, |set_delay\()([0-9.]+)',hint)],fade_shader=[material['shader_param/Size'],material['shader_param/screenWidth'],material['shader_param/screenHeight'],box[2],box[3],*follow],background_color=rgba(clear),blackbars=bb,resources=resources,locales=locales,scenes=scenes,borders=borders,border_rects=[dict(rect=rect(node(border,n)),color=rgba(node(border,n)['color'])) for n in b['border_nodes']],cloud=cloud_clip,fades=fades,fade_mostly=mostly,doors=doors,text_sounds=text_sounds,scope='Original two-scene intro source adapters; no generic Godot. Source dependencies and role bindings reviewed at fixed pin. English and Simplified Chinese source font roles; 1:1 native viewport composition.')
def validate(d):
 def fields(value,keys):require(type(value)is dict and set(value)==set(keys.split()),'Unknown/missing Introduction fields')
 fields(d,'schema commit sources bindings_sha256 asset_recipe_sha256 source_map asset_receipt_sha256 font_catalog canvas music music_gain music_fade finish_stop_slot hint_timing hint_curve fade_shader background_color blackbars resources locales scenes borders border_rects cloud fades fade_mostly doors text_sounds scope')
 require(d.get('schema')==1 and d.get('commit')==json.loads((ROOT/'upstream.lock').read_text(encoding='utf8'))['commit'],'Intro source schema/pin rejected')
 require(type(d['finish_stop_slot'])is int and d['finish_stop_slot'] in (0,1),'Invalid finish SFX slot')
 require(len(d['hint_curve'])==2 and all(math.isfinite(x) and x!=0 and abs(x)<=100 for x in d['hint_curve']) and len(d['hint_timing'])==3 and len(d['fade_shader'])==7 and len(d['fade_mostly'])==6 and len(d['text_sounds'])==2,'Intro aggregate shape rejected')
 require(all(isinstance(x,(int,float)) and math.isfinite(x) and x>0 for x in [d['music_fade'],*d['hint_timing']]) and all(math.isfinite(x) for x in d['fade_shader']),'Intro global timing rejected')
 require(all(re.fullmatch('[0-9a-f]{64}',d[k]) for k in ['bindings_sha256','asset_recipe_sha256','asset_receipt_sha256']),'Invalid source fingerprint')
 require(0<len(d['resources'])<=64 and len(d['scenes'])==2 and len(d['borders'])==5 and len(d['doors'])==3 and len(d['fades'])==6,'Intro schema counts rejected')
 require(len(d['canvas'])==2 and all(math.isfinite(x) and 0<x<=1024 for x in d['canvas']) and safe(d['font_catalog']),'Invalid Intro canvas/font path')
 require(0<len(d['locales'])<=32 and len({r['code'] for r in d['locales']})==len(d['locales']),'Unknown Intro locale')
 for r in d['resources']:
  fields(r,'path source width height columns rows frame_count source_width source_height trim_x trim_y bytes crc32 sha256')
  require(all(type(r[k])is int and 0<=r[k]<2**32 for k in ['width','height','columns','rows','frame_count','source_width','source_height','trim_x','trim_y','bytes','crc32']),'Intro integer domain')
  require(safe(r['path']) and r['path'].endswith('.t3x') and r['bytes']>0 and re.fullmatch('[0-9a-f]{64}',r['sha256']) and 0<r['width']<=1024 and 0<r['height']<=1024 and 0<r['columns']<=32 and 0<r['rows']<=32 and 0<r['frame_count']<=r['rows']*r['columns'] and r['width']%r['columns']==0 and r['height']%r['rows']==0 and r['width']//r['columns']+r['trim_x']<=r['source_width'] and r['height']//r['rows']+r['trim_y']<=r['source_height'],'Invalid Intro resource')
 require(len({r['path'] for r in d['resources']})==len(d['resources']),'Duplicate Intro resource')
 for l in d['locales']:
  fields(l,'code old_font now_font hint_font character_spacing punctuation skip texts');require(len(l['texts'])==sum(s['text_count'] for s in d['scenes']) and all(type(t)is str and t for t in l['texts']) and l['punctuation'] and l['skip'] and all(safe(l[k]) for k in ['old_font','now_font','hint_font']) and len(l['character_spacing'])==3 and all(math.isfinite(x) and abs(x)<=100 for x in l['character_spacing']),'Incomplete Intro text locale')
 for s in d['scenes']:
  fields(s,'delay length speed slow pause hide hide_y pitch_min pitch_max round_images text_first text_count images resources tracks events text_clip background text_color hint_color line_spacing hint_rect')
  require(0<s['length']<=180 and 0<s['delay']<10 and 0<s['speed']<1 and 0<s['pause']<10 and 0<s['hide']<10 and 0<s['pitch_min']<=s['pitch_max']<=2,'Invalid Intro timing')
  require(all(math.isfinite(s[k]) for k in ['delay','length','speed','slow','pause','hide','hide_y','pitch_min','pitch_max','line_spacing']),'Nonfinite Intro timing')
  require(type(s['round_images'])is int and 0<=s['round_images']<=len(s['images']) and type(s['text_first'])is int and type(s['text_count'])is int and s['text_count']>0 and s['text_first']>=0 and s['text_first']+s['text_count']<=len(d['locales'][0]['texts']),'Invalid Intro text/rounding reference')
  require(len(s['images'])==len(s['resources']) and all(0<=i<len(d['resources']) for i in s['resources']),'Invalid Intro resource reference')
  for image,index in zip(s['images'],s['resources']):
   fields(image,'path rect alpha visible frame columns');require(image['path']==d['resources'][index]['path'] and 0<=image['frame']<d['resources'][index]['frame_count'] and 0<=image['alpha']<=1 and type(image['visible'])is bool,'Invalid Intro image')
  for e in s['events']:
   fields(e,'time kind arg source name');require(math.isfinite(e['time']) and 0<=e['time']<=s['length']+1 and e['kind'] in range(8) and type(e['arg'])is int and 0<=e['arg']<5,'Unknown Intro event')
   require(e['kind']!=5 or (e['arg']<2 and e['source'].startswith('res://') and safe(e['source'][6:]) and e['name']),'Invalid Intro sound event')
  require(all(a['time']<=b['time'] for a,b in zip(s['events'],s['events'][1:])),'Unordered Intro events')
  require(sum(e['kind']==0 for e in s['events'])==s['text_count'],'Incomplete Intro caption timeline')
 for clip in d['scenes']+d['borders']+[d['cloud']]+d['fades']:
  for t in clip['tracks']:
   fields(t,'target index property discrete keys')
   require(t['target'] in range(4) and t['property'] in range(4) and t['discrete'] in (0,1) and t['keys'],'Unknown Intro track enum')
   for k in t['keys']:fields(k,'time value transition');require(len(k['value'])==4,'Intro key vector domain')
   require(all(math.isfinite(k['time']) and k['time']>=0 and all(math.isfinite(x) for x in k['value']) and math.isfinite(k['transition']) for k in t['keys']),'Nonfinite Intro key')
   require(all(a['time']<b['time'] for a,b in zip(t['keys'],t['keys'][1:])),'Unordered Intro keys')
 for door in d['doors']:
  fields(door,'destination in_kind out_kind in_speed out_speed');fields(door['destination'],'scene x y dx dy set_respawn unpause');require(door['in_kind'] in range(3) and door['out_kind'] in range(3) and door['in_speed']>0 and door['out_speed']>0,'Invalid Intro door')
def verify(d):
 validate(d);ex=Extractor(ROOT)
 for path,digest in d['sources'].items():require(hashlib.sha256(ex.data(path)).hexdigest()==digest,'Changed Intro source '+path)
 require(sha(BINDINGS)==d['bindings_sha256'],'Intro source bindings changed');require(sha(ROOT/d['source_map']['asset_recipe'])==d['asset_recipe_sha256'],'Intro source asset recipe changed');require(sha(RECEIPT)==d['asset_receipt_sha256'],'Intro asset receipt changed')
 for r in d['resources']:
  raw=(ROOT/'romfs'/r['path']).read_bytes();require(len(raw)==r['bytes'] and hashlib.sha256(raw).hexdigest()==r['sha256'] and zlib.crc32(raw)==r['crc32'],'Stale intro texture '+r['path'])
 require(extract()==d,'Stale intro semantics or source/asset binding')
def encode(d):
 validate(d);out=bytearray()
 def u(*v):out.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):out.extend(struct.pack('<'+'f'*len(v),*v))
 def number(*v):out.extend(struct.pack('<'+'d'*len(v),*v))
 def text(v):raw=v.encode('utf8');u(len(raw));out.extend(raw)
 def tracks(v):
  u(len(v))
  for t in v:
   u(t['target'],t['index'],t['property'],t['discrete'],len(t['keys']))
   for k in t['keys']:f(k['time'],*k['value'],k['transition'])
 def clips(v):
  u(len(v))
  for c in v:f(c['length']);tracks(c['tracks'])
 f(*d['canvas']);text(d['font_catalog']);text(d['music']);f(d['music_gain'],d['music_fade'],*d['hint_timing'])
 f(*d['hint_curve']);f(*d['fade_shader']);u(d['background_color'])
 from tools.blackbars_assets import encode as blackbar_encode
 bb=blackbar_encode(d['blackbars']);u(len(bb));out.extend(bb);u(d['finish_stop_slot'])
 u(len(d['resources']))
 for r in d['resources']:text(r['path']);text(r['source']);text(r['sha256']);u(*(r[k] for k in ['width','height','columns','rows','frame_count','source_width','source_height','trim_x','trim_y','bytes','crc32']))
 u(len(d['locales']))
 for l in d['locales']:
  for k in ['code','old_font','now_font','hint_font','punctuation','skip']:text(l[k])
  f(*l['character_spacing'])
  u(len(l['texts']))
  for t in l['texts']:text(t)
 u(len(d['scenes']))
 for s in d['scenes']:
  number(*(s[k] for k in ['delay','length','speed','slow','pause','hide','hide_y','pitch_min','pitch_max']));u(s['text_first'],s['text_count'],s['round_images']);f(*s['text_clip']);u(len(s['images']))
  f(*s['background']['rect']);u(s['background']['color'],s['text_color'],s['hint_color']);f(s['line_spacing'],*s['hint_rect'])
  for i,r in zip(s['images'],s['resources']):u(r);f(*i['rect'],i['alpha']);u(int(i['visible']),i['frame'])
  tracks(s['tracks']);u(len(s['events']))
  for e in s['events']:f(e['time']);u(e['kind'],e['arg']);text(e['source']);text(e['name'])
 clips(d['borders']);u(len(d['border_rects']))
 for m in d['border_rects']:f(*m['rect']);u(m['color'])
 clips([d['cloud']]);clips(d['fades']);u(len(d['fade_mostly']));f(*d['fade_mostly']);u(len(d['doors']))
 for door in d['doors']:
  dest=door['destination'];text(dest['scene']);f(*(dest[k] for k in ['x','y','dx','dy']));u(int(dest['set_respawn']),int(dest['unpause']),door['in_kind'],door['out_kind']);f(door['in_speed'],door['out_speed'])
 for s in d['text_sounds']:text(s)
 return struct.pack('<8s4I',b'ENCINTRO',1,len(out)+24,zlib.crc32(out),1)+out
def stage_files(root):
 d=json.loads(IR.read_text(encoding='utf8'));verify(d);raw=(Path(root)/'data/opening.encintro').read_bytes();require(raw==encode(d),'Stale introduction pack');files={Path('data/opening.encintro'):raw}
 for r in d['resources']:
  raw=(Path(root)/r['path']).read_bytes();require(len(raw)==r['bytes'] and sha(Path(root)/r['path'])==r['sha256'],'Staged introduction texture changed');files[Path(r['path'])]=raw
 return files
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args()
 if a.action=='extract':IR.parent.mkdir(parents=True,exist_ok=True);IR.write_text(json.dumps(extract(),ensure_ascii=False,indent=2)+'\n',encoding='utf8');return
 d=json.loads(IR.read_text(encoding='utf8'));verify(d);raw=encode(d)
 if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
 else:require(PACK.read_bytes()==raw,'Stale introduction binary')
 print('Original two-scene Introduction: %d checked bytes'%len(raw))
if __name__=='__main__':main()
