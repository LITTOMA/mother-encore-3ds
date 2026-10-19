"""Compile reviewed music-manager call programs, separate from execution code."""
from pathlib import Path
import argparse,hashlib,json,math,re,struct,sys,zlib
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools.reference_movement import extract
RECIPE=ROOT/'content/music-programs.json'
OPS={n:i+1 for i,n in enumerate('add play volume fade_in fade_to fade_out fade_all pause resume remove stop capture_position if_exists if_playing if_paused if_equal if_different if_boolean stop_areas'.split())}
TARGETS={n:i for i,n in enumerate('none child latest song contains'.split())}
KINDS={n:i for i,n in enumerate('empty string number boolean argument gain register'.split())}
LIMIT=128*1024
def val(key=None,value=None):return {}if key is None else {key:value}
def target(kind='none',value=None):return dict(kind=kind,**({}if kind in ('none','latest')else {'value':value}))
def op(name,to=None,args=(),skip=0):return dict(op=name,target=target()if to is None else to,args=list(args),skip=skip)
def condition(name,to,args,yes,no=None):
 no=[]if no is None else no
 if no:return [op(name,to,args,len(yes)+1),*yes,op('if_boolean',args=[val('boolean',False)],skip=len(no)),*no]
 require(bool(yes),'Empty music conditional body');return [op(name,to,args,len(yes)),*yes]
