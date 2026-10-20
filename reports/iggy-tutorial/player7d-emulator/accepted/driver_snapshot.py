"""Actual recorded save -> ordinary LOAD/B/Goods/door/Goods, two cold runs.
Read-only probe and ordinary button input only; no natural-opening/full-game claim.
"""
from pathlib import Path
import argparse,base64,hashlib,json,secrets,shutil,socket,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT))
from tools import run_emulator_semantics as r
from tools.emulator_artifact import checked_resources
from tools.native_content import parse_pack
p=argparse.ArgumentParser();p.add_argument('--package',type=Path,required=True);p.add_argument('--fixture',type=Path,required=True);p.add_argument('--emulator',type=Path,required=True);p.add_argument('--dsp',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
package=a.package.resolve();game=package/'3ds/encore-native/encore-native.3dsx';receipt=json.loads((package/'build-receipt.json').read_bytes());source=receipt['source_commit'];assert source=='7d2e552f86c9ef0c995c85c301e3a2f26a78f32a';identity=receipt['artifacts']['encore-native.3dsx'];assert game.stat().st_size==identity['bytes']and r.sha(game)==identity['sha256']
assert a.fixture.resolve().is_relative_to(ROOT/'build')and r.sha(a.fixture)=='2ec58ed5e5e9e03f9cf9832346fc31d6da77ccb80e21953f4c833c4ca67c1714'
out=a.out.resolve();assert out.is_relative_to(ROOT/'build/emulator-regression')and not out.exists();out.mkdir(parents=True);shutil.copyfile(__file__,out/'driver_snapshot.py')
metadata=checked_resources(game.read_bytes(),source,['romfs/world/podunk/podunk.encroom','romfs/system/native.encresources'],ROOT);room=parse_pack(metadata['romfs/world/podunk/podunk.encroom']);flags={room['strings'][row['name_string']]:i for i,row in enumerate(room['sections']['Flag'])};stable=['hp','pp','level','inventory_count','key_count','experience','cash','bank','earned_cash',*['story_flag_'+str(i)for i in range(6)],*['inventory_'+str(i)+'_'+key for i in range(16)for key in ('definition','doses','equipped')]]
r.CONFIG=r.CONFIG.replace('graphics_api=1','graphics_api=2')+'\n[System]\ninit_clock=1\ninit_clock\\default=false\ninit_time=946681277\ninit_time\\default=false\ninit_ticks_type=1\ninit_ticks_type\\default=false\ninit_ticks_override=0\ninit_ticks_override\\default=false\n'
projections=[]
for repetition in (1,2):
 case=out/('cold-'+str(repetition));case.mkdir();work=case/'temporary';work.mkdir();marker=work/'.owned-emulator-workspace';marker.write_text('player7d-baseline-v1');exe=r.portable(a.emulator.resolve(),work/'emulator')
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
  def projection(s):return {k:s[k]for k in stable}
  def goods(name):
   advance(60);before=record(name+'_before');pulse('START');wait(lambda s:s['menu_phase']==2 and s['paused']);pulse('A');wait(lambda s:s['submenu_phase']==1);shown=record(name+'_shown');assert shown['submenu_count']==before['inventory_count'];capture(name);pulse('B');wait(lambda s:s['submenu_phase']==0 and s['menu_phase']==2);pulse('B');wait(lambda s:s['menu_phase']==0 and not s['paused']);after=record(name+'_closed');assert projection(after)==projection(before);return after
  wait(lambda s:s['title_phase']==1);record('title');assert state()['frame']<2000
  while state()['frame']<2000:advance(min(120,2000-state()['frame']))
  pulse('DOWN');pulse('A');wait(lambda s:s['title_phase']==3);pulse('A');wait(lambda s:s['title_phase']==4);pulse('A');wait(lambda s:s['title_phase']==0 and not s['paused']);loaded=record('loaded');assert loaded['world_stage']==0 and loaded['world_dialogue']==0xffffffff
  i=flags['cem_roots_grown'];assert loaded['story_flag_'+str(i//32)]&(1<<(i%32));i=flags['bat'];assert not loaded['story_flag_'+str(i//32)]&(1<<(i%32))
  pulse('B');neutral=record('unlearned_B');assert projection(neutral)==projection(loaded)and neutral['title_phase']==0 and not neutral['crouch']and neutral['width']==400 and neutral['height']==240
  after_house=goods('house_goods');client.call('input.set',{'buttons':['DOWN']})
  try:wait(lambda s:s['field_scene']==2 and s['field_phase']==0 and not s['paused'],limit=3600)
  finally:client.call('input.clear')
  advance(60);outside=record('outside');capture('outside');assert projection(outside)==projection(after_house);after_field=goods('field_goods');projections.append(projection(after_field));assert after_field['audio_available']and after_field['region_music_voices']>0 and after_field['region_music_track']and not after_field['region_music_paused']
  # The static House voice is deliberately stopped at scene commit. Verify
  # the real field-region owner and DSP progress during normal execution.
  samples=[];client.call('input.clear');client.call('process.resume')
  try:
   time.sleep(1)
   for i in range(8):time.sleep(.15);sample=clean(state(running=True));samples.append(sample);records['region_audio_'+str(i)]=sample
  finally:client.call('process.pause')
  assert len({s['frame']for s in samples})>1 and any(s['dsp_region_playing']for s in samples)and len({(s['dsp_region_sample_sum'],s['dsp_region_wave_sum'])for s in samples})>1
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
  assert marker.read_text()=='player7d-baseline-v1'and work.resolve().is_relative_to(out);shutil.rmtree(work)
assert len(projections)==2 and projections[0]==projections[1];(out/'idempotence.json').write_bytes((json.dumps(dict(status='passed',repetitions=2,projection=projections[0],scope=__doc__),indent=2)+'\n').encode());print('Two cold baseline runs passed with identical result projection; owned profiles removed')
