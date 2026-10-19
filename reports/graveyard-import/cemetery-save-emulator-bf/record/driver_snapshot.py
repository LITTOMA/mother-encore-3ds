#!/usr/bin/env python3
"""Normal-input bridge regression; read-only observation, owned fixed-clock profile.
The checkpoint is artificial and does not claim the opening was played.
"""
import argparse,base64,json,math,re,secrets,shutil,socket,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT))
from tools import run_emulator_semantics as r
from tools.native_content import parse_pack
from tools.emulator_artifact import checked_resources
import hashlib
p=argparse.ArgumentParser();p.add_argument('--package',type=Path,required=True);p.add_argument('--fixture',type=Path,required=True);p.add_argument('--emulator',type=Path,required=True);p.add_argument('--dsp-component',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--clock',type=int,default=946681277);p.add_argument('--mom-help',action='store_true');p.add_argument('--house-roundtrip',action='store_true');p.add_argument('--locale-preference',type=Path);p.add_argument('--mayor-route',type=Path);p.add_argument('--wsl-walk-planner',type=Path);p.add_argument('--walk-planner-source',type=Path,default=ROOT/'tools/field_walk_plan.cpp');p.add_argument('--expect-map-owned',action='store_true');p.add_argument('--cemetery-plan',type=Path,required=True);a=p.parse_args()
if a.expect_map_owned and not a.mayor_route:raise ValueError('Map ownership expectation requires the Mayor route')
package=a.package.resolve();out=a.out.resolve();game=package/'3ds/encore-native/encore-native.3dsx'
receipt=json.loads((package/'build-receipt.json').read_bytes());source=receipt['source_commit']
if not re.fullmatch('[0-9a-f]{40}',source):raise ValueError('Exact package source required')
identity=receipt['artifacts']['encore-native.3dsx']
if game.stat().st_size!=identity['bytes']or r.sha(game)!=identity['sha256']:raise ValueError('Genuine artifact identity mismatch')
if out.exists()or not out.is_relative_to(ROOT/'build/emulator-regression'):raise ValueError('Fresh owned regression directory required')
if not a.fixture.is_file()or not a.fixture.resolve().is_relative_to(ROOT/'build')or not 0<a.fixture.stat().st_size<=1048576:raise ValueError('Bounded core-validated fixture required')
if shutil.disk_usage(ROOT).free<2*1024**3:raise ValueError('Insufficient free disk')
preference=a.locale_preference.resolve()if a.locale_preference else None
if preference and(not preference.is_file()or not preference.is_relative_to(ROOT/'build')or not 0<preference.stat().st_size<=52):raise ValueError('Bounded core-validated locale preference required')
metadata_paths=['romfs/world/podunk/podunk.encroom','romfs/system/native.encresources','romfs/session/opening.encsession','romfs/ui/opening.encitems','romfs/world/house/opening.encroom']
if a.house_roundtrip:metadata_paths+=['romfs/session/opening.encrestore']
mayor_route=None
if a.mayor_route:
 route_file=a.mayor_route.resolve()
 if not route_file.is_file()or not route_file.is_relative_to(ROOT)or route_file.stat().st_size>65536:raise ValueError('Bounded checked Mayor route required')
 mayor_route=json.loads(route_file.read_bytes())
 if mayor_route.get('schema')!=1 or mayor_route.get('map_path')!='romfs/world/podunk/podunk.encmap' or mayor_route.get('route')!=3:raise ValueError('Unknown Mayor route contract')
 points=mayor_route.get('waypoints')
 if not isinstance(points,list)or not 2<=len(points)<=256 or any(not isinstance(p,list)or len(p)!=2 or any(type(v)not in(int,float)or not math.isfinite(v)or abs(v)>100000 for v in p)for p in points):raise ValueError('Mayor route points rejected')
 if any((x[0]==y[0])==(x[1]==y[1])for x,y in zip(points,points[1:])):raise ValueError('Mayor route must have nonempty cardinal segments')
 metadata_paths+=['romfs/world/podunk/podunk.encmap','romfs/world/mayor/mayor.encroom','romfs/world/mayor/mayor.encmap','romfs/sound/banks/mayor.encmusic','romfs/world/world.enclinks','romfs/ui/field.encmenus']
source_metadata=checked_resources(game.read_bytes(),source,metadata_paths,ROOT)
if mayor_route:
 from tools.emulator_walk_route import load as load_walk_route
 mayor_route=load_walk_route(a.mayor_route,source_metadata['romfs/world/podunk/podunk.encmap'])
 from tools import field_area as area,podunk_field as field,field_menus as menus
 from PIL import Image
 _,sections=field.unpack_container(source_metadata['romfs/world/mayor/mayor.encmap'],field.MAP_MAGIC,field.MAP_STRIDES,field.MAP_LIMIT)
 map_area=area.decode(sections[-1],sections[0]);assert map_area['sub_area']
 menu_recipe=json.loads(subprocess.check_output(['git','show',source+':content/native-field-menus.json'],cwd=ROOT))
 if menus.encode(menu_recipe)!=source_metadata['romfs/ui/field.encmenus']:raise ValueError('Map expectation differs from the exact artifact menu data')
 assert map_area['map_item']==menu_recipe['map_item']
 with Image.open(ROOT/'upstream/MOTHER-Encore'/menu_recipe['images'][0 if a.expect_map_owned else 1])as im:map_size=im.size
 map_view=menu_recipe['display_sizes'][1]
 expected_map=tuple(int(max(0,min(p-v/2,max(0,size-v))))for p,v,size in zip(map_area['map_offset'],map_view,map_size))
restore_recipe=None
if a.house_roundtrip:
 from tools.native_restore import encode as encode_restore
 restore_recipe=json.loads(subprocess.check_output(['git','show',source+':content/native-restore.json'],cwd=ROOT))
 if encode_restore(restore_recipe)!=source_metadata['romfs/session/opening.encrestore']:raise ValueError('House music expectations differ from artifact restore rules')
cemetery_plan=json.loads(a.cemetery_plan.read_bytes())
if set(cemetery_plan)!={'scope','expanded','grid','waypoints'} or cemetery_plan['grid']!=8 or cemetery_plan['waypoints'][0]!=[8,-39] or cemetery_plan['waypoints'][-1]!=[312,4561]:raise ValueError('Unreviewed cemetery approach')
actual_room=parse_pack(subprocess.check_output(['git','show',source+':romfs/world/podunk/podunk.encroom'],cwd=ROOT))
assert len(actual_room['sections']['Program'])==6 and actual_room['strings'][actual_room['sections']['Program'][5]['source_path_string']]=='Podunk/cutscenes/graveyard_zombie_roots'
driver_revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
driver_sources={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest()for name in ['tools/run_podunk_bridge_semantics.py','tools/emulator_artifact.py','tools/ci_console_check.py','tools/run_emulator_semantics.py','tools/cdb_client.py','tools/semantic_probe.py']}
driver_sources['build/full-port/run_cemetery_record.py']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
if a.mayor_route:
 driver_sources[a.mayor_route.resolve().relative_to(ROOT).as_posix()]=hashlib.sha256(a.mayor_route.read_bytes()).hexdigest()
 driver_sources['tools/emulator_walk_route.py']=hashlib.sha256((ROOT/'tools/emulator_walk_route.py').read_bytes()).hexdigest()
 for name in ['tools/field_area.py','tools/podunk_field.py','tools/field_menus.py']:driver_sources[name]=hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
room=parse_pack(source_metadata['romfs/world/podunk/podunk.encroom']);flags={room['strings'][f['name_string']]:i for i,f in enumerate(room['sections']['Flag'])}
def flag(s,name):i=flags[name];return bool(s['story_flag_'+str(i//32)]&(1<<(i%32)))
if a.house_roundtrip:driver_sources['tools/native_restore.py']=hashlib.sha256((ROOT/'tools/native_restore.py').read_bytes()).hexdigest()
out.mkdir(parents=True);shutil.copyfile(__file__,out/'driver_snapshot.py');work=out/'temporary';work.mkdir();(work/'.owned-emulator-workspace').write_text('podunk-bridge-v1')
r.CONFIG=r.CONFIG.replace('graphics_api=1','graphics_api=2')+'\n[System]\ninit_clock=1\ninit_clock\\default=false\ninit_time='+str(a.clock)+'\ninit_time\\default=false\ninit_ticks_type=1\ninit_ticks_type\\default=false\ninit_ticks_override=0\ninit_ticks_override\\default=false\n'
exe=r.portable(a.emulator.resolve(),work/'emulator')
for source_file,dest in [(a.fixture,'sdmc/3ds/encore-native/save-1.encsave'),(a.dsp_component,'sdmc/3ds/dspfirm.cdc')]:
 target=exe.parent/'user'/dest;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source_file,target)
if preference:shutil.copyfile(preference,exe.parent/'user/sdmc/3ds/encore-native/language.encprefs')
with socket.socket()as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
token=secrets.token_hex(24);startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
process=client=session=None;records={};performance={};result={'status':'failed'}
try:
 with(out/'stdout.log').open('wb')as log:process=subprocess.Popen([str(exe),'--cdb-port',str(port),'--cdb-token',token,'--cdb-allow-input',str(game)],cwd=exe.parent,stdout=log,stderr=subprocess.STDOUT,startupinfo=startup)
 for attempt in range(600):
  try:client=r.CdbClient(token,port);break
  except OSError:time.sleep(.1)
 assert client is not None
 caps=client.call('cdb.capabilities');assert caps['memoryWriteEnabled']is False and caps['inputEnabled']is True
 time.sleep(8);session=r.Session(client)
 def state(running=False):
  for attempt in range(4):
   try:return session.state()
   except ValueError as error:
    if 'mid-commit'not in str(error):raise
    if running:
     # A free-running producer completes its own write. frame.step here would
     # silently pause the guest again and freeze every later audio sample.
     time.sleep(.01);continue
    # A paused producer cannot finish its seqlock write by wall-clock waiting.
    # Advance normal guest execution, with the same bounded read retry contract.
    try:return session.advance(1)
    except ValueError as retry:
     if 'mid-commit'not in str(retry):raise
  raise RuntimeError('Observation remained mid-commit')
 def clean(s):
  for name in ('house_error','field_error','menu_error','audio_error','battle_error'):assert not s[name],(name,s)
 def advance(frames):
  s=session.advance(frames);clean(s);return s
 def wait(predicate,limit=1800,chunk=8):
  s=state()
  for _ in range((limit+chunk-1)//chunk):
   if predicate(s):return s
   s=advance(chunk)
  raise TimeoutError(str(s))
 def pulse(button,settle=15):
  client.call('input.set',{'buttons':[button]});advance(4);client.call('input.clear');return advance(settle)
 def record(name):
  s=state();records[name]=s;print(name,{k:s[k]for k in ('frame','field_phase','world_stage','world_dialogue','battle_active','battle_round_phase','hp','enemy_hp','experience','cash','player_x','player_y')},flush=True);return s
 def capture(name):
  job=client.call('screen.capture',{'screen':'combined'});event=client.event('screen.captured',jobId=job['jobId']);raw=base64.b64decode(event['data']);assert len(raw)<=2097152;(out/(name+'.png')).write_bytes(raw)
 def audio_window(name):
  # CDB pauses only the game process; the DSP server keeps its own schedule.
  # Observe queue progress during normal running, then restore debugger pause.
  samples=[];client.call('input.clear');client.call('process.resume')
  try:
   time.sleep(1)
   for i in range(8):
    time.sleep(.15);sample=state(running=True);clean(sample);samples.append(sample);records[name+'_'+str(i)]=sample
   assert len({s['frame']for s in samples})>1,'Audio observation unexpectedly paused the guest'
   performance[name]=client.call('system.info').get('performance',{})
  finally:client.call('process.pause')
  return samples
 def menu_smoke(name):
  # SceneDoor Unpause fires at mostly_out, before the full FadeOut is done.
  # Wait through that source transition; START is intentionally gated meanwhile.
  advance(60)
  before=state();assert before['version']==9 and before['menu_phase']==0 and before['submenu_phase']==0
  pulse('START');wait(lambda s:s['menu_phase']==2 and s['paused'])
  pulse('A');wait(lambda s:s['submenu_phase']==1)
  shown=record(name);capture(name);assert shown['submenu_count']==before['inventory_count']
  pulse('B');wait(lambda s:s['submenu_phase']==0 and s['menu_phase']==2)
  pulse('B');wait(lambda s:s['menu_phase']==0 and not s['paused'])
  after=state()
  stable=['hp','pp','inventory_count','key_count','experience','cash','bank','earned_cash',*['story_flag_'+str(i)for i in range(6)],*['inventory_'+str(i)+'_'+key for i in range(16)for key in ('definition','doses','equipped')]]
  assert {k:before[k]for k in stable}=={k:after[k]for k in stable},'Opening Goods mutated session state'
 def travel(button,predicate,limit=1800):
  client.call('input.clear');advance(3)
  client.call('input.set',{'buttons':[button]})
  try:
   last=None;stopped=0
   for tick in range(limit):
    s=advance(1)
    if predicate(s):return s
    pos=(s['player_x'],s['player_y']);stopped=stopped+1 if pos==last and not s['paused'] and s['world_stage']==0 else 0;last=pos
    if stopped>=150:raise TimeoutError('Blocked '+button+': '+str(s))
   raise TimeoutError('Travel budget '+str(s))
  finally:client.call('input.clear');advance(2)
 def precise_axis(axis,value):
  other='player_y'if axis=='player_x'else'player_x'
  now=state();delta=value-now[axis]
  if abs(delta)>24:
   button=('RIGHT'if delta>0 else'LEFT')if axis=='player_x'else('DOWN'if delta>0 else'UP')
   travel(button,lambda s:abs(value-s[axis])<=16)
  history=[]
  client.call('input.clear');advance(3)
  for attempt in range(128):
   now=state();delta=value-now[axis]
   if abs(delta)<=1.05:return
   button=('RIGHT'if delta>0 else'LEFT')if axis=='player_x'else('DOWN'if delta>0 else'UP')
   if abs(delta)<9:
    # Let the app's ordinary clock settle before the short press. Process-only
    # debugger pauses otherwise make the next render catch up several ticks.
    client.call('input.clear');client.call('process.resume');time.sleep(.10)
    tap=client.call('input.tap',{'buttons':[button],'frames':1+(attempt%3)})
    notice=client.event('input.changed');assert notice['frame']>=tap['releaseFrame'] and notice['state']['buttons']==0,'Scheduled key release was not acknowledged'
    time.sleep(.08);client.call('process.pause');after=state()
   else:
    client.call('input.set',{'buttons':[button]});advance(1);client.call('input.clear');after=advance(3)
   history.append((now[axis],after[axis],after[other]))
  raise TimeoutError('Precise normal-input alignment failed '+axis+' target='+str(value)+' recent='+str(history[-12:]))
 def precise_point(x,y):
  precise_axis('player_x',x);precise_axis('player_y',y)
  now=state();assert abs(now['player_x']-x)<=1.05 and abs(now['player_y']-y)<=1.05,'Corner alignment drifted'
 def waypoint(button,axis,value):
  # Coarse braking alone is insufficient: an in-flight HID sample and the
  # normal catch-up clock can advance several source ticks. Align this axis
  # before turning, as on the later planned route. Never warp the guest.
  positive=button in ('RIGHT','DOWN');threshold=value-4 if positive else value+4
  travel(button,lambda s:s[axis]>=threshold if positive else s[axis]<=threshold)
  precise_axis(axis,value)
  s=state();print('waypoint',button,axis,value,s['player_x'],s['player_y'],flush=True)
 wait(lambda s:s['title_phase']==1);record('title_ready')
 # Align normal inputs at a deterministic emulated frame, never alter guest state.
 assert state()['frame']<2000,state()
 while state()['frame']<2000:advance(min(120,2000-state()['frame']))
 pulse('DOWN');pulse('A');wait(lambda s:s['title_phase']==3);pulse('A');wait(lambda s:s['title_phase']==4);pulse('A');wait(lambda s:s['title_phase']==0 and s['paused']==0)
 initial=record('loaded');assert initial['world_stage']==0 and initial['world_dialogue']==0xffffffff,'Checkpoint activates a source House cutscene; this route requires a walking checkpoint'
 assert flag(initial,'beat_dog_bridge')and flag(initial,'pippis_mom_call')and flag(initial,'pippis_mom_help');assert any(initial['inventory_'+str(i)+'_definition']==3 and initial['inventory_'+str(i)+'_equipped']for i in range(initial['inventory_count']))
 travel('DOWN',lambda s:s['field_scene']!=0 and s['field_phase']==0 and s['paused']==0);record('outside')
 if a.house_roundtrip:
  advance(60);before=record('roundtrip_before')
  stable_trip=['hp','pp','level','experience','cash','bank','earned_cash','inventory_count','key_count','menu_uid_hash','menu_key_uid_hash','menu_inventory_hash','menu_key_hash','menu_settings_hash','menu_controls_hash',*['story_flag_'+str(i)for i in range(6)],*['inventory_'+str(i)+'_'+key for i in range(16)for key in ('definition','doses','equipped')]]
  travel('UP',lambda s:s['field_scene']==0)
  wait(lambda s:s['field_scene']==0 and not s['paused']and s['world_stage']==0,limit=2400)
  house=record('roundtrip_house');capture('roundtrip-house')
  assert {k:before[k]for k in stable_trip}=={k:house[k]for k in stable_trip},'House return mutated session'
  expected=[m for m in restore_recipe['music_areas']if all(abs(house['player_'+axis]-m['center'][i])<=m['extents'][i]for i,axis in enumerate(('x','y')))and all(flag(house,c['flag_name'])==c['expected_value']for c in m['conditions'])]
  assert len(expected)<=1 and all(m['supported']for m in expected),'House source music selection unsupported or ambiguous'
  performance['house_source_music']=[{'source_path':m['source_path'],'resource_path':m['resource_path'],'conditions':m['conditions']}for m in expected]
  samples=audio_window('roundtrip_house_audio')
  if expected:
   assert all(s['music_playing']and s['dsp_music_playing']for s in samples)and len({(s['dsp_music_sample_pos'],s['dsp_music_wave_seq'])for s in samples})>1,'Enabled source House music did not progress after return'
  else:
   assert all(not s['music_playing']and not s['dsp_music_playing']and not s['dsp_region_playing']for s in samples),'Source-disabled House music or retired field music kept playing'
  travel('DOWN',lambda s:s['field_scene']!=0 and s['field_phase']==0 and not s['paused'])
  outside=record('roundtrip_outside');capture('roundtrip-outside')
  assert {k:before[k]for k in stable_trip}=={k:outside[k]for k in stable_trip},'Repeated field entry mutated session'
  samples=audio_window('roundtrip_field_audio')
  assert any(s['dsp_region_playing']for s in samples)and len({(s['dsp_region_sample_sum'],s['dsp_region_wave_sum'])for s in samples})>1,'Field music did not progress after reentry'
 # Continue with ordinary inputs through the source collision map to the cemetery.
 advance(60);before_cemetery=record('cemetery_depart_house');assert not flag(before_cemetery,'cem_roots_grown')
 for i,(x,y)in enumerate(cemetery_plan['waypoints']):
  precise_point(x,y)
  if i%12==0:record('cemetery_walk_'+str(i))
  assert state()['world_stage']==0 and state()['field_scene']==2,'Unexpected cutscene while approaching cemetery'
 approach=record('cemetery_approach');capture('cemetery-approach')
 travel('DOWN',lambda s:s['world_stage']==1 and s['world_programme']==5,limit=240)
 record('cemetery_started');capture('cemetery-started')
 phases=set();grew=False
 for tick in range(1500):
  s=state();clean(s);phases.add(s['world_phrase'])
  if flag(s,'cem_roots_grown')and not grew:record('cemetery_grow');capture('cemetery-grow');grew=True
  if s['world_stage']==0:break
  advance(2)
 else:raise AssertionError('Cemetery source programme did not complete')
 assert grew and phases==set(range(11)),('Cemetery phase/flag mismatch',phases)
 after_cemetery=record('cemetery_completed');capture('cemetery-completed')
 assert flag(after_cemetery,'cem_roots_grown') and after_cemetery['field_phase']==0 and not after_cemetery['paused']
 revision=(after_cemetery['story_revision_lo'],after_cemetery['story_revision_hi'])
 advance(60);travel('DOWN',lambda s:s['player_y']>=4608);travel('UP',lambda s:s['player_y']<=4561)
 repeated=record('cemetery_repeated');assert repeated['world_stage']==0 and flag(repeated,'cem_roots_grown') and revision==(repeated['story_revision_lo'],repeated['story_revision_hi'])
 performance['cemetery_roots']={'phases':sorted(phases),'flag':True,'repeated':True,'scope':'Normal-input source cutscene; no save validation or pixel equivalence'}
 # Return to the real House authority and use the original telephone Record.
 for i,(x,y)in enumerate(reversed(cemetery_plan['waypoints'])):
  precise_point(x,y)
  if i%18==0:record('cemetery_return_'+str(i))
  assert state()['world_stage']==0 and state()['field_scene']==2
 travel('UP',lambda s:s['field_scene']==0,limit=900)
 wait(lambda s:s['world_stage']==0 and not s['paused'],limit=2400)
 home=record('cemetery_home');assert flag(home,'cem_roots_grown');capture('cemetery-home')
 precise_point(144,711);pulse('UP',settle=5);pulse('A')
 for i in range(120):
  s=state();clean(s)
  if s['choice_phase']==2:break
  if s['dialogue_active']and(s['dialogue_finished']or s['dialogue_stopped']):pulse('A')
  else:advance(90)
 else:raise AssertionError('Original Dad normal choice did not open')
 record('cemetery_record_choice');capture('cemetery-record-choice');pulse('A')
 for i in range(120):
  s=state();clean(s)
  if s['save_phase']==2:break
  if s['dialogue_active']and(s['dialogue_finished']or s['dialogue_stopped']):pulse('A')
  else:advance(60)
 else:raise AssertionError('Original Record SaveSelect did not open')
 record('cemetery_record_slots');capture('cemetery-record-slots')
 advance(60);pulse('DOWN');pulse('A');advance(60)
 saved=exe.parent/'user/sdmc/3ds/encore-native/save-2.encsave'
 assert saved.is_file()and saved.stat().st_size>1000,'Record did not write owned empty slot2'
 shutil.copyfile(saved,out/'recorded-slot2.encsave')
 record_state=record('cemetery_recorded');assert flag(record_state,'cem_roots_grown')and record_state['save_phase']==2
 capture('cemetery-recorded')
 for path,blob in source_metadata.items():
  if path in ('romfs/session/opening.encsession','romfs/ui/opening.encitems','romfs/world/house/opening.encroom'):
   dest=out/'artifact-resources'/path.removeprefix('romfs/');dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(blob)
 performance['actual_record']={'slot':2,'bytes':saved.stat().st_size,'sha256':r.sha(saved),'scope':'Actual normal-input telephone Record from a source-validated artificial initial checkpoint; no LOAD claim yet'}
 frontier=record_state
 result={'status':'passed','scope':'Artificial core-validated post-Mayor checkpoint; normal LOAD/House exit/walk/cemetery programme/return/actual telephone Record slot2; no original pixel, old battle rerun, natural new-game, saved LOAD, whole-game or hardware claim','source_commit':receipt['source_commit'],'game_sha256':r.sha(game),'driver_revision':driver_revision,'driver_sources_sha256':driver_sources,'source_metadata_sha256':{path:hashlib.sha256(value).hexdigest()for path,value in source_metadata.items()},'clock_fixture':a.clock,'fixture_sha256':r.sha(a.fixture),'emulator_sha256':r.sha(a.emulator),'capabilities':{'memoryWriteEnabled':caps['memoryWriteEnabled'],'inputEnabled':caps['inputEnabled']},'cemetery':performance['cemetery_roots'],'projection':{k:frontier[k]for k in ['hp','pp','level','experience','cash','bank','earned_cash','inventory_count','key_count',*['story_flag_'+str(i)for i in range(6)]]}}
except BaseException as exc:
 result['error']=repr(exc)
 if session:
  try:records['failure']=state();capture('failure')
  except Exception:pass
 raise
finally:
 result['performance']=performance
 (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');(out/'observations.json').write_text(json.dumps(records,indent=2)+'\n')
 if process:
  process.terminate()
  try:process.wait(timeout=15)
  except subprocess.TimeoutExpired:process.kill();process.wait(timeout=15)
 if client:client.close()
 log=exe.parent/'user/log/azahar_log.txt'
 if log.exists():shutil.copyfile(log,out/'emulator.log')
 if (work/'.owned-emulator-workspace').read_text()!='podunk-bridge-v1':raise ValueError('Cleanup ownership marker differs')
 shutil.rmtree(work)