def original():
 ex=Extractor(ROOT);paths={'manager':'Scripts/global/audioManager.gd','title':'Scripts/UI/Title screen.gd','naming':'Scripts/UI/NamingScreen/Naming screen.gd','dialogue':'Scripts/UI/DialogueBox.gd','battle':'Scripts/UI/Battle/BattleSystem.gd'};texts={k:ex.text(p).replace('\r\n','\n')for k,p in paths.items()};methods={}
 names={'manager':['play_music','fadeout_all_music','pause_all_music','resume_all_music','music_fadeto','music_fadeout','music_fadein','set_audio_player_volume','add_audio_player','get_audio_player_from_song','is_playing','stop_audio_player','_stop_audio_player_obj'],'title':['_ready','_physics_process'],'naming':['_ready'],'battle':['_ready','_battle_start','_win','_level_up_or_learn_skills','_end_battle_to_overworld','_remove_battle_music']}
 for key,selected in names.items():
  raw=ex.data(paths[key]);_,hashes=extract(raw,hashlib.sha256(raw).hexdigest(),selected);methods[paths[key]]=hashes
 ex.data('Nodes/Overworld/MusicChanger.tscn')
 manager,title,naming,dialogue,battle=[texts[k]for k in ('manager','title','naming','dialogue','battle')]
 prefix=re.findall(r'"(res://[^"\n]+/)"\s*\+\s*(?:loop|track)',manager);require(prefix and len(set(prefix))==1,'Music URI root changed');prefix=prefix[0]
 ready=re.search(r'if audioManager.get_audio_player\(audioManager.get_latest_audio_player_index\(\)\).stream != load\("([^"]+)"\):\s+audioManager.fadeout_all_music\(([0-9.]+)\)\s+audioManager.add_audio_player\(\)\s+audioManager.play_music_on_latest_player\("", "([^"]+)"\)',title);require(ready and ready[1]==prefix+ready[3],'Title music block changed')
 new=re.search(r'MenuOptions.NEW_GAME:\s+audioManager.fadeout_all_music\(([0-9.]+)\)',title);volume=re.search(r'set_audio_player_volume\(_music_id, ([0-9.]+)\)',naming);default=re.search(r'func music_fadeto\(index, volume = 0, duration = ([0-9.]+)\):',manager);fade=re.search(r'else:\s+audioManager.music_fadeout\(0, ([0-9.]+)\)',dialogue);back=re.search(r'audioManager.music_fadein\(0, audioManager.get_audio_player\(0\).volume_db, ([0-9.]+)\)',battle);wins=re.search(r'var win_themes_path := "([^"]+)"',battle)
 require(all([new,volume,default,fade,back,wins]),'Music source timing/selector changed')
 require('if _embed_mode:\n\t\t\taudioManager.pause_all_music()'in naming and 'audioManager.play_music("", naming_sequence.bgm, _music_id)'in naming,'Naming music/pause scope changed')
 require('yield(get_tree(), "idle_frame")\n\t\taudioManager.music_fadeto(0, _curr_phrase["musicvolume"])'in dialogue,'Dialogue music volume deferred boundary changed')
 require('if audioManager.get_audio_player(0).playing:\n\t\t\taudioManager.add_audio_player()'in dialogue,'Dialogue loop owner selection changed')
 effects=re.search(r'var _musical_effects = (\{.*?\n\})',battle,re.S);require(effects,'Battle music effects changed');effects=json.loads(effects[1]);require(all(type(v)is str for v in effects.values()),'Battle music effect types')
 require('if !audioManager.is_playing(_musical_effects["lvlup"]):'in battle and 'get_audio_player_from_song(_musical_effects["lvlup"]).get_playback_position()'in battle and 'if !audioManager.is_playing(party_mem_level_up):'in battle and 'play_music_on_latest_player("", party_mem_level_up, start_time)'in battle,'Level-up music ordering changed')
 require('if _music and !audioManager.overworldBattleMusic: audioManager.pause_all_music()'in battle and 'if !audioManager.overworldBattleMusic or _is_boss:'in battle,'Battle music predicates changed')
 N=lambda n:val('number',n);S=lambda s:val('string',s);I=lambda i:val('argument',i);E=val();latest=target('latest');first=target('child',N(0));song=lambda s:target('song',s);play=lambda a,b,start=N(0):op('play',latest,[a,b,start]);add=op('add');pause=op('pause');resume=op('resume')
 programs=[]
 def program(identity,name,inputs,steps,delay=0):programs.append(dict(id=identity,name=name,inputs=inputs,delay_idle_frames=delay,instructions=steps))
 program(1,'title_ready',[],condition('if_different',latest,[S(ready[1])],[op('fade_all',args=[N(float(ready[2]))]),add,play(E,S(ready[1]))]))
 program(2,'title_new_game',[],[op('fade_all',args=[N(float(new[1]))])])
 program(3,'naming_ready',['bgm:string','embedded:boolean'],condition('if_boolean',None,[I(1)],[pause])+[add,play(E,I(0)),op('volume',latest,[N(float(volume[1]))])])
 program(4,'fade_all',['seconds:number'],[op('fade_all',args=[I(0)])])
 program(5,'dialogue_loop',['track:string','loop:string'],condition('if_playing',first,[],[add])+[play(I(0),I(1))])
 program(6,'dialogue_one_shot',['track:string'],condition('if_playing',first,[],[add])+[play(I(0),E)])
 program(7,'dialogue_fade',[],[op('fade_out',first,[N(float(fade[1]))])])
 program(8,'dialogue_volume',['volume:number'],[op('fade_to',first,[I(0),N(float(default[1]))])],1)
 program(9,'encounter',['sound:string','pause:boolean'],condition('if_boolean',None,[I(1)],[pause])+[add,play(I(0),E)])
 program(10,'battle_music',['intro:string','loop:string','enabled:boolean'],condition('if_boolean',None,[I(2)],[add,play(I(0),I(1))]))
 victory=condition('if_boolean',None,[I(1)],[play(S(prefix+effects['youwonboss']),S(prefix+effects['victory']))],[play(S(prefix+effects['youwon']),S(prefix+effects['victory']))])
 program(11,'victory',['enabled:boolean','boss:boolean'],condition('if_boolean',None,[I(0)],[pause,add,*victory]))
 base=S(prefix+effects['lvlup']);missing=condition('if_exists',song(base),[],[],[pause,add,play(E,base)])
 layer=condition('if_exists',song(I(1)),[],[],[add,play(E,I(1),val('register',0))])
 program(12,'level_up',['enabled:boolean','personal:string'],condition('if_boolean',None,[I(0)],[*missing,op('capture_position',song(base),[N(0)]),*layer]))
 stop=condition('if_boolean',None,[I(3)],[op('stop_areas',args=[N(0)])]);restore=condition('if_exists',first,[],condition('if_paused',first,[],[op('fade_in',first,[val('gain',True),N(float(back[1]))])]))
 remove=[op('remove',target('contains',S(wins[1]))),*stop,op('remove',song(I(1))),op('remove',song(I(2))),*restore]
 program(13,'battle_return',['result_music:boolean','battle:string','intro:string','stop_areas:boolean'],condition('if_boolean',None,[I(0)],remove)+[resume])
 program(14,'stop_song',['song:string'],[op('stop',song(I(0)))])
 bindings=[];inputs=[('content/native-round.json','Data/Battlers/lamp.yaml'),('content/doll-round.json','Data/Battlers/doll.yaml'),('content/pillow-round.json','Data/Battlers/pillow.yaml'),('content/bridge-round.json','Data/Battlers/straydog.yaml')]
 for recipe,path in inputs:
  ir=json.loads((ROOT/recipe).read_bytes());data=ex.yaml(path);require(type(data)is dict and type(data.get('music'))is str and type(data.get('musicintro',''))is str,'Unreviewed enemy music inheritance/type');require(ir['commit']==PIN,'Encounter music rule source changed');bindings.append(dict(battle=ir['binding']['battle_id'],source=path,round_recipe=recipe,music=prefix+'Battle Themes/'+data['music']if data['music']else'',intro=prefix+'Battle Themes/'+data.get('musicintro','')if data.get('musicintro','')else''))
 result=dict(schema=1,kind='encore.music-programs.candidate',source_commit=PIN,sources=ex.sources,programs=programs,encounters=bindings)
 proof=dict(source_commit=PIN,release_staged=False,manager_and_callsite_methods=methods,sources=ex.sources,source_recipe=result,mapped_implementation={'binary_programs':'runtime/music_programs.cpp','manager_operations':'runtime/music_players.cpp','registered_area_callbacks':'runtime/music_scene_graph.cpp','position_query':'platform/ctr/music_bank_transports.cpp'},scope='Current title/naming/intro fade, dialogue music and four reviewed encounters; paths, gains, durations, conditions, URI selectors and one-idle volume delay remain data. Narrow forward-only manager operations, not a Godot interpreter. Parameters originate in checked callers. Independent metadata only; not catalog-bound, staged or connected to main yet.')
 return result,proof
