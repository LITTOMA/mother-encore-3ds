#!/usr/bin/env python3
"""Source-reviewed Podunk signal symbols, not a script interpreter."""
from pathlib import Path
import argparse, hashlib, re, struct, sys, zlib
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor
from tools.podunk_scene import PIN, read, write, require, sha
IR=ROOT/'content/native-field-scene-signal-callbacks.json'
REVIEW=ROOT/'reports/field-scene-signal-callbacks/source-review.json'
PACK=ROOT/'romfs/data/podunk.encsignals'
LIFECYCLE=ROOT/'content/podunk-scene-lifecycle.json'
CONSTRUCTOR=ROOT/'content/native-global-data-constructor.json'

def derive(engine_proof=None, engine_path=None, object_path=None):
    ex=Extractor(ROOT); lifecycle=read(LIFECYCLE)
    require(lifecycle['commit']==PIN,'Signal lifecycle pin differs')
    sources={}
    def source(p):
        text=ex.text(p); sources[p]=ex.sources[p]; return text
    global_text=source('Scripts/global/global.gd')
    area=source('Scripts/Main/RoomTypes/AreaRoom.gd')
    prompt=source('Scripts/UI/Button Prompt.gd')
    player=source('Scripts/Main/party/Player.gd')
    arrows=source('Scripts/UI/MapScreen/MapArrows.gd')
    # The Area's party member reference is authoring data, never a C++ name/ID.
    members=re.findall(r'globaldata\.characters\.([A-Za-z_][A-Za-z_0-9]*)',area)
    require(len(members)==3 and len(set(members))==1,'Area party member expression differs')
    ctor=read(CONSTRUCTOR);require(ctor['commit']==PIN,'Area character constructor pin differs')
    declarations=[v for v in ctor['declarations']if v['name']=='characters'and v['kind']==9]
    require(len(declarations)==1,'Area character dictionary absent/ambiguous')
    matches=[v for v in declarations[0]['references']if v[0]==members[0]]
    require(len(matches)==1,'Area party dictionary binding absent/ambiguous')
    member_id=matches[0][1]
    objects=[v for v in ctor['objects']if v['id']==member_id]
    require(len(objects)==1 and objects[0]['kind']==1 and objects[0]['role']==1 and objects[0]['name']==members[0], 'Area party declaration kind/identity differs')
    for path,h in ctor['sources'].items():
        source(path);require(sources[path]==h,'Area constructor source changed')
    signals=[]
    for role,name,text,arity,usage in [(1,'flags_updated',global_text,0,0),(2,'area_left',area,0,1),
        (3,'synced_switches_changed',area,3,3),(4,'event_detector_entered',player,1,1),
        (5,'event_detector_exited',player,1,1),(6,'paused',player,0,0),(7,'unpaused',player,0,0),
        (8,'inputs_changed',global_text,0,0),(9,'locale_changed',global_text,0,0)]:
        m=re.search(r'^signal\s+'+re.escape(name)+r'\s*(?:\(([^\n]*)\))?\s*$',text,re.M)
        require(m is not None,'Missing actual source signal '+name)
        args=[] if not m.group(1) else m.group(1).split(',')
        require(len(args)==arity,'Source signal declaration signature differs')
        signals.append(dict(role=role,name=name,declaration=arity,emission=usage))
    require('emit_signal("area_left",' in area,'Actual source area emit usage absent')
    for role,name,text,arity in [(10,'hide',prompt,0),(11,'frame_changed',arrows,0),(12,'animation_finished',arrows,1)]:
        require('"'+name+'"' in text,'Source native signal use absent '+name)
        signals.append(dict(role=role,name=name,declaration=arity,emission=arity))
    if engine_proof is None:
        require(engine_path is not None and object_path is not None,'Extraction needs explicit reviewed engine source files')
        engine=Path(engine_path).read_text(encoding='utf-8'); obj=Path(object_path).read_text(encoding='utf-8')
        require('obj->connect(signal, gdfs.ptr(), "_signal_callback", varray(gdfs), Object::CONNECT_ONESHOT)' in engine,
            'Engine yield actual FunctionState self-bind/one-shot differs')
        require('Ref<GDScriptFunctionState> self = *p_args[p_argcount - 1];' in engine and 'return resume(arg);' in engine,
            'Engine signal resume semantics differ')
        require('int argc = p_argcount;' in obj and 'target->call(c.method, args, argc, ce)' in obj,
            'Native emission forwards actual varargs unchanged')
        engine_proof={'gdscript_function.cpp':sha(Path(engine_path)),'object.cpp':sha(Path(object_path))}
    require(set(engine_proof)=={'gdscript_function.cpp','object.cpp'} and
        all(re.fullmatch('[0-9a-f]{64}',h) for h in engine_proof.values()),'Source engine proof unknown')
    switch_source='Scripts/Main/Switches/TwoStatesSwitch.gd'
    switch=source(switch_source)
    require('class_name TwoStatesSwitch' in switch and 'emitter: TwoStatesSwitch' in area,
        'Source switch callback object type differs')
    signals += [dict(role=13,name='_signal_callback',declaration=1,emission=1),dict(role=14,name='GDScriptFunctionState',declaration=0,emission=0),dict(role=15,name=switch_source,declaration=0,emission=0)]
    rules={7:[(1,'Scripts/Main/Flag Landmarks.gd','_check_flags',0)],
        8:[(2,'Scripts/Main/FlaggableObject.gd','_on_leave_area',1)],
        9:[(3,'Scripts/Main/RoomTypes/AreaRoom.gd','_on_switches_changed_state',3)],
        14:[(5,'Scripts/UI/Button Prompt.gd','_on_player_nearby',2),(6,'Scripts/UI/Button Prompt.gd','_on_player_toggle_pause',0),(7,'Scripts/UI/Button Prompt.gd','_set_key_name',0)],
        16:[(4,'Scripts/Main/Openable Door.gd','_update_door_state',0)],
        18:[(1,'Scripts/Main/Interact Dialog.gd','_check_flags',0)],
        19:[(1,'Scripts/Main/Interact Dialog.gd','_check_flags',0)],
        20:[(2,'Scripts/Main/FlaggableObject.gd','_on_leave_area',1)],
        21:[(2,'Scripts/Main/FlaggableObject.gd','_on_leave_area',1),(9,'Scripts/Main/DroppedItem.gd','_on_player_paused',0),(10,'Scripts/Main/DroppedItem.gd','_on_player_unpaused',0)]}
    callbacks=[]
    for row in lifecycle['roster']:
        for role,path,name,arity in rules.get(row['role'],[]):
            body=source(path); leaf=source(row['script'])
            require(ex.sources[row['script']]==row['sha256'],'Signal receiver leaf source differs')
            m=re.search(r'^func\s+'+re.escape(name)+r'\(([^\n]*)\)',body,re.M)
            require(m is not None,'Actual callback method absent '+path+':'+name)
            require(len([x for x in m.group(1).split(',') if x.strip()])==arity,'Source callback signature differs')
            callbacks.append(dict(node=row['id'],role=role,script=row['script'],method_source=path,
                method=name,leaf_sha=row['sha256'],method_sha=ex.sources[path],arguments=arity))
    # MapArrow AnimationPlayer source connections bind the particular arrow.
    require('_on_anim_finished' in arrows,'Source MapArrows finished callback absent')
    for row in lifecycle['roster']:
        if row['script']=='Scripts/UI/MapScreen/MapArrows.gd':
            callbacks.append(dict(node=row['id'],role=8,script=row['script'],method_source=row['script'],
                method='_on_anim_finished',leaf_sha=row['sha256'],method_sha=row['sha256'],arguments=2))
    return dict(schema=1,format=2,capability=2,rules=1,family=0x454e0068,commit=PIN,
        scene=lifecycle['scene'],scene_id=lifecycle['scene_id'],source_sha256=lifecycle['source_sha256'],
        sources=sources,dependencies={str(LIFECYCLE.relative_to(ROOT)):sha(LIFECYCLE),str(CONSTRUCTOR.relative_to(ROOT)):sha(CONSTRUCTOR)},
        party_member=dict(name=members[0],declaration=member_id,constructor_sha256=sha(CONSTRUCTOR)),
        engine=engine_proof,symbols=signals,callbacks=callbacks,
        wait_source='Scripts/UI/MapScreen/MapArrows.gd',wait_sha=sources['Scripts/UI/MapScreen/MapArrows.gd'],
        wait_id=int.from_bytes(hashlib.sha256(b'scene-signal-function-state').digest()[:4],'little'),wait_flags=4)

