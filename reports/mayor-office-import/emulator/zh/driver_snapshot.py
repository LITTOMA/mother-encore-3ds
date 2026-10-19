#!/usr/bin/env python3
"""Normal-input bridge regression; read-only observation, owned fixed-clock profile.
The checkpoint is artificial and does not claim the opening was played.
"""
import argparse,base64,json,math,re,secrets,shutil,socket,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_emulator_semantics as r
from tools.native_content import parse_pack
from tools.emulator_artifact import checked_resources
import hashlib
p=argparse.ArgumentParser();p.add_argument('--package',type=Path,required=True);p.add_argument('--fixture',type=Path,required=True);p.add_argument('--emulator',type=Path,required=True);p.add_argument('--dsp-component',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--clock',type=int,default=946681277);p.add_argument('--mom-help',action='store_true');p.add_argument('--house-roundtrip',action='store_true');p.add_argument('--locale-preference',type=Path);p.add_argument('--mayor-route',type=Path);p.add_argument('--wsl-walk-planner',type=Path);p.add_argument('--walk-planner-source',type=Path,default=ROOT/'tools/field_walk_plan.cpp');a=p.parse_args()
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
metadata_paths=['romfs/world/podunk/podunk.encroom','romfs/system/native.encresources']
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
 metadata_paths+=['romfs/world/podunk/podunk.encmap','romfs/world/mayor/mayor.encroom','romfs/world/mayor/mayor.encmap','romfs/sound/banks/mayor.encmusic','romfs/world/world.enclinks']
source_metadata=checked_resources(game.read_bytes(),source,metadata_paths,ROOT)
if mayor_route:
 from tools.emulator_walk_route import load as load_walk_route
 mayor_route=load_walk_route(a.mayor_route,source_metadata['romfs/world/podunk/podunk.encmap'])
restore_recipe=None
if a.house_roundtrip:
 from tools.native_restore import encode as encode_restore
 restore_recipe=json.loads(subprocess.check_output(['git','show',source+':content/native-restore.json'],cwd=ROOT))
 if encode_restore(restore_recipe)!=source_metadata['romfs/session/opening.encrestore']:raise ValueError('House music expectations differ from artifact restore rules')
driver_revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
driver_sources={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest()for name in ['tools/run_podunk_bridge_semantics.py','tools/emulator_artifact.py','tools/ci_console_check.py','tools/run_emulator_semantics.py','tools/cdb_client.py','tools/semantic_probe.py']}
if a.mayor_route:
 driver_sources[a.mayor_route.resolve().relative_to(ROOT).as_posix()]=hashlib.sha256(a.mayor_route.read_bytes()).hexdigest()
 driver_sources['tools/emulator_walk_route.py']=hashlib.sha256((ROOT/'tools/emulator_walk_route.py').read_bytes()).hexdigest()
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
 def state():
  for attempt in range(4):
   try:return session.state()
   except ValueError as error:
    if 'mid-commit'not in str(error):raise
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
    time.sleep(.15);sample=state();clean(sample);samples.append(sample);records[name+'_'+str(i)]=sample
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
  # The HID sample already in flight plus neutral settling advances two more
  # source 2px movement ticks. Brake before the requested corner; never warp
  # the guest or alter its original collision geometry.
  positive=button in ('RIGHT','DOWN');threshold=value-4 if positive else value+4
  travel(button,lambda s:s[axis]>=threshold if positive else s[axis]<=threshold)
  s=state();print('waypoint',button,axis,value,s['player_x'],s['player_y'],flush=True)
 wait(lambda s:s['title_phase']==1);record('title_ready')
 # Align normal inputs at a deterministic emulated frame, never alter guest state.
 assert state()['frame']<2000,state()
 while state()['frame']<2000:advance(min(120,2000-state()['frame']))
 pulse('DOWN');pulse('A');wait(lambda s:s['title_phase']==3);pulse('A');wait(lambda s:s['title_phase']==4);pulse('A');wait(lambda s:s['title_phase']==0 and s['paused']==0)
 initial=record('loaded');assert initial['world_stage']==0 and initial['world_dialogue']==0xffffffff,'Checkpoint activates a source House cutscene; this route requires a walking checkpoint'
 assert not flag(initial,'beat_dog_bridge')and not flag(initial,'pippis_mom_call');assert any(initial['inventory_'+str(i)+'_definition']==3 and initial['inventory_'+str(i)+'_equipped']for i in range(initial['inventory_count']))
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
 menu_smoke('goods_before_battle')
 travel('DOWN',lambda s:s['player_y']>=60);travel('LEFT',lambda s:s['player_x']<=-32);travel('DOWN',lambda s:s['player_y']>=190);travel('RIGHT',lambda s:s['player_x']>=314);travel('DOWN',lambda s:s['player_y']>=274)
 if state()['player_y']>278:travel('UP',lambda s:s['player_y']<=276)
 travel('RIGHT',lambda s:s['world_stage']!=0);record('bridge_trigger');capture('bridge-trigger')
 wait(lambda s:s['battle_active']and s['battle_round_phase']==1,limit=2400)
 battle=record('battle_commands');capture('battle-commands');assert battle['region_music_paused']and battle['dialogue_music_playing']
 samples=audio_window('battle_audio')
 assert all(s['region_music_paused']and s['dialogue_music_playing']and s['dialogue_music_asset']==battle['dialogue_music_asset']for s in samples)
 assert any(s['dsp_dialogue_playing']for s in samples)and len({(s['dsp_dialogue_sample_pos'],s['dsp_dialogue_wave_seq'])for s in samples})>1
 assert len({(s['dsp_region_sample_sum'],s['dsp_region_wave_sum'])for s in samples})==1
 for turn in range(160):
  s=state()
  if not s['battle_active']:break
  assert s['battle_round_phase']!=6,('Fixture was defeated',s)
  pulse('A',settle=35)
 else:raise TimeoutError('Battle did not return '+str(state()))
 won=record('victory_return');capture('victory-return');assert flag(won,'beat_dog_bridge')and not flag(won,'pippis_mom_call');assert won['experience']==initial['experience']+8 and won['cash']==initial['cash'] and won['bank']==initial['bank']+20 and won['earned_cash']==initial['earned_cash']+20
 assert won['loot_ready']and won['loot_granted'];assert won['inventory_count']==initial['inventory_count']+won['loot_selected'];assert not won['region_music_paused']and not won['dialogue_music_playing']
 samples=audio_window('returned_audio')
 assert all(not s['region_music_paused']and not s['dialogue_music_playing']and s['region_music_track']==battle['region_music_track']for s in samples)
 assert any(s['dsp_region_playing']for s in samples)and len({(s['dsp_region_sample_sum'],s['dsp_region_wave_sum'])for s in samples})>1
 wait(lambda s:s['world_stage']==0 and s['paused']==0,limit=1200)
 menu_smoke('goods_after_battle')
 travel('RIGHT',lambda s:s['world_stage']!=0);record('mom_trigger');capture('mom-trigger')
 for text in range(1000,1005):
  wait(lambda s:s['world_dialogue']==text and s['dialogue_active']and s['dialogue_finished'],limit=1800)
  record('mom_'+str(text));capture('mom_'+str(text));advance(90);assert state()['world_dialogue']==text,('Dialogue advanced without input',state())
  pulse('A',settle=20)
 wait(lambda s:s['world_stage']==0 and s['paused']==0,limit=1200)
 called=record('mom_complete');assert flag(called,'pippis_mom_call')and flag(called,'beat_dog_bridge')
 if a.mom_help:
  # The source call restores the talker at (640,304). Approach from above
  # by ordinary walking; no guest warp or memory write is permitted.
  waypoint('RIGHT','player_x',584);waypoint('DOWN','player_y',282);waypoint('RIGHT','player_x',640)
  # The lateral approach ends facing right. Face/walk toward the talker below
  # us, rather than relying on a marginal body overlap at y283.
  waypoint('DOWN','player_y',286);approach=record('help_approach');assert approach['direction_y']>0
  pulse('A');wait(lambda s:s['world_programme']==2 and s['world_stage']!=0)
  baseline=record('help_started');assert not flag(baseline,'pippis_mom_help')
  stable_help=['hp','pp','level','experience','cash','bank','earned_cash','inventory_count','key_count',*['inventory_'+str(i)+'_'+key for i in range(16)for key in ('definition','doses','equipped')]]
  def acknowledge(text,waits):
   for index in range(waits):
    wait(lambda s:s['world_dialogue']==text and s['dialogue_stopped'],limit=2400)
    stopped=record('help_'+str(text)+'_wait_'+str(index));capture('help_'+str(text)+'_wait_'+str(index));advance(90)
    assert state()['world_dialogue']==text and state()['dialogue_visible']==stopped['dialogue_visible'] and state()['dialogue_stopped']
    pulse('A',settle=20)
   wait(lambda s:s['world_dialogue']==text and s['dialogue_finished'],limit=2400)
   record('help_'+str(text)+'_finished');capture('help_'+str(text)+'_finished');advance(90)
   assert state()['world_dialogue']==text and state()['dialogue_finished']
   pulse('A',settle=20)
  for text in range(1010,1015):acknowledge(text,2 if text==1011 else 0)
  wait(lambda s:s['world_stage']==0 and not s['paused'],limit=1200);first=record('help_complete');assert flag(first,'pippis_mom_help')
  assert {k:baseline[k]for k in stable_help}=={k:first[k]for k in stable_help},'Help mutated rewards or inventory'
  advance(90);pulse('A');wait(lambda s:s['world_programme']==3 and s['world_stage']!=0);acknowledge(1020,2)
  wait(lambda s:s['world_stage']==0 and not s['paused'],limit=1200);called=record('help_repeat_complete')
  assert {k:first[k]for k in stable_help}=={k:called[k]for k in stable_help} and all(first['story_flag_'+str(i)]==called['story_flag_'+str(i)]for i in range(6)),'Repeated help mutated the session'
  # Re-enter the bridge's middle before walking west; the lower riverbank is solid.
  waypoint('LEFT','player_x',584);waypoint('UP','player_y',264)
 # Revisit both completed areas before continuing along the road. The host
 # test separately covers the full forest-corridor round trip. Reward and
 # dialogue idempotence use exactly the same state assertion in the emulator.
 stable=['hp','pp','level','experience','cash','bank','earned_cash','inventory_count','key_count','loot_selected',*['story_flag_'+str(i)for i in range(6)],*['inventory_'+str(i)+'_'+n for i in range(16)for n in ('definition','doses','equipped')]]
 travel('LEFT',lambda s:s['player_x']<=400)
 travel('RIGHT',lambda s:s['player_x']>=600)
 travel('LEFT',lambda s:s['player_x']<=536)
 repeated=record('completed_triggers_repeated');assert not repeated['battle_active']and repeated['world_stage']==0
 assert {k:called[k]for k in stable}=={k:repeated[k]for k in stable}
 # Walk beyond both original progress triggers by normal input.
 waypoint('RIGHT','player_x',536)
 for button,axis,value in [('DOWN','player_y',480),('LEFT','player_x',408),('DOWN','player_y',488),('LEFT','player_x',400),('DOWN','player_y',512),('LEFT','player_x',344),('DOWN','player_y',520),('LEFT','player_x',336),('DOWN','player_y',528),('LEFT','player_x',184),('DOWN','player_y',536),('LEFT','player_x',176),('DOWN','player_y',560),('LEFT','player_x',-560),('UP','player_y',496)]:
  waypoint(button,axis,value)
 frontier=record('west_road');capture('west-road')
 if mayor_route:
  baseline=frontier
  if a.wsl_walk_planner:
   planner=a.wsl_walk_planner.resolve()
   if not planner.is_relative_to(ROOT/'build')or planner.read_bytes()[:4]!=b'\x7fELF':raise ValueError('Owned native planner ELF required')
   nav=work/'walk-plan';nav.mkdir();nr=nav/'room.encroom';nm=nav/'map.encmap';nf=nav/'enabled-flags.txt'
   nr.write_bytes(source_metadata['romfs/world/podunk/podunk.encroom']);nm.write_bytes(source_metadata['romfs/world/podunk/podunk.encmap'])
   enabled=[name for name in flags if flag(frontier,name)];nf.write_text(''.join(name+'\n'for name in enabled),encoding='utf8')
   def wp(path):
    value=path.resolve().as_posix()
    if not re.match('[A-Za-z]:/',value):raise ValueError('Planner expects checked Windows workspace paths')
    return '/mnt/'+value[0].lower()+value[2:]
   goal=mayor_route['waypoints'][-1]
   command=['wsl','-d','Ubuntu-20.04','--',wp(planner),wp(nr),wp(nm),wp(nf),str(frontier['player_x']),str(frontier['player_y']),str(goal[0]),str(goal[1]),'6']
   planned=subprocess.run(command,capture_output=True,timeout=45);(out/'planner-stderr.log').write_bytes(planned.stderr)
   if planned.returncode:raise RuntimeError('Bounded static route planner rejected current observed state')
   effective=json.loads(planned.stdout);assert set(effective)=={'scope','expanded','grid','waypoints'}
   mayor_route=dict(mayor_route,**effective,planner_source_sha256=hashlib.sha256(a.walk_planner_source.read_bytes()).hexdigest())
   from tools.emulator_walk_route import validate as validate_walk_route
   validate_walk_route(mayor_route,source_metadata['romfs/world/podunk/podunk.encmap']);(out/'effective-walk-route.json').write_text(json.dumps(mayor_route,indent=2)+'\n')
   driver_sources['host_walk_planner_elf_sha256']=r.sha(planner);driver_sources['host_walk_planner_cpp_sha256']=mayor_route['planner_source_sha256']
  points=mayor_route['waypoints'];assert abs(frontier['player_x']-points[0][0])<=4 and abs(frontier['player_y']-points[0][1])<=4,'Static route start differs from actual player'
  precise_point(*points[0])
  for i,(x,y)in enumerate(points[1:],1):
   precise_point(x,y)
   if i%8==0:record('town_hall_walk_'+str(i))
   assert state()['field_scene']==2 and state()['world_stage']==0,'Walk left its admitted source route'
  record('town_hall_approach');capture('town-hall-approach')
  travel('UP',lambda s:s['field_scene']==3 and s['field_phase']==0 and not s['paused'],limit=1800)
  entered=record('mayor_lobby_entered');capture('mayor-lobby')
  town_track=int.from_bytes(source_metadata['romfs/sound/banks/mayor.encmusic'][64:68],'little')
  wait(lambda s:s['region_music_track']==town_track,limit=600)
  samples=audio_window('mayor_lobby_audio');assert any(s['dsp_region_playing']for s in samples)and len({(s['dsp_region_sample_sum'],s['dsp_region_wave_sum'])for s in samples})>1,'Town Hall source track did not progress'
  menu_smoke('mayor_lobby_goods')
  def mayor_map(name):
   # The source door unpauses at mostly_out, while the remaining fade still
   # owns input. As with menu_smoke, let the ordinary transition finish first.
   advance(60)
   pulse('START');wait(lambda s:s['menu_phase']==2 and s['paused']);pulse('DOWN');pulse('DOWN');pulse('A');wait(lambda s:s['submenu_phase']==11)
   shown=record(name);capture(name);point=shown['menu_map_x'],shown['menu_map_y'];pulse('B');wait(lambda s:s['submenu_phase']==0);pulse('B');wait(lambda s:s['menu_phase']==0 and not s['paused']);return point
  fixed_map=mayor_map('mayor_lobby_map')
  def read_mayor(text,choice=False):
   pages=0
   for _ in range(1600):
    now=state();assert now['field_scene']==3 and now['world_dialogue']==text,(text,now)
    if choice and now['choice_phase']==2:return
    if now['dialogue_stopped']or(now['dialogue_finished']and not choice):
     pages+=1;assert pages<32,'Unbounded Mayor dialogue acknowledgement';held=record('mayor_'+str(text)+'_page_'+str(pages));advance(90)
     assert state()['world_dialogue']==text and state()['dialogue_visible']==held['dialogue_visible'],'Mayor text advanced without player confirmation'
     pulse('A',settle=20)
     if not choice and state()['world_dialogue']!=text:return
    else:advance(8)
   raise TimeoutError('Mayor source text/choice did not complete '+str(state()))
  waypoint('UP','player_y',-20);pulse('A');wait(lambda s:s['world_dialogue']==2000);record('abbott_lobby_talk');read_mayor(2000);wait(lambda s:s['world_stage']==0 and not s['paused'])
  waypoint('LEFT','player_x',-72);travel('UP',lambda s:s['field_scene']==3 and s['player_y']<-400 and s['field_phase']==0 and not s['paused'])
  record('mayor_office_entered');capture('mayor-office');menu_smoke('mayor_office_goods');assert mayor_map('mayor_office_map')==fixed_map,'Subarea map moved with internal world coordinates'
  waypoint('UP','player_y',-560)
  for selected in ('yes','no','cancel'):
   pulse('A');wait(lambda s:s['world_dialogue']==2001);read_mayor(2001,True);record('mayor_help_choice_'+selected);capture('mayor-help-choice-'+selected)
   if selected=='no':pulse('RIGHT')
   pulse('B'if selected=='cancel'else'A');expected=2002 if selected=='yes'else 2003;wait(lambda s:s['world_dialogue']==expected);read_mayor(expected);wait(lambda s:s['world_stage']==0 and not s['paused']);record('mayor_help_'+selected+'_complete');advance(90)
  travel('DOWN',lambda s:s['field_scene']==3 and s['player_y']>-200 and s['field_phase']==0 and not s['paused'])
  record('mayor_lobby_returned');capture('mayor-lobby-returned');assert mayor_map('mayor_return_map')==fixed_map
  waypoint('DOWN','player_y',8);waypoint('RIGHT','player_x',8);travel('DOWN',lambda s:s['field_scene']==2 and s['field_phase']==0 and not s['paused'])
  frontier=record('mayor_returned_to_podunk');capture('mayor-returned-to-podunk');assert {k:baseline[k]for k in stable}=={k:frontier[k]for k in stable},'Initial Mayor visit changed rewards, inventory or flags'
  menu_smoke('mayor_return_goods');samples=audio_window('mayor_return_field_audio');assert any(s['dsp_region_playing']for s in samples)and len({(s['dsp_region_sample_sum'],s['dsp_region_wave_sum'])for s in samples})>1,'Podunk music did not progress after Mayor return'
 result={'status':'passed','scope':'Artificial post-basement checkpoint, normal LOAD/door/walk/battle/dialogue input; no original pixel or whole-game claim','source_commit':receipt['source_commit'],'driver_revision':driver_revision,'driver_sources_sha256':driver_sources,'source_metadata_sha256':{path:hashlib.sha256(value).hexdigest()for path,value in source_metadata.items()},'clock_fixture':a.clock,'fixture_sha256':r.sha(a.fixture),'emulator_sha256':r.sha(a.emulator),'capabilities':{'memoryWriteEnabled':caps['memoryWriteEnabled'],'inputEnabled':caps['inputEnabled']},'idempotent_trigger_revisit':True,'mayor_initial_route':bool(mayor_route),'mom_help_and_repeat':a.mom_help,'house_roundtrip':a.house_roundtrip,'locale_preference_sha256':r.sha(a.locale_preference)if a.locale_preference else None,'menu_before_and_after_battle':True,'completion_projection':{k:frontier[k]for k in ['level','experience','cash','bank','earned_cash','key_count',*['story_flag_'+str(i)for i in range(6)]]},'projection':{k:frontier[k]for k in ['hp','pp','level','experience','cash','bank','earned_cash','inventory_count','key_count','loot_selected',*['story_flag_'+str(i)for i in range(6)],*['inventory_'+str(i)+'_'+n for i in range(16)for n in ('definition','doses','equipped')]]}}
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
