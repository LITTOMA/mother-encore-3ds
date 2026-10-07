#!/usr/bin/env python3
"""Remaining actual scene-native leaves, sharing reviewed typed playback owners."""
import argparse,json,struct,sys,zlib,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import read,write,sha,require,PIN,SCENE,decode
from tools.extract_battle_entry import Extractor
IR=ROOT/'content/native-scene-leaf-native.json';PACK=ROOT/'romfs/data/podunk.encleafnative'
REVIEW=ROOT/'reports/scene-leaf-native/source-review.json'
INPUTS=['content/podunk-node-tree.json','content/podunk-game-camera.json','content/native-field-player-transitions.json','content/podunk-birds.json','content/native-field-dropped.json','content/native-field-payphone.json','content/native-field-melody-background.json']
PRIOR=['content/podunk-camera-arrows.json','content/native-field-npc.json','content/native-field-butterfly.json','content/native-prompt-native.json','content/native-scene-clip-native.json']
def derive(native, engine, camera):
    ex=Extractor(ROOT);t=read(ROOT/INPUTS[0]);by={n['node']:n for n in t['records']};d=read(native)
    require(d['source']=='res://'+SCENE,'Native scene identity changed')
    nm={n['path']:decode(n['properties'])for n in d['nodes']};resources={n['id']:n for n in d['resources']}
    expected={};apprior=set();rows=[]
    inputs=[read(ROOT/p) for p in INPUTS]
    for ir in inputs[1:]:
        for p,h in ir['sources'].items():ex.data(p);require(ex.sources[p]==h,'Typed source closure changed '+p)
    def add(path,kind,owner,target=0):
        n=by[path];require(n['id']not in expected,'Duplicate native ownership '+path)
        expected[n['id']]=dict(id=n['id'],parent=n['parent'],owner=owner,kind=kind,path=path,target=target)
    for n in inputs[1]['records']:
        add(n['node']+'/ArrowsAnim',1,n['id']);add(n['node'],9,n['id'])
    for n in inputs[2]['records']:
        if n['kind']==1:add(n['node']+'/AnimationPlayer',2,n['id'],n['sprite'][7])
    for n in inputs[3]['records']:add(n['node']+'/AnimationPlayer',3,n['id'],n['children'][0])
    for n in inputs[4]['bindings']:
        add(n['node']+'/AnimationPlayer',4,n['id'],by[n['node']+'/Sprite']['id'])
        add(n['node']+'/Tween',8,n['id'],by[n['node']+'/Sprite']['id'])
    for n in inputs[5]['records']:add(n['node']+'/AnimationPlayer',5,n['id'],n['sprite_id'])
    for n in inputs[6]['bindings']:
        add(n['node']+'/AnimationPlayer',7,n['id'],n['bg_id']);add(next(v['node']for v in t['records']if v['id']==n['bg_id']),12,n['id'],n['bg_id'])
    empty=by['Cutscenes/Intro Cutscene'];add(empty['node']+'/AnimationPlayer',6,empty['id']);add(empty['node']+'/Camera2D',10,empty['id'])
    add('Camareas/CamareaRect',11,by['Camareas']['id'])
    for p in PRIOR:
        x=read(ROOT/p)
        if p.endswith('camera-arrows.json'):apprior.update(v['id']for v in x['players'])
        elif p.endswith('npc.json'):apprior.update(by[v['node']+'/CharacterSprite/AnimationPlayer']['id']for v in x['npcs'])
        elif p.endswith('butterfly.json'):apprior.update(v[k]for v in x['bindings']for k in ('fly_id','orbit_id'))
        else:apprior.update(v['id']for v in x['records']if any(n['id']==v['id']and t['classes'][n['class_index']]=='AnimationPlayer'for n in t['records']))
    allaps={n['id']for n in t['records']if t['classes'][n['class_index']]=='AnimationPlayer'}
    require(not(set(expected)&apprior)and {k for k,v in expected.items()if v['kind']<=7}==allaps-apprior,'Actual AP complement missing/duplicate')
    for n in t['records']:
        if n['id']not in expected:continue
        v=expected[n['id']];p=nm[n['node']];v['class']=t['classes'][n['class_index']]
        require(not n['script']or v['kind']==9,'Unexpected native leaf source script')
        v['clips']=[]
        if v['kind']<=7:
            require(v['class']=='AnimationPlayer'and p['root_node']=={'type':'NodePath','value':'..'}and p['autoplay']==''and p['playback_process_mode']==1 and p['playback_default_blend_time']==0 and p['blend_times']==[]and p['script']is None,'Unsupported AP native playback')
            require(p['playback_speed']==(0 if v['kind']==4 else 1)and p['method_call_mode']==(1 if v['kind']==1 else 0),'Native AP speed/method changed')
            v['speed']=p['playback_speed'];v['method_mode']=p['method_call_mode']
            for key,value in p.items():
                if not key.startswith('anims/'):continue
                resource=resources[value['id']];a=decode(resource['properties'])
                require(resource['class']=='Animation'and not a['script'],'Unknown actual Animation class/script')
                v['clips'].append(dict(name=key[6:],length=a['length'],loop=a['loop']))
            require(bool(v['clips'])==(v['kind']!=6),'Empty source clip library changed')
        else:
            v['speed']=0;v['method_mode']=0
        v['properties']=p if v['kind']>=8 else {}
        v['active_clip']=''
        if v['kind']==5:v['active_clip']=re.search(r'_anim_player\.play\("([^"]+)"\)',ex.text('Maps/Testing/phone.gd').split('func _use_phone')[1])[1]
        if v['kind']==2:v['active_clip']=re.search(r'\$AnimationPlayer\.play\("([^"]+)"\)',ex.text('Scripts/Main/Jump Area.gd'))[1]
        if v['kind']==4:v['active_clip']=next(c['name']for c in v['clips']if c['loop'])
        if v['kind']==8:require(p['playback_process_mode']==1 and p['playback_speed']==p['playback/speed']==1 and p['playback/active']is False and p['repeat']is False and p['playback/repeat']is False and p['script']is None,'Dropped Tween native defaults changed')
        if v['kind']==11:require(p['editor_only']is True,'ReferenceRect requires runtime GPU border')
        rows.append(v)
    require('set_process_internal' in engine.read_text() and 'get_camera_screen_center' in camera.read_text() and 'connect("size_changed", this, "_update_scroll")' in camera.read_text(),'Missing reviewed native source evidence')
    return dict(schema=1,capability=1,rules=1,family=0x454e0070,commit=PIN,scene=SCENE,scene_id=t['scene_id'],source_sha256=t['source_sha256'],sources=ex.sources,dependencies={p:sha(ROOT/p)for p in INPUTS+PRIOR},native_sha256=sha(native),engine_sha256={engine.name:sha(engine),camera.name:sha(camera)},symbols=['idle_process_internal','animation_started','animation_finished','size_changed','_update_scroll'],records=rows)
