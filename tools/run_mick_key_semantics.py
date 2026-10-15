#!/usr/bin/env python3
import base64,json,secrets,shutil,socket,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_emulator_semantics as r
from tools.semantic_probe import projection

import argparse
p=argparse.ArgumentParser(description='Exercise actual Mick key delivery using normal input and a validated no-key level-2 fixture.')
p.add_argument('--package',type=Path,required=True);p.add_argument('--fixture',type=Path,required=True)
p.add_argument('--emulator',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
p.add_argument('--locale',choices=('en','zh'),default='en');p.add_argument('--dsp-component',type=Path)
p.add_argument('--expect-grant-rejection',action='store_true',help='Negative reproduction of the old missing-host build only')
a=p.parse_args();package=a.package.resolve();out=a.out.resolve();locale=a.locale
if out.exists() or not out.is_relative_to(ROOT/'build/emulator-regression'):raise ValueError('Fresh owned regression output required')
if not a.fixture.is_file() or not a.fixture.resolve().is_relative_to(ROOT/'build') or not 0<a.fixture.stat().st_size<=1024*1024:raise ValueError('Bounded shared-core fixture in build required')
receipt=json.loads((package/'build-receipt.json').read_bytes());game=package/'3ds/encore-native/encore-native.3dsx'
identity=receipt['artifacts']['encore-native.3dsx']
if game.stat().st_size!=identity['bytes'] or r.sha(game)!=identity['sha256']:raise ValueError('Genuine artifact receipt mismatch')
# The negative run deliberately executes an older, verified console build.
paths=['romfs','content'] if a.expect_grant_rejection else ['runtime','include','platform','romfs','content']
subprocess.run(['git','diff','--exit-code',receipt['source_commit'],'--',*paths],cwd=ROOT,check=True)
if shutil.disk_usage(ROOT).free<2*1024**3:raise ValueError('At least 2 GiB free disk required')
out.mkdir(parents=True);work=out/'temporary';work.mkdir();(work/'.owned-emulator-workspace').write_text('mick-key-test-v1')
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
    def hold_stable(name,button=None):
        before=record(name+'_before');assert before['dialogue_stopped']==1 and before['dialogue_finished']==0
        if button:client.call('input.set',{'buttons':[button]})
        time.sleep(3)
        client.call('input.clear');after=record(name+'_after')
        fields=['dialogue_active','dialogue_stopped','dialogue_finished','dialogue_visible','mick_phase','key_count','player_x','player_y']
        assert projection(before,fields)==projection(after,fields)
    def page(name):
        wait(lambda s:s['dialogue_stopped']==1);hold_stable(name)
        if name.startswith('key_'):capture(name)
        # One fresh A must only release this gate. Keeping it down cannot
        # release the next one. The next page may still be typing on release.
        old=state()['dialogue_visible'];client.call('input.set',{'buttons':['A']});time.sleep(3)
        after=state();assert after['dialogue_visible']>old
        client.call('input.clear');time.sleep(.1)
    wait(lambda s:s['title_phase']==1)
    if locale=='zh':pulse('R',settle=2)
    pulse('DOWN');pulse('A');wait(lambda s:s['title_phase']==3)
    pulse('A');wait(lambda s:s['title_phase']==4)
    pulse('A',settle=1)
    wait(lambda s:s['title_phase']==0 and s['paused']==0)
    record('loaded');pulse('DOWN',duration=.9,settle=1)
    wait(lambda s:s['field_scene']!=0 and s['field_phase']==0)
    record('outside')
    # Move below the front door, then left along the yard to Mick, without
    # invoking the gated south-fence event. Source player speed is 64 px/s.
    client.call('input.set',{'buttons':['DOWN']});wait(lambda s:s['player_y']>=20);client.call('input.clear')
    client.call('input.set',{'buttons':['LEFT']});wait(lambda s:s['player_x']<=s['mick_x']);client.call('input.clear')
    pulse('UP',duration=.05);record('near_mick');pulse('A')
    wait(lambda s:s['mick_phase']==2 and s['dialogue_stopped']==1)
    page('woof_0_wait_1');page('woof_0_wait_2')
    wait(lambda s:s['dialogue_finished']==1 and s['mick_phase']==2)
    before=record('choices_ready');time.sleep(2);assert state()['dialogue_visible']==before['dialogue_visible']
    pulse('A');wait(lambda s:s['mick_phase']==1)
    wait(lambda s:s['dialogue_finished']==1);pulse('A') # scratch narration
    wait(lambda s:s['dialogue_stopped']==1)
    page('woof_4_wait_1');page('woof_4_wait_2');page('woof_4_wait_3')
    wait(lambda s:s['dialogue_finished']==1);pulse('A')
    wait(lambda s:s['dialogue_finished']==1);pulse('A')
    wait(lambda s:s['field_phase']==0 and s['paused']==0 and s['dialogue_active']==0)
    before=record('scratch_finished');assert before['level']==2 and before['psi_learned']==1 and before['key_count']==1
    # Player still faces Mick. Native source shortcut: hold B, then press A.
    client.call('input.set',{'buttons':['B']});time.sleep(.25)
    client.call('input.set',{'buttons':['B','A']});time.sleep(.1);client.call('input.clear')
    wait(lambda s:s['mick_phase']==1 and s['dialogue_stopped']==1)
    page('key_wait_1');page('key_wait_2')
    wait(lambda s:s['dialogue_finished']==1);record('before_grant');pulse('A')
    if a.expect_grant_rejection:
        rejected=wait(lambda s:s['mick_phase']==5)
        assert rejected['key_count']==before['key_count'] and rejected['field_error']=='Mick key grant host is unsupported'
        capture('missing_host');record('missing_host')
        result={'status':'passed_negative_reproduction','locale':locale,'source_commit':receipt['source_commit'],'actual_error':rejected['field_error']}
    else:
        granted=wait(lambda s:s['key_count']==before['key_count']+1)
        assert granted['field_error']=='';record('key_granted');capture('key_granted')
        # Receipt narration, Mick follow-up and final flag-setting phrase.
        for n in range(3):
            wait(lambda s:s['dialogue_finished']==1);pulse('A')
        final=wait(lambda s:s['field_phase']==0 and s['paused']==0 and s['dialogue_active']==0)
        assert final['key_count']==2;record('key_conversation_finished')
        # Completion flag must select food thoughts and never award another key.
        client.call('input.set',{'buttons':['B']});time.sleep(.25)
        client.call('input.set',{'buttons':['B','A']});time.sleep(.1);client.call('input.clear')
        wait(lambda s:s['mick_phase']==1 and s['dialogue_finished']==1);pulse('A')
        final=wait(lambda s:s['field_phase']==0 and s['paused']==0 and s['dialogue_active']==0)
        assert final['key_count']==2 and final['field_error']==''
        record('repeat_thought_finished');capture('returned_to_field')
        result={'status':'passed','locale':locale,'projection':{'key_count_before':before['key_count'],'key_count_after':final['key_count'],'repeat_does_not_duplicate':True,'field_phase':final['field_phase'],'paused':final['paused']},'source_commit':receipt['source_commit']}
    result.update(memory_write_enabled=False,fixture_sha256=r.sha(a.fixture),emulator_sha256=r.sha(a.emulator),game_sha256=identity['sha256'],scope='Artificial shared-core validated level-2 House exit with no basement key; normal LOAD, scratch conversation, Telepathy and repeated Telepathy. Not naturally-played opening or hardware verification.')

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