def encode(recipe):
 require(type(recipe)is dict and set(recipe)=={'schema','kind','source_commit','sources','programs','encounters'}and type(recipe['schema'])is int and recipe['schema']==1 and recipe['kind']=='encore.music-programs.candidate'and recipe['source_commit']==PIN,'Music program recipe identity/fields')
 require(type(recipe['sources'])is dict and recipe['sources']and all(type(k)is str and type(v)is str and re.fullmatch('[0-9a-f]{64}',v)for k,v in recipe['sources'].items()),'Music program source fingerprints')
 strings=[];programs=recipe['programs'];encounters=recipe['encounters'];require(type(programs)is list and 1<=len(programs)<=64 and type(encounters)is list and len(encounters)<=64,'Music program collection limits');blob=bytearray();ids=set();total=0
 def string(s):
  require(type(s)is str and len(s)<=1024 and all(32<=ord(c)<=126 and c!='\\'for c in s),'Music program string')
  if not s:return 0
  if s not in strings:strings.append(s)
  require(len(strings)<=256,'Music program string count');return strings.index(s)+1
 def value(v):
  require(type(v)is dict and len(v)<=1,'Music operand fields')
  if not v:return struct.pack('<2Id',0,0,0)
  key=next(iter(v));item=v[key];require(key in KINDS and key!='empty','Music operand kind');reference=0;number=0
  if key=='string':reference=string(item)
  elif key in ('argument','register'):require(type(item)is int and 0<=item<8,'Music operand index');reference=item
  elif key=='boolean':require(type(item)is bool,'Music boolean value');number=int(item)
  elif key=='gain':require(item is True,'Music current gain operand')
  else:require(type(item)in(int,float)and math.isfinite(item),'Music numeric operand');number=item
  return struct.pack('<2Id',KINDS[key],reference,number)
 for p in programs:
  require(type(p)is dict and set(p)=={'id','name','inputs','delay_idle_frames','instructions'}and type(p['id'])is int and 0<p['id']<2**32 and p['id']not in ids and type(p['name'])is str and re.fullmatch('[a-z][a-z0-9_]*',p['name']),'Music program identity');ids.add(p['id'])
  require(type(p['inputs'])is list and len(p['inputs'])<=8 and all(type(v)is str and re.fullmatch('[a-z_]+:(string|number|boolean)',v)for v in p['inputs'])and type(p['delay_idle_frames'])is int and 0<=p['delay_idle_frames']<=1,'Music program inputs/delay');steps=p['instructions'];require(type(steps)is list and 1<=len(steps)<=256,'Music instruction count');total+=len(steps);require(total<=1024,'Music total instruction limit');blob.extend(struct.pack('<3I',p['id'],p['delay_idle_frames'],len(steps)))
  for index,i in enumerate(steps):
   require(type(i)is dict and set(i)=={'op','target','args','skip'}and i['op']in OPS and type(i['skip'])is int and 0<=i['skip']<=len(steps)-index-1,'Music instruction fields/opcode/jump');require((i['op'].startswith('if_')and i['skip']>0)or(not i['op'].startswith('if_')and i['skip']==0),'Music branch scope');to=i['target'];require(type(to)is dict and to.get('kind')in TARGETS and set(to)==({'kind'}if to['kind']in ('none','latest')else {'kind','value'}),'Music target fields');args=i['args'];require(type(args)is list and len(args)<=3,'Music argument count');blob.extend(struct.pack('<4I',OPS[i['op']],i['skip'],TARGETS[to['kind']],len(args)));blob.extend(value(to.get('value',{})))
   def kind(v):
    if not v:return 'empty'
    k=next(iter(v))
    if k=='argument':
     require(v[k]<len(p['inputs']),'Music argument lacks declaration');return p['inputs'][v[k]].split(':')[1]
    return k
   def numeric(v,lo,hi,gain=False):
    k=kind(v);require(k in (('number','register','gain')if gain else ('number','register')),'Music numeric operand type')
    if 'number'in v:require(lo<=v['number']<=hi,'Music numeric operand range')
   def text(v,uri=False):
    k=kind(v);require(k in ('empty','string'),'Music URI operand type')
    if uri and 'string'in v and v['string']:
     v=v['string'];require(v.startswith('res://')and all(part not in ('','.','..')for part in v[6:].split('/'))and ':'not in v[6:],'Music literal stream URI')
   name=i['op'];mode=to['kind'];single=mode in ('child','latest','song');none=mode=='none'
   if mode=='child':
    require(kind(to['value'])=='number','Music index operand type')
    if 'number'in to['value']:require(0<=to['value']['number']<256 and int(to['value']['number'])==to['value']['number'],'Music index range')
   elif mode in ('song','contains'):
    text(to['value'])
    if mode=='contains':require(to['value']and to['value'].get('string',True)!='','Empty music substring')
   if name in ('add','pause','resume'):require(none and not args,'Music no-target command signature')
   elif name in ('fade_all','stop_areas'):require(none and len(args)==1,'Music global fade signature');numeric(args[0],0,60)
   elif name=='play':
    require(single and len(args)==3,'Music play signature');text(args[0],True);text(args[1],True);numeric(args[2],0,3.4028234663852886e38)
    require(not((not args[0]or args[0].get('string')=='')and(not args[1]or args[1].get('string')=='')),'Empty music play source')
   elif name in ('volume','fade_out'):
    require(single and len(args)==1,'Music scalar command signature');numeric(args[0],-120 if name=='volume'else 0,24 if name=='volume'else 60,name=='volume')
   elif name in ('fade_in','fade_to'):require(single and len(args)==2,'Music fade signature');numeric(args[0],-120,24,True);numeric(args[1],0,60)
   elif name in ('remove','stop'):require((single or(name=='remove'and mode=='contains'))and not args,'Music removal signature')
   elif name=='capture_position':require(single and len(args)==1 and set(args[0])=={'number'}and 0<=args[0]['number']<8 and int(args[0]['number'])==args[0]['number'],'Music capture register signature')
   elif name in ('if_exists','if_playing','if_paused'):require(single and not args,'Music guard signature')
   elif name in ('if_equal','if_different'):require(single and len(args)==1,'Music stream guard signature');text(args[0])
   elif name=='if_boolean':require(none and len(args)==1 and kind(args[0])=='boolean','Music boolean guard signature')
   for a in args:blob.extend(value(a))
 seen=set()
 for b in encounters:
  require(type(b)is dict and set(b)=={'battle','source','round_recipe','music','intro'}and type(b['battle'])is int and 0<b['battle']<2**32 and b['battle']not in seen and type(b['source'])is str and type(b['round_recipe'])is str and(bool(b['music'])or not b['intro']),'Music encounter binding');seen.add(b['battle']);blob.extend(struct.pack('<3I',b['battle'],string(b['music']),string(b['intro'])))
 text=bytearray()
 for s in strings:raw=s.encode('ascii');text.extend(struct.pack('<I',len(raw))+raw)
 data=bytearray(struct.pack('<8s8I',b'ENCMOP01',1,40+len(text)+len(blob),0,1,len(strings),len(programs),len(encounters),0)+text+blob);require(len(data)<=LIMIT,'Music program byte limit');struct.pack_into('<I',data,16,zlib.crc32(data));return bytes(data)
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--out',required=True,type=Path);a=ap.parse_args();out=a.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build','Music program candidate stays under build');derived,proof=original();require(derived==json.loads(RECIPE.read_bytes()),'Music program IR differs from reviewed source mapping');require(proof==json.loads((ROOT/'compatibility/reviews/music-programs-v0410.json').read_bytes()),'Music program semantic review changed');data=encode(derived);out.mkdir(parents=True,exist_ok=True);(out/'global.encmop').write_bytes(data);(out/'source.json').write_bytes((json.dumps(dict(source=proof,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),output_sha256=hashlib.sha256(data).hexdigest(),bytes=len(data)),indent=2)+'\n').encode());print('Music program candidate:',len(derived['programs']),'programs;',len(derived['encounters']),'source encounter bindings;',len(data),'bytes; not staged')
if __name__=='__main__':main()
