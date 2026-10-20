"""Actual recorded save -> ordinary LOAD/walk/Iggy choices/teaching/learned Bat, two cold runs.
Read-only probe and ordinary button input only; no natural-opening/full-game claim.
"""
from pathlib import Path
import argparse,base64,hashlib,json,secrets,shutil,socket,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT))
from tools import run_emulator_semantics as r
from tools.emulator_artifact import checked_resources
from tools.native_content import parse_pack
p=argparse.ArgumentParser();p.add_argument('--package',type=Path,required=True);p.add_argument('--fixture',type=Path,required=True);p.add_argument('--emulator',type=Path,required=True);p.add_argument('--dsp',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--walk-plan',type=Path,required=True);a=p.parse_args()
package=a.package.resolve();game=package/'3ds/encore-native/encore-native.3dsx';receipt=json.loads((package/'build-receipt.json').read_bytes());source=receipt['source_commit'];assert source=='f0296baf53d03285975c0fe9aa1639943a831911';identity=receipt['artifacts']['encore-native.3dsx'];assert game.stat().st_size==identity['bytes']and r.sha(game)==identity['sha256']
assert a.fixture.resolve().is_relative_to(ROOT/'build')and r.sha(a.fixture)=='2ec58ed5e5e9e03f9cf9832346fc31d6da77ccb80e21953f4c833c4ca67c1714'
out=a.out.resolve();assert out.is_relative_to(ROOT/'build/emulator-regression')and not out.exists();out.mkdir(parents=True);shutil.copyfile(__file__,out/'driver_snapshot.py')
metadata=checked_resources(game.read_bytes(),source,['romfs/world/podunk/podunk.encroom','romfs/system/native.encresources'],ROOT);room=parse_pack(metadata['romfs/world/podunk/podunk.encroom']);flags={room['strings'][row['name_string']]:i for i,row in enumerate(room['sections']['Flag'])};stable=['hp','pp','level','inventory_count','key_count','experience','cash','bank','earned_cash',*['story_flag_'+str(i)for i in range(6)],*['inventory_'+str(i)+'_'+key for i in range(16)for key in ('definition','doses','equipped')]]
r.CONFIG=r.CONFIG.replace('graphics_api=1','graphics_api=2')+'\n[System]\ninit_clock=1\ninit_clock\\default=false\ninit_time=946681277\ninit_time\\default=false\ninit_ticks_type=1\ninit_ticks_type\\default=false\ninit_ticks_override=0\ninit_ticks_override\\default=false\n'
route=json.loads(a.walk_plan.read_bytes());assert route['waypoints'][0]==[8,-39]and route['waypoints'][-1]==[1304,4665];shutil.copyfile(a.walk_plan,out/'walk-plan.json')
projections=[]
for repetition in (1,2):
 case=out/('cold-'+str(repetition));case.mkdir();work=case/'temporary';work.mkdir();marker=work/'.owned-emulator-workspace';marker.write_text('iggy-f029-teaching-v1');exe=r.portable(a.emulator.resolve(),work/'emulator')
 for source_file,dest in [(a.fixture,'sdmc/3ds/encore-native/save-1.encsave'),(a.dsp,'sdmc/3ds/dspfirm.cdc')]:
  target=exe.parent/'user'/dest;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source_file,target)
 with socket.socket()as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
 token=secrets.token_hex(24);startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0;process=client=session=None;records={};result={'status':'failed'}
 try:
  with(case/'stdout.log').open('wb')as log:process=subprocess.Popen([str(exe),'--cdb-port',str(port),'--cdb-token',token,'--cdb-allow-input',str(game)],cwd=exe.parent,stdout=log,stderr=subprocess.STDOUT,startupinfo=startup)
  for attempt in range(600):
   try:client=r.CdbClient(token,port);break
   except OSError:time.sleep(.1)
  assert client;caps=client.call('cdb.capabilities');assert not caps['memoryWriteEnabled']and caps['inputEnabled'];time.sleep(8);session=r.Session(client)
  def state(running=False):
   for attempt in range(4):
    try:return session.state()
    except ValueError as error:
     if 'mid-commit'not in str(error):raise
     if running:time.sleep(.01);continue
     try:session.advance(1)
     except ValueError as retry:
      if 'mid-commit'not in str(retry):raise
   raise RuntimeError('Persistent probe mid-commit')
  def clean(s):
   for name in ('house_error','field_error','menu_error','audio_error','battle_error'):assert not s[name],(name,s)
   return s
  def advance(frames):return clean(session.advance(frames))
  def wait(test,limit=2400):
   s=state()
   for _ in range(limit//8):
    if test(s):return clean(s)
    s=advance(8)
   raise TimeoutError(str(s))
  def pulse(button):client.call('input.set',{'buttons':[button]});advance(4);client.call('input.clear');return advance(15)
  def record(name):
   s=clean(state());records[name]=s;print(repetition,name,{k:s[k]for k in ('frame','title_phase','field_scene','world_stage','hp','player_x','player_y')},flush=True);return s
  def capture(name):
   job=client.call('screen.capture',{'screen':'combined'});event=client.event('screen.captured',jobId=job['jobId']);(case/(name+'.png')).write_bytes(base64.b64decode(event['data']))
  def flag(s,name):
   i=flags[name];return bool(s['story_flag_'+str(i//32)]&(1<<(i%32)))
  def projection(s):return {k:s[k]for k in stable}
  def goods(name):
   advance(60);before=record(name+'_before');pulse('START');wait(lambda s:s['menu_phase']==2 and s['paused']);pulse('A');wait(lambda s:s['submenu_phase']==1);shown=record(name+'_shown');assert shown['submenu_count']==before['inventory_count'];capture(name);pulse('B');wait(lambda s:s['submenu_phase']==0 and s['menu_phase']==2);pulse('B');wait(lambda s:s['menu_phase']==0 and not s['paused']);after=record(name+'_closed');assert projection(after)==projection(before);return after
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
  wait(lambda s:s['title_phase']==1);record('title');assert state()['frame']<2000
  while state()['frame']<2000:advance(min(120,2000-state()['frame']))
  pulse('DOWN');pulse('A');wait(lambda s:s['title_phase']==3);pulse('A');wait(lambda s:s['title_phase']==4);pulse('A');wait(lambda s:s['title_phase']==0 and not s['paused']);loaded=record('loaded');assert loaded['world_stage']==0 and loaded['world_dialogue']==0xffffffff
  i=flags['cem_roots_grown'];assert loaded['story_flag_'+str(i//32)]&(1<<(i%32));i=flags['bat'];assert not loaded['story_flag_'+str(i//32)]&(1<<(i%32))
  pulse('B');neutral=record('unlearned_B');assert projection(neutral)==projection(loaded)and neutral['title_phase']==0 and not neutral['crouch']and neutral['width']==400 and neutral['height']==240
  after_house=goods('house_goods');client.call('input.set',{'buttons':['DOWN']})
  try:wait(lambda s:s['field_scene']==2 and s['field_phase']==0 and not s['paused'],limit=3600)
  finally:client.call('input.clear')
  advance(60);outside=record('outside');capture('outside');assert projection(outside)==projection(after_house);after_field=goods('field_goods');assert after_field['audio_available']and after_field['region_music_voices']>0 and after_field['region_music_track']and not after_field['region_music_paused']
  # The static House voice is deliberately stopped at scene commit. Verify
  # the real field-region owner and DSP progress during normal execution.
  samples=[];client.call('input.clear');client.call('process.resume')
  try:
   time.sleep(1)
   for i in range(8):time.sleep(.15);sample=clean(state(running=True));samples.append(sample);records['region_audio_'+str(i)]=sample
  finally:client.call('process.pause')
  assert len({s['frame']for s in samples})>1 and any(s['dsp_region_playing']for s in samples)and len({(s['dsp_region_sample_sum'],s['dsp_region_wave_sum'])for s in samples})>1
  for i,(x,y)in enumerate(route['waypoints']):
   precise_point(x,y)
   now=state();assert now['world_stage']==0 and now['field_scene']==2 and now['field_phase']==0,('Unexpected source event during planned approach',i,now)
   if i%18==0:record('walk_'+str(i))
  record('iggy_approach');capture('iggy_approach')
  travel('DOWN',lambda s:s['world_programme']==6 and s['world_stage']!=0,limit=240)
  record('teaching_started');capture('teaching_started')
  choices=0;visits=[];last=None;flag_at=None;finish_frame=None
  for tick in range(2400):
   now=state();ident=now['world_dialogue']
   if ident!=0xffffffff and ident!=last:
    assert 3100<=ident<=3113,('Unexpected teaching text',now)
    last=ident;visits.append(ident);record('teaching_text_'+str(len(visits))+'_'+str(ident))
   if flag_at is None and flag(now,'bat'):flag_at=now['frame'];record('bat_flag_set')
   if now['world_stage']==0 and flag(now,'bat')and now['world_dialogue']==0xffffffff:
    finish_frame=now['frame'];break
   if now['choice_phase']==2:
    record('choice_'+str(choices));capture('choice_'+str(choices))
    if repetition==2 and choices==0:pulse('RIGHT');pulse('A')
    elif repetition==2 and choices==1:pulse('B')
    else:pulse('A')
    choices+=1
   elif now['dialogue_active']and(now['dialogue_finished']or now['dialogue_stopped']):pulse('A')
   else:advance(8)
  else:raise TimeoutError('Teaching did not finish '+str(state()))
  assert choices==(1 if repetition==1 else 3),(choices,visits)
  assert flag_at is not None and finish_frame-flag_at>=179,(flag_at,finish_frame)
  advance(15);completed=record('teaching_completed');capture('teaching_completed')
  assert completed['key_count']==loaded['key_count']+1 and completed['inventory_count']==loaded['inventory_count']
  pos=(completed['player_x'],completed['player_y']);assert abs(pos[0]-1432)<.01 and abs(pos[1]-4719)<.01
  client.call('input.set',{'buttons':['B']});advance(4);client.call('input.clear');capture('learned_bat')
  client.call('input.set',{'buttons':['RIGHT']});during=advance(4);client.call('input.clear')
  assert(during['player_x'],during['player_y'])==pos,('Learned Bat did not hold movement during ATTACK',during)
  advance(80);neutral=record('bat_finished');assert neutral['title_phase']==0 and neutral['width']==400 and neutral['height']==240 and neutral['key_count']==completed['key_count']
  client.call('input.set',{'buttons':['RIGHT']});moved=advance(8);client.call('input.clear');assert moved['player_x']>pos[0],('MOVE did not resume after Bat',moved)
  advance(10);frontier=record('frontier');capture('frontier');projections.append(projection(frontier))
  records['teaching_summary']=dict(choices=choices,text_visits=visits,flag_frame=flag_at,finish_frame=finish_frame,bat_movement_held=True,move_resumed=True)
  result=dict(status='passed',source_commit=source,game_sha256=identity['sha256'],fixture_sha256=r.sha(a.fixture),emulator_sha256=r.sha(a.emulator),projection=projections[-1],scope=__doc__,capabilities=dict(memoryWriteEnabled=False,inputEnabled=True))
 except Exception as error:
  result['error']=repr(error)
  if session:
   try:records['failure']=state();capture('failure')
   except Exception:pass
  raise
 finally:
  (case/'result.json').write_bytes((json.dumps(result,indent=2)+'\n').encode());(case/'observations.json').write_bytes((json.dumps(records,indent=2)+'\n').encode())
  if process:
   process.terminate()
   try:process.wait(timeout=15)
   except subprocess.TimeoutExpired:process.kill();process.wait(timeout=15)
  if client:client.close()
  logfile=exe.parent/'user/log/azahar_log.txt'
  if logfile.exists():shutil.copyfile(logfile,case/'emulator.log')
  assert marker.read_text()=='iggy-f029-teaching-v1'and work.resolve().is_relative_to(out);shutil.rmtree(work)
assert len(projections)==2 and projections[0]==projections[1];(out/'idempotence.json').write_bytes((json.dumps(dict(status='passed',repetitions=2,projection=projections[0],scope=__doc__),indent=2)+'\n').encode());print('Two ordinary teaching runs passed with identical gameplay result projection; owned profiles removed')