def load(root=ROOT):
    require(Path(root).resolve()==ROOT,'Source symbol root differs')
    ir=read(IR)
    review=read(REVIEW); require(review['ir_sha256']==sha(IR) and review['producer_sha256']==sha(Path(__file__)), 'Source symbols need actual semantic review')
    require(ir==derive(review['engine_sources']),'Signal source/producer/lifecycle closure differs')
    return ir

def encode(ir):
    out=bytearray()
    def u(v):out.extend(struct.pack('<I',v))
    def s(v):
        b=v.encode('utf-8');u(len(b));out.extend(b)
    s(ir['scene']);s(ir['wait_source']);out.extend(bytes.fromhex(ir['wait_sha']));u(ir['wait_id']);u(ir['wait_flags'])
    s(ir['party_member']['name']);u(ir['party_member']['declaration']);out.extend(bytes.fromhex(ir['party_member']['constructor_sha256']))
    u(len(ir['sources']))
    for p,h in sorted(ir['sources'].items()):s(p);out.extend(bytes.fromhex(h))
    u(len(ir['symbols']))
    for v in ir['symbols']:u(v['role']);s(v['name']);u(v['declaration']);u(v['emission'])
    u(len(ir['callbacks']))
    for v in ir['callbacks']:
        u(v['node']);u(v['role']);s(v['script']);s(v['method_source']);s(v['method'])
        out.extend(bytes.fromhex(v['leaf_sha']));out.extend(bytes.fromhex(v['method_sha']));u(v['arguments'])
    header=b'ENCSIG01'+struct.pack('<7I',2,2,1,0x454e0068,ir['scene_id'],len(out),zlib.crc32(out))+bytes.fromhex(PIN)+bytes.fromhex(ir['source_sha256'])
    return header+out

def compile():
    raw=encode(load());PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
    return raw

def stage_files(root):
    raw=(Path(root)/'data/podunk.encsignals').read_bytes();require(raw==encode(load()),'Staged scene symbols differ')
    return {Path('data/podunk.encsignals'):raw}

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);p.add_argument('--engine',type=Path);p.add_argument('--object',type=Path);a=p.parse_args()
    if a.action=='extract':
        d=derive(engine_path=a.engine,object_path=a.object);write(IR,d)
        write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),engine_sources=d['engine'],semantics=[
            'Exact source callback signatures, inherited source bindings and synchronous signal bool binds.',
            'AreaRoom declares area_left with zero arguments and emits one region_changed bool; actual Object emission forwards varargs.',
            'MapArrows one-shot FunctionState yield resumes on the actual frame_changed signal without a second clock.',
            'AreaRoom party member is resolved from its three actual dictionary expressions and the reviewed globalData constructor; same declaration ID is consumed by the actual party array operations.'],
            not_implemented=['No general script VM; unsupported source callbacks reject.']))
    else:compile()
