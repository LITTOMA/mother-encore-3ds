#!/usr/bin/env python3
import base64,json,re,secrets,shutil,socket,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_emulator_semantics as r
from tools.semantic_probe import projection

import argparse
p=argparse.ArgumentParser(description='Exercise basement bat/diary, field Lifeup and persistence using ordinary emulator input.')
p.add_argument('--package',type=Path,required=True);p.add_argument('--fixture',type=Path,required=True)
p.add_argument('--emulator',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
p.add_argument('--locale',choices=('en','zh'),default='en');p.add_argument('--dsp-component',type=Path)
p.add_argument('--restored',action='store_true',help='Cold-boot verification of the actual Record output')
a=p.parse_args();package=a.package.resolve();out=a.out.resolve();locale=a.locale
if out.exists() or not out.is_relative_to(ROOT/'build/emulator-regression'):raise ValueError('Fresh owned regression output required')
if not a.fixture.is_file() or not a.fixture.resolve().is_relative_to(ROOT/'build') or not 0<a.fixture.stat().st_size<=1024*1024:raise ValueError('Bounded shared-core fixture in build required')
receipt=json.loads((package/'build-receipt.json').read_bytes());game=package/'3ds/encore-native/encore-native.3dsx'
if not re.fullmatch('[0-9a-f]{40}',receipt['source_commit']):raise ValueError('Exact artifact source commit required')
if not a.emulator.is_file() or (a.dsp_component and not a.dsp_component.is_file()):raise ValueError('Explicit emulator/DSP file missing')
identity=receipt['artifacts']['encore-native.3dsx']
if game.stat().st_size!=identity['bytes'] or r.sha(game)!=identity['sha256']:raise ValueError('Genuine artifact receipt mismatch')
# The negative run deliberately executes an older, verified console build.
paths=['runtime','include','platform','romfs','content']
subprocess.run(['git','diff','--exit-code',receipt['source_commit'],'--',*paths],cwd=ROOT,check=True)
if shutil.disk_usage(ROOT).free<2*1024**3:raise ValueError('At least 2 GiB free disk required')
out.mkdir(parents=True);shutil.copyfile(__file__,out/'driver_snapshot.py')
work=out/'temporary';work.mkdir();(work/'.owned-emulator-workspace').write_text('basement-test-v1')
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
    def travel(button,predicate,limit=1200):
        client.call('process.pause')
        try:
            client.call('input.set',{'buttons':[button]})
            for _ in range(limit):
                s=session.advance(1)
                if predicate(s):break
            else:raise TimeoutError('Movement '+button+': '+str(s))
            client.call('input.clear');session.advance(2)
        finally:
            client.call('input.clear');client.call('process.resume')
        time.sleep(.15);clean(state())
    def converse(name,stop=idle,timeout=150):
        deadline=time.monotonic()+timeout;seen=set()
        while True:
            s=state();clean(s)
            if stop(s):return record(name+'_done')
            signature=(s['house_phase'],s['world_phrase'],s['dialogue_visible'])
            if signature not in seen:
                seen.add(signature)
                if len(seen)<64:records[name+'_'+str(len(seen))]=s
            if s['dialogue_active'] and (s['dialogue_finished'] or s['dialogue_stopped']):pulse('A',settle=.4)
            else:time.sleep(.1)
            if time.monotonic()>deadline:raise TimeoutError(name+': '+str(s))
    def open_psi():
        pulse('START',settle=.6);wait(lambda s:s['menu_phase']==2)
        pulse('RIGHT');assert state()['menu_command']==1
        pulse('A',settle=.6);wait(lambda s:s['menu_phase']==9)
        assert state()['psi_learned']==1
        pulse('DOWN');assert state()['psi_learned']==1
    def close_psi():
        pulse('B',settle=.6);wait(lambda s:s['menu_phase']==2)
        pulse('B',settle=.6);wait(lambda s:s['menu_phase']==0 and s['paused']==0)
    wait(lambda s:s['title_phase']==1)
    if locale=='zh':pulse('R',settle=2)
    pulse('DOWN');pulse('A');wait(lambda s:s['title_phase']==3)
    pulse('A');wait(lambda s:s['title_phase']==4)
    pulse('A',settle=1);wait(lambda s:s['title_phase']==0 and s['paused']==0)
    start=record('loaded');clean(start)
    if a.restored:
        assert start['level']==2 and start['inventory_count']==2 and start['key_count']==3 and start['pp']==22
        assert start['present_opened_mask']==3 and start['object_flag_count']==1
        open_psi();pulse('A');wait(lambda s:s['menu_phase']==11)
        pulse('B');wait(lambda s:s['menu_phase']==9);assert state()['pp']==22
        capture('restored_lifeup');close_psi()
        result={'status':'passed','locale':locale,'restored':True,'restored_projection':projection(state(),['inventory_count','key_count','present_opened_mask','object_flag_count','hp','pp'])}
    else:
        assert start['player_x']==88 and start['player_y']==977 and start['inventory_count']==1 and start['key_count']==2
        travel('DOWN',lambda s:s['player_y']>=1057)
        travel('RIGHT',lambda s:s['player_x']>=160)
        pulse('DOWN',duration=.025);pulse('A')
        wait(lambda s:s['house_phase']==13)
        bat=converse('bat_received');capture('bat_received')
        assert bat['inventory_count']==2 and bat['present_opened_mask']==2 and bat['object_flag_count']==1
        pulse('A');wait(lambda s:s['house_phase']==13);bat2=converse('bat_repeat')
        assert bat2['inventory_count']==2 and bat2['key_count']==2
        travel('UP',lambda s:s['player_y']<=1033)
        travel('LEFT',lambda s:s['player_x']<=128)
        pulse('UP',duration=.025);pulse('A')
        wait(lambda s:s['house_phase'] in (7,13))
        diary=converse('diary',timeout=180);capture('diary_completed')
        assert diary['inventory_count']==2 and diary['key_count']==3 and diary['present_opened_mask']==3
        assert abs(diary['player_x']-128)<.1 and abs(diary['player_y']-1040)<.1
        assert diary['music_playing']==1
        open_psi();capture('lifeup_learned')
        before=state()['pp'];assert before==27
        pulse('A');wait(lambda s:s['menu_phase']==11);time.sleep(.5);assert state()['pp']==before
        pulse('B');wait(lambda s:s['menu_phase']==9);assert state()['pp']==before
        pulse('A');wait(lambda s:s['menu_phase']==11);pulse('A')
        used=wait(lambda s:s['menu_phase']==12);assert used['pp']==before-5 and used['hp']==65
        time.sleep(1);assert state()['pp']==before-5;capture('lifeup_result');record('lifeup_used')
        pulse('A');wait(lambda s:s['menu_phase']==9);close_psi()
        pulse('START',settle=.6);wait(lambda s:s['menu_phase']==2)
        pulse('DOWN');assert state()['menu_command']==2
        pulse('A',settle=.6);wait(lambda s:s['menu_phase']==4)
        pulse('A');wait(lambda s:s['menu_phase']==5);capture('bat_equipment_candidate')
        pulse('A');wait(lambda s:s['menu_phase']==4);record('bat_equipped');capture('bat_equipped')
        pulse('B',settle=.6);pulse('B',settle=.6);wait(lambda s:s['menu_phase']==0 and s['paused']==0)
        # The diary has its own repeat branch; approach again after its source movement.
        pulse('UP',duration=.025);pulse('A');wait(lambda s:s['house_phase']==13)
        repeated=converse('diary_repeat');assert repeated['key_count']==3 and repeated['pp']==22
        travel('LEFT',lambda s:s['player_x']<=88)
        travel('UP',lambda s:s['player_y']<900 and s['house_phase']==0)
        travel('DOWN',lambda s:s['player_y']>=735)
        travel('RIGHT',lambda s:s['player_x']>=148)
        travel('UP',lambda s:s['player_y']<=713)
        pulse('A');wait(lambda s:s['house_phase']==7)
        converse('dad_record_choice',stop=lambda s:s['choice_phase']==2)
        pulse('A');wait(lambda s:s['save_phase']==2)
        pulse('A');wait(lambda s:s['save_phase']==4)
        pulse('A');wait(lambda s:s['save_phase']==2)
        save=exe.parent/'user/sdmc/3ds/encore-native/save-1.encsave'
        assert save.read_bytes()!=a.fixture.read_bytes()
        shutil.copyfile(save,out/'record.encsave');capture('record_written');record('record_written')
        pulse('B');converse('dad_finished')
        result={'status':'passed','locale':locale,'restored':False,'projection':projection(state(),['inventory_count','key_count','present_opened_mask','object_flag_count','hp','pp']),'record_sha256':r.sha(out/'record.encsave'),'repeat_does_not_duplicate':True,'target_cancel_preserves_pp':True,'one_use_costs_five_pp':True}
    result.update(source_commit=receipt['source_commit'],memory_write_enabled=False,driver_sha256=r.sha(out/'driver_snapshot.py'),fixture_sha256=r.sha(a.fixture),emulator_sha256=r.sha(a.emulator),game_sha256=identity['sha256'],scope='Explicit shared-core validated level-2 basement checkpoint; normal LOAD, movement, A, START, target cancellation, healing, equipment and Dad Record. Separate cold boot consumes the actual Record output. No naturally-played whole opening or hardware claim.')

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
print('PASSED',locale,result['status'],flush=True)