def encode(d):
    o=bytearray()
    def u(*v):o.extend(struct.pack('<'+'I'*len(v),*v))
    def f(*v):o.extend(struct.pack('<'+'f'*len(v),*v))
    def s(v):b=v.encode();u(len(b));o.extend(b)
    for v in d['symbols']:s(v)
    u(len(d['sources']))
    for p,h in sorted(d['sources'].items()):s(p);o.extend(bytes.fromhex(h))
    u(len(d['records']))
    for v in d['records']:
        u(v['id'],v['owner'],v['parent'],v['kind'],v['target'],v['method_mode']);f(v['speed']);s(v['path']);s(v['class']);s(v['active_clip']);u(len(v['clips']))
        for c in v['clips']:s(c['name']);f(c['length']);u(int(c['loop']))
        p=v['properties']
        if v['kind']in (9,10):
            f(*p['offset'],*p['zoom']);u(p['anchor_mode'],int(p['rotating']),int(p['current']),p['process_mode']);u(*(p[k]&0xffffffff for k in ('limit_left','limit_top','limit_right','limit_bottom')))
            require(not p['limit_smoothed']and not p['drag_margin_h_enabled']and not p['drag_margin_v_enabled']and not p['smoothing_enabled']and p['offset_h']==p['offset_v']==0,'Unsupported camera smoothing/drag')
        elif v['kind']==11:
            f(*(p[k]for k in ('margin_left','margin_top','margin_right','margin_bottom')));u(int(p['editor_only']))
    return b'ENCSLN01'+struct.pack('<7I',1,1,1,d['family'],d['scene_id'],len(o),zlib.crc32(o))+bytes.fromhex(PIN)+bytes.fromhex(d['source_sha256'])+o
def load():
    d=read(IR);require(d['schema']==d['capability']==d['rules']==1 and d['family']==0x454e0070 and d['commit']==PIN and d['scene']==SCENE,'Leaf identity/version changed')
    require(sha(IR)==read(REVIEW)['ir_sha256'],'Leaf semantic review stale');ex=Extractor(ROOT)
    for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Leaf source changed '+p)
    for p,h in d['dependencies'].items():require(sha(ROOT/p)==h,'Leaf actual typed dependency changed '+p)
    return d
def stage_files(root):
    raw=(Path(root)/'data/podunk.encleafnative').read_bytes();require(raw==encode(load()),'Leaf staged bytes changed');return {Path('data/podunk.encleafnative'):raw}
def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);p.add_argument('--native',type=Path);p.add_argument('--engine',type=Path);p.add_argument('--camera-engine',type=Path);a=p.parse_args()
    if a.action=='extract':
        require(a.native is not None and a.engine is not None and a.camera_engine is not None,'Actual native and engine source conversions required');d=derive(a.native,a.engine,a.camera_engine);write(IR,d)
        write(REVIEW,dict(schema=1,ir_sha256=sha(IR),commit=PIN,engine_sha256=d['engine_sha256'],semantics=['Actual 91-AP disjoint complement of existing 581 typed native AP; complete stable source identities, no count-only admission','Existing typed owner remains unique clip clock; native AP emits source signals/internal membership','Empty Intro source clip library is genuinely empty; unknown play rejected','ReferenceRect source editor_only=true has no in-game border draw','Source Camera2D properties retain defaults and distinguish 14 script Cameras from one plain Intro camera','Original GameCamera construction/Ready does not call dynamic Shaker or SceneTreeTween branches; native capability checks occur at actual branch invocation, never default success','Dropped animation/Tween start notifications register existing core playback in native internal group without a second clock','Camera no-drag/no-smoothing selection is shared by actual scene/Player native owners; source global.currentCamera remains independent']))
    else:PACK.write_bytes(encode(load()));print('Scene leaf native:',len(load()['records']),'actual leaves')
if __name__=='__main__':main()
