#!/usr/bin/env python3
"""Source-owned global Slowmo / MouseHider lifecycle policies, no scene execution."""
from __future__ import annotations
import argparse,hashlib,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor
IR=ROOT/'content/native-global-child-ready.json';REVIEW=ROOT/'reports/global-child-ready/source-review.json'
ENGINE=ROOT/'reports/global-child-ready/engine-source.json';PACK=ROOT/'romfs/data/global.encchildready'
CTOR=ROOT/'content/native-field-global-constructor.json';FAMILY=0x454e0057
SLOW='Scripts/global/Slowmo.gd';MOUSE='Scripts/UI/mouse_hider.gd';AUDIO='Scripts/global/audioManager.gd'
def engine_export(directory):
    files=read(directory/'files.json');require(len(files)==4,'Incomplete original native engine source')
    source={x['path']:(directory/x['path'].replace('/','-')).read_bytes()for x in files}
    for x in files:require(hashlib.sha256(source[x['path']]).hexdigest()==x['sha256']and '3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'in x['url'],'Changed fixed official engine source')
    engine=source['core/engine.cpp'].decode();input=source['core/os/input.h'].decode();loop=source['core/os/main_loop.h'].decode();backend=source['main/input_default.cpp'].decode()
    require('void Engine::set_time_scale(float p_scale) {\n\t_time_scale = p_scale;\n}'in engine,'Unknown native time scale write')
    modes=re.search(r'enum MouseMode \{(.*?)\};',input,re.S)[1];names=[x.strip().strip(',')for x in modes.splitlines()if x.strip()]
    require(names[:2]==['MOUSE_MODE_VISIBLE','MOUSE_MODE_HIDDEN'],'Unknown native mouse enums')
    event=int(re.search(r'NOTIFICATION_WM_MOUSE_ENTER = (\d+)',loop)[1])
    require('mouse_button_mask = 0;'in backend and 'emulate_mouse_from_touch = false;'in backend and 'Point2 InputDefault::get_last_mouse_speed()'in backend,'Unknown mouse-less native input constructor')
    write(ENGINE,dict(schema=1,engine_commit='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8',files=files,visible=names.index('MOUSE_MODE_VISIBLE'),hidden=names.index('MOUSE_MODE_HIDDEN'),mouse_enter=event,engine_initial_scale=float(re.search(r'_time_scale = (\d+\.\d+);',engine)[1]),review=['Engine stores float time scale without clamp; actual scheduler must read same owner','InputDefault mouse button mask and speed tracker begin empty; mouse speed updates only from native mouse events','3DS backend has no physical mouse; lower touch gesture is not a synthesized mouse event and remains separate','Node source process/input automatic activation precedes script Ready; Slowmo Ready disables idle only','WM mouse-enter notification is a native source notification; no 3DS window enter event is fabricated']))
