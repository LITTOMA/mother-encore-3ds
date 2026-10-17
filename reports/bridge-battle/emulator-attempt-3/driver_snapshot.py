#!/usr/bin/env python3
"""Normal-input bridge regression; read-only observation, owned fixed-clock profile.
The checkpoint is artificial and does not claim the opening was played.
"""
import argparse,base64,json,re,secrets,shutil,socket,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_emulator_semantics as r
from tools.native_content import parse_pack
p=argparse.ArgumentParser();p.add_argument('--package',type=Path,required=True);p.add_argument('--fixture',type=Path,required=True);p.add_argument('--emulator',type=Path,required=True);p.add_argument('--dsp-component',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--clock',type=int,default=946681277);a=p.parse_args()
package=a.package.resolve();out=a.out.resolve();game=package/'3ds/encore-native/encore-native.3dsx'
receipt=json.loads((package/'build-receipt.json').read_bytes());source=receipt['source_commit']
if not re.fullmatch('[0-9a-f]{40}',source):raise ValueError('Exact package source required')
subprocess.run(['git','diff','--exit-code',source,'--','runtime','include','platform','romfs','content'],cwd=ROOT,check=True)
identity=receipt['artifacts']['encore-native.3dsx']
if game.stat().st_size!=identity['bytes']or r.sha(game)!=identity['sha256']:raise ValueError('Genuine artifact identity mismatch')
if out.exists()or not out.is_relative_to(ROOT/'build/emulator-regression'):raise ValueError('Fresh owned regression directory required')
if not a.fixture.is_file()or not a.fixture.resolve().is_relative_to(ROOT/'build')or not 0<a.fixture.stat().st_size<=1048576:raise ValueError('Bounded core-validated fixture required')
if shutil.disk_usage(ROOT).free<2*1024**3:raise ValueError('Insufficient free disk')
room=parse_pack((ROOT/'romfs/world/podunk/podunk.encroom').read_bytes());flags={room['strings'][f['name_string']]:i for i,f in enumerate(room['sections']['Flag'])}
def flag(s,name):i=flags[name];return bool(s['story_flag_'+str(i//32)]&(1<<(i%32)))
out.mkdir(parents=True);shutil.copyfile(__file__,out/'driver_snapshot.py');work=out/'temporary';work.mkdir();(work/'.owned-emulator-workspace').write_text('podunk-bridge-v1')
r.CONFIG=r.CONFIG.replace('graphics_api=1','graphics_api=2')+'\n[System]\ninit_clock=1\ninit_clock\\default=false\ninit_time='+str(a.clock)+'\ninit_time\\default=false\ninit_ticks_type=1\ninit_ticks_type\\default=false\ninit_ticks_override=0\ninit_ticks_override\\default=false\n'
exe=r.portable(a.emulator.resolve(),work/'emulator')
for source_file,dest in [(a.fixture,'sdmc/3ds/encore-native/save-1.encsave'),(a.dsp_component,'sdmc/3ds/dspfirm.cdc')]:
 target=exe.parent/'user'/dest;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source_file,target)
with socket.socket()as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
token=secrets.token_hex(24);startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
process=client=session=None;records={};result={'status':'failed'}
try:
 with(out/'stdout.log').open('wb')as log:process=subprocess.Popen([str(exe),'--cdb-port',str(port),'--cdb-token',token,'--cdb-allow-input',str(game)],cwd=exe.parent,stdout=log,stderr=subprocess.STDOUT,startupinfo=startup)
 for attempt in range(600):
  try:client=r.CdbClient(token,port);break
  except OSError:time.sleep(.1)
 assert client is not None
 caps=client.call('cdb.capabilities');assert caps['memoryWriteEnabled']is False and caps['inputEnabled']is True
 time.sleep(8);session=r.Session(client)
 def state():return session.state()
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
 wait(lambda s:s['title_phase']==1);record('title_ready')
 # Align normal inputs at a deterministic emulated frame, never alter guest state.
 assert state()['frame']<2000,state()
 while state()['frame']<2000:advance(min(120,2000-state()['frame']))
 pulse('DOWN');pulse('A');wait(lambda s:s['title_phase']==3);pulse('A');wait(lambda s:s['title_phase']==4);pulse('A');wait(lambda s:s['title_phase']==0 and s['paused']==0)
 initial=record('loaded');assert not flag(initial,'beat_dog_bridge')and not flag(initial,'pippis_mom_call');assert any(initial['inventory_'+str(i)+'_definition']==3 and initial['inventory_'+str(i)+'_equipped']for i in range(initial['inventory_count']))
 travel('DOWN',lambda s:s['field_scene']!=0 and s['field_phase']==0 and s['paused']==0);record('outside')
 travel('DOWN',lambda s:s['player_y']>=60);travel('LEFT',lambda s:s['player_x']<=-32);travel('DOWN',lambda s:s['player_y']>=190);travel('RIGHT',lambda s:s['player_x']>=314);travel('DOWN',lambda s:s['player_y']>=274)
 if state()['player_y']>278:travel('UP',lambda s:s['player_y']<=276)
 travel('RIGHT',lambda s:s['world_stage']!=0);record('bridge_trigger');capture('bridge-trigger')
 wait(lambda s:s['battle_active']and s['battle_round_phase']==1,limit=2400)
 battle=record('battle_commands');capture('battle-commands');assert battle['region_music_paused']and battle['dialogue_music_playing']and battle['dsp_dialogue_playing']
 music_before=(battle['dsp_dialogue_sample_pos'],battle['dsp_dialogue_wave_seq']);region_before=(battle['dsp_region_sample_sum'],battle['dsp_region_wave_sum']);advance(90);audio=record('battle_audio');assert music_before!=(audio['dsp_dialogue_sample_pos'],audio['dsp_dialogue_wave_seq']);assert region_before==(audio['dsp_region_sample_sum'],audio['dsp_region_wave_sum'])
 for turn in range(160):
  s=state()
  if not s['battle_active']:break
  assert s['battle_round_phase']!=6,('Fixture was defeated',s)
  pulse('A',settle=35)
 else:raise TimeoutError('Battle did not return '+str(state()))
 won=record('victory_return');capture('victory-return');assert flag(won,'beat_dog_bridge')and not flag(won,'pippis_mom_call');assert won['experience']==initial['experience']+8 and won['cash']==initial['cash'] and won['bank']==initial['bank']+20 and won['earned_cash']==initial['earned_cash']+20
 assert won['loot_ready']and won['loot_granted']==won['loot_selected'];assert won['inventory_count']==initial['inventory_count']+won['loot_selected'];assert not won['region_music_paused']and not won['dialogue_music_playing']
 music_before=(won['dsp_region_sample_sum'],won['dsp_region_wave_sum']);advance(120);audio=record('returned_audio');assert audio['dsp_region_playing']and music_before!=(audio['dsp_region_sample_sum'],audio['dsp_region_wave_sum'])
 wait(lambda s:s['world_stage']==0 and s['paused']==0,limit=1200)
 travel('RIGHT',lambda s:s['world_stage']!=0);record('mom_trigger');capture('mom-trigger')
 for text in range(1000,1005):
  wait(lambda s:s['world_dialogue']==text and s['dialogue_active']and s['dialogue_finished'],limit=1800)
  record('mom_'+str(text));advance(90);assert state()['world_dialogue']==text,('Dialogue advanced without input',state())
  pulse('A',settle=20)
 wait(lambda s:s['world_stage']==0 and s['paused']==0,limit=1200)
 called=record('mom_complete');assert flag(called,'pippis_mom_call')and flag(called,'beat_dog_bridge')
 # Walk beyond both original progress triggers by normal input.
 travel('RIGHT',lambda s:s['player_x']>=536)
 for button,axis,value in [('DOWN','player_y',480),('LEFT','player_x',408),('DOWN','player_y',488),('LEFT','player_x',400),('DOWN','player_y',512),('LEFT','player_x',344),('DOWN','player_y',520),('LEFT','player_x',336),('DOWN','player_y',528),('LEFT','player_x',184),('DOWN','player_y',536),('LEFT','player_x',176),('DOWN','player_y',560),('LEFT','player_x',-560),('UP','player_y',496)]:
  travel(button,lambda s,axis=axis,value=value,positive=button in ('RIGHT','DOWN'):s[axis]>=value if positive else s[axis]<=value)
 frontier=record('west_road');capture('west-road')
 result={'status':'passed','scope':'Artificial post-basement checkpoint, normal LOAD/door/walk/battle/dialogue input; no original pixel or whole-game claim','source_commit':receipt['source_commit'],'clock_fixture':a.clock,'fixture_sha256':r.sha(a.fixture),'emulator_sha256':r.sha(a.emulator),'capabilities':{'memoryWriteEnabled':caps['memoryWriteEnabled'],'inputEnabled':caps['inputEnabled']},'projection':{k:frontier[k]for k in ['hp','pp','level','experience','cash','bank','earned_cash','inventory_count','key_count','loot_selected',*['story_flag_'+str(i)for i in range(6)],*['inventory_'+str(i)+'_'+n for i in range(16)for n in ('definition','doses','equipped')]]}}
except BaseException as exc:
 result['error']=repr(exc)
 if session:
  try:records['failure']=state();capture('failure')
  except Exception:pass
 raise
finally:
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
