#!/usr/bin/env python3
import base64,json,re,secrets,shutil,socket,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_emulator_semantics as r
from tools.semantic_probe import projection

import argparse
p=argparse.ArgumentParser(description='Exercise Podunk bridge movement using normal input and a validated post-treats fixture.')
p.add_argument('--package',type=Path,required=True);p.add_argument('--fixture',type=Path,required=True)
p.add_argument('--emulator',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
p.add_argument('--locale',choices=('en','zh'),default='en');p.add_argument('--dsp-component',type=Path)
p.add_argument('--baseline',action='store_true',help='Capture the known old camera-placement defect')
a=p.parse_args();package=a.package.resolve();out=a.out.resolve();locale=a.locale
if out.exists() or not out.is_relative_to(ROOT/'build/emulator-regression'):raise ValueError('Fresh owned regression output required')
if not a.fixture.is_file() or not a.fixture.resolve().is_relative_to(ROOT/'build') or not 0<a.fixture.stat().st_size<=1024*1024:raise ValueError('Bounded shared-core fixture in build required')
receipt=json.loads((package/'build-receipt.json').read_bytes());game=package/'3ds/encore-native/encore-native.3dsx'
if not re.fullmatch('[0-9a-f]{40}',receipt['source_commit']):raise ValueError('Exact artifact source commit required')
if not a.emulator.is_file() or (a.dsp_component and not a.dsp_component.is_file()):raise ValueError('Explicit emulator/DSP file missing')
identity=receipt['artifacts']['encore-native.3dsx']
if game.stat().st_size!=identity['bytes'] or r.sha(game)!=identity['sha256']:raise ValueError('Genuine artifact receipt mismatch')
# The negative run deliberately executes an older, verified console build.
paths=['runtime','include','platform','content/native-session.json','content/native-house.json','romfs/world/house','romfs/world/podunk/podunk.encroom','romfs/world/world.enclinks'] if a.baseline else ['runtime','include','platform','romfs','content']
subprocess.run(['git','diff','--exit-code',receipt['source_commit'],'--',*paths],cwd=ROOT,check=True)
if shutil.disk_usage(ROOT).free<2*1024**3:raise ValueError('At least 2 GiB free disk required')
out.mkdir(parents=True);shutil.copyfile(__file__,out/'driver_snapshot.py')
work=out/'temporary';work.mkdir();(work/'.owned-emulator-workspace').write_text('podunk-camera-test-v1')
r.CONFIG=r.CONFIG.replace('graphics_api=1','graphics_api=2');exe=r.portable(a.emulator.resolve(),work/'emulator')
p=exe.parent/'user/sdmc/3ds/encore-native/save-1.encsave';p.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(a.fixture,p)
if a.dsp_component:
    p=exe.parent/'user/sdmc/3ds/dspfirm.cdc';p.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(a.dsp_component,p)
with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
token=secrets.token_hex(24);startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
process=client=session=None;records={};result={'status':'failed'}
try:
    with (out/'stdout.log').open('wb') as log:process=subprocess.Popen([str(exe),'--cdb-port',str(port),'--cdb-token',token,'--cdb-allow-input',str(game)],cwd=exe.parent,stdout=log,stderr=subprocess.STDOUT,startupinfo=startup)
    for attempt in range(600):
        try:client=r.CdbClient(token,port);break
        except OSError:time.sleep(.1)
    assert client is not None
    caps=client.call('cdb.capabilities');assert caps['memoryWriteEnabled'] is False and caps['inputEnabled'] is True
    time.sleep(10);session=r.Session(client);client.call('process.resume')
    def state():
        for attempt in range(50):
            try:return session.state()
            except ValueError as e:
                if 'mid-commit' not in str(e):raise
                time.sleep(.01)
        raise RuntimeError('Probe remained mid-commit')
    def wait(predicate,timeout=90):
        stop=time.monotonic()+timeout;observed=state()
        while not predicate(observed):
            if time.monotonic()>stop:raise TimeoutError(str(observed))
            time.sleep(.05);observed=state()
        return observed
    def record(name):
        observed=state();records[name]=observed;print(name,projection(observed,['field_scene','field_phase','player_x','player_y','mick_x','mick_y','mick_phase','dialogue_stopped','dialogue_finished','dialogue_visible']),flush=True);return observed
    def capture(name):
        job=client.call('screen.capture',{'screen':'combined'});event=client.event('screen.captured',jobId=job['jobId']);data=base64.b64decode(event['data']);assert len(data)<=2*1024*1024;(out/(name+'.png')).write_bytes(data)
    def pulse(button,duration=.08,settle=.15):
        client.call('input.set',{'buttons':[button],'circle':{'x':0,'y':0,'override':True}})
        time.sleep(duration);client.call('input.clear');time.sleep(settle)
    def idle(s):return s['house_phase']==0 and s['paused']==0 and s['dialogue_active']==0
    def clean(s):
        assert not s['house_error'] and not s['menu_error'] and not s['field_error'],s
    def travel(button,predicate,limit=1200,allow_boundary=False):
        client.call('process.pause')
        try:
            client.call('input.set',{'buttons':[button]})
            previous=None;unchanged=0
            for tick in range(limit):
                s=session.advance(1)
                position=(s['player_x'],s['player_y'])
                unchanged=unchanged+1 if position==previous else 0;previous=position
                if tick%120==0:print('travel',button,tick,position,flush=True)
                if predicate(s):break
                if unchanged>=90:raise TimeoutError('Blocked '+button+': '+str(s))
            else:raise TimeoutError('Movement '+button+': '+str(s))
            client.call('input.clear');session.advance(2)
        finally:
            client.call('input.clear');client.call('process.resume')
        time.sleep(.15)
        if not allow_boundary:clean(state())
    wait(lambda s:s['title_phase']==1)
    if locale=='zh':pulse('R',settle=2)
    pulse('DOWN');pulse('A');wait(lambda s:s['title_phase']==3)
    pulse('A');wait(lambda s:s['title_phase']==4)
    pulse('A',settle=1);wait(lambda s:s['title_phase']==0 and s['paused']==0)
    record('loaded');pulse('DOWN',duration=.9,settle=1)
    wait(lambda s:s['field_scene']!=0 and s['field_phase']==0 and s['paused']==0)
    record('outside');capture('outside')
    travel('DOWN',lambda s:s['player_y']>=60)
    travel('LEFT',lambda s:s['player_x']<=-32)
    travel('DOWN',lambda s:s['player_y']>=190)
    record('south_of_yard');capture('south_of_yard')
    travel('RIGHT',lambda s:s['player_x']>=314)
    travel('DOWN',lambda s:s['player_y']>=274)
    record('bridge_approach');capture('bridge_approach')
    # The wall-clock entry pulse can leave this approach four pixels farther
    # south. At y=282 the original bridge railing blocks x=411; source-derived
    # collision admits the short UP correction before the unchanged crossing.
    # Steer through the walkable lane with normal input, never warp the player.
    if state()['player_y']>278:
        travel('UP',lambda s:s['player_y']<=276)
    travel('RIGHT',lambda s:s['player_x']>=351)
    crossing=record('bridge_crossed');capture('bridge_crossed')
    assert crossing['field_phase']==0 and crossing['paused']==0
    travel('RIGHT',lambda s:s['field_phase']!=0,allow_boundary=True)
    frontier=record('bridge_story_boundary');capture('bridge_story_boundary')
    assert 440<=frontier['player_x']<=465 and frontier['paused']==1 and frontier['field_phase']==3
    # ABI v6 field_error covers loading failures; source boundary text is
    # rendered directly from FieldScene::error and is not on that wire field.
    pulse('B');wait(lambda s:s['field_phase']==0 and s['paused']==0)
    for _ in range(2):
        travel('LEFT',lambda s:s['player_x']<=314)
        travel('RIGHT',lambda s:s['player_x']>=351)
    final=record('bridge_repeat');capture('bridge_repeat')
    keys=['field_scene','field_phase','paused','inventory_count','key_count','hp','pp','object_flag_count']
    assert projection(crossing,keys)==projection(final,keys)
    result={'status':'passed','baseline_capture':a.baseline,'projection':projection(final,keys),'source_commit':receipt['source_commit'],'memory_write_enabled':False,'driver_sha256':r.sha(out/'driver_snapshot.py'),'fixture_sha256':r.sha(a.fixture),'emulator_sha256':r.sha(a.emulator),'game_sha256':identity['sha256'],'scope':'Artificial shared-core validated post-treats House-exit checkpoint. Normal LOAD and movement; bridge repeated twice; original not-yet-ported bridge story boundary stays closed and B returns. Screenshots are visual evidence, not a pixel oracle; exact camera origin is separately checked by shared core tests. No natural opening or hardware claim.'}

finally:
    if client:
        try:client.call('input.clear')
        except Exception:pass
        client.close()
    if process and process.poll() is None:
        process.terminate()
        try:process.wait(timeout=10)
        except subprocess.TimeoutExpired:process.kill();process.wait()
    log=work/'emulator/user/log/azahar_log.txt'
    if log.is_file():shutil.copyfile(log,out/'emulator.log')
    (out/'observations.json').write_text(json.dumps(records,indent=2),encoding='utf8');(out/'result.json').write_text(json.dumps(result,indent=2),encoding='utf8')
    assert work.resolve().parent==out.resolve() and (work/'.owned-emulator-workspace').is_file()
    shutil.rmtree(work)
print('PASSED',result['status'],flush=True)