def derive():
    ex=Extractor(ROOT);ctor=read(CTOR);engine=read(ENGINE)
    require(ctor['commit']==PIN and engine['schema']==1,'Changed global/native source identity')
    texts={p:ex.text(p)for p in [SLOW,MOUSE,AUDIO,'Scripts/global/global.tscn','Scripts/global/Slowmo.tscn','Scripts/global/MouseHider.tscn']}
    expected={SLOW:['_ready','start_slowmo','_process','circl_ease_in'],MOUSE:['_ready','_notification','_process','_input','_manage_mouse','set_active']}
    nodes=[]
    for kind,script in [(1,SLOW),(2,MOUSE)]:
        body=texts[script];methods=re.findall(r'^func (\w+)\(',body,re.M);require(methods==expected[script],'Unknown original child callback/method')
        row=next(x for x in ctor['recipe']['records']if x['script']==script)
        fields=next(x for x in ctor['child_defaults']if x['id']==row['id'])
        require(ctor['recipe']['classes'][row['class_index']]=='Node'and len(fields['fields'])==4,'Wrong actual child native class/declarations')
        nodes.append(dict(kind=kind,id=row['id'],class_index=row['class_index'],methods=row['script_methods'],path=row['node'],script=script,script_sha256=ex.sources[script],fields=[f['name']for f in fields['fields']],methods_source=methods,functions=[dict(method=m,sha256=hashlib.sha256(re.search(r'^func '+m+r'\(.*?(?=^func |\Z)',body,re.S|re.M)[0].encode()).hexdigest())for m in methods]))
    slow=texts[SLOW];mouse=texts[MOUSE]
    require('set_process(false)\n\tEngine.time_scale = 1'in slow and 'start_time = Time.get_ticks_msec()'in slow and 'Engine.time_scale = start_value\n\tset_process(true)'in slow,'Unknown actual Slowmo start/Ready order')
    require('var value = circl_ease_in(current_time, start_value, endValue, length_ms)'in slow and '\t\tt /= d'not in slow and '\tt /= d\n\treturn -c * (sqrt(1 - t * t) - 1) + b'in slow,'Unknown original circular easing')
    require('if current_time >= length_ms:\n\t\tset_process(false)\n\t\tvalue = endValue\n\tEngine.time_scale = value\n\tif with_pitch: audioManager.set_audio_pitch(value)'in slow,'Unknown Slowmo end/audio order')
    require('Input.set_mouse_mode(Input.MOUSE_MODE_HIDDEN)\n\t_mouse_position = Input.get_last_mouse_speed()'in mouse and 'if _mouse_position != Input.get_last_mouse_speed():'in mouse,'Unknown source mouse velocity comparison')
    require('if Input.get_mouse_button_mask():'in mouse and 'if !value:\n\t\tInput.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)'in mouse,'Unknown mouse input/disable behavior')
    require('func set_audio_pitch(speed: float):\n\tfor soundEffect in $Sfx.get_children():\n\t\tsoundEffect.pitch_scale = speed'in texts[AUDIO],'Unknown actual Sfx-only pitch target')
    policy=dict(slow_end=float(re.search(r'^const endValue = (\d+)',slow,re.M)[1]),length_scale=float(re.search(r'length_ms = length \* (\d+)',slow)[1]),default_pitch=re.search(r'pitch := (true|false)',slow)[1]=='true',moving_frames=float(re.search(r'_mouse_hidden_time >= delta\*(\d+)',mouse)[1]),idle_reset=float(re.search(r'_mouse_idle_time >= ([0-9.]+)',mouse)[1]),shown_hide=float(re.search(r'_mouse_shown_time >= ([0-9.]+)',mouse)[1]),**{k:engine[k]for k in ['visible','hidden','mouse_enter','engine_initial_scale']})
    return dict(schema=1,commit=PIN,family=FAMILY,kind='encore.global-child-ready.source-ir',identity=dict(scene_id=ctor['scene_id'],source_sha256=ctor['source_sha256']),constructor_ir_sha256=sha(CTOR),engine_receipt_sha256=sha(ENGINE),sources=ex.sources,nodes=nodes,policy=policy,audio_source=AUDIO,audio_method='set_audio_pitch',semantics=['Same actual two source Node children; no duplicate ObjectDB allocation','Actual native Ready automatic idle/input activation is supplied by kernel; this adapter receives ReadyScript only','Slowmo stores real monotonic milliseconds, starts by scale assignment before enabling process; no RNG','Circular expression uses endValue as coefficient, not end-start; source overtime computes sqrt then overwrites endValue before publication','Each pitch-enabled idle publishes engine scale then actual audioManager Sfx children pitch, audio owner absence refuses before start','Mouse compares last mouse SPEED rather than screen coordinates, reads twice in moving frame; input button branch resets all three timers','Mouse hidden-motion threshold depends on current delta*5, not accumulated frames count; original order and >= boundaries preserved','3DS physical touch uses existing NativeInputAdapter gesture endpoint; no mouse movement or mouse-window notification fabricated'],scene_admitted=False)
def extract(directory):
    engine_export(directory);d=derive();write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],engine_receipt_sha256=sha(ENGINE),scope='Full source Slowmo/MouseHider methods, no whole global Ready; Sfx pitch requires actual audio owner'))
def load():
    d=read(IR);require(d==derive(),'Changed original child source/bindings/policy');r=read(REVIEW);require(r['ir_sha256']==sha(IR)and r['engine_receipt_sha256']==sha(ENGINE)and r['sources']==d['sources'],'Stale child review');return d
def encode(d):
    b=bytearray(128)
    def u(*x):b.extend(struct.pack('<'+'I'*len(x),*x))
    def t(s):v=s.encode();u(len(v));b.extend(v)
    b.extend(bytes.fromhex(d['constructor_ir_sha256']));b.extend(bytes.fromhex(d['engine_receipt_sha256']));t(d['audio_source']);t(d['audio_method'])
    u(len(d['sources']))
    for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
    u(len(d['nodes']))
    for r in d['nodes']:
        u(r['kind'],r['id'],r['class_index'],r['methods']);t(r['path']);t(r['script']);b.extend(bytes.fromhex(r['script_sha256']))
        for k in ['fields','methods_source']:u(len(r[k]));[t(x)for x in r[k]]
    p=d['policy'];b.extend(struct.pack('<6d',*[p[k]for k in ['slow_end','length_scale','moving_frames','idle_reset','shown_hide','engine_initial_scale']]));u(p['visible'],p['hidden'],p['mouse_enter'],int(p['default_pitch']))
    struct.pack_into('<8s8I',b,0,b'ENCCHD01',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,d['identity']['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['identity']['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)
def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--engine-directory',type=Path);a=p.parse_args()
    if a.action=='extract':require(a.engine_directory,'Actual fixed native source directory required');extract(a.engine_directory);return
    b=encode(load())
    if a.action=='compile':PACK.write_bytes(b)
    else:require(PACK.read_bytes()==b,'Stale child binary')
    print('Source Slowmo/MouseHider: two actual child methods;',len(b),'bytes')
if __name__=='__main__':
    try:main()
    except(ValueError,KeyError,OSError,TypeError,StopIteration,AttributeError)as e:sys.exit('GLOBAL CHILD SOURCE ERROR: '+str(e))
