#!/usr/bin/env python3
"""Compile validated external native-content IR to immutable ENCRMD01 tables.

This is a data compiler, not a C++ generator or a Godot runtime. It never reads
runtime C++ or generated content headers and never invokes a C++ toolchain.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import re
from pathlib import Path
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
MAGIC = b'ENCRMD01'
FAMILY = 0x454e0002
RULES = 4
CAPABILITIES = 4
TARGET = 1
HEADER_BYTES = 128
DIRECTORY_BYTES = 24
MAX_BYTES = 1024 * 1024
NONE = 0xffffffff
NO_ACTOR = 0xffff
# Format schema only. No game content values belong in this module.
SCHEMAS = {
    'StringRef': (1, 8, [('offset','u32'),('length','u32')]),
    'StringBytes': (2, 1, []),
    'Resource': (3, 56, [('stable_id','u32'),('path_string','u32'),('width','u16'),('height','u16'),('columns','u16'),('rows','u16'),('kind','u16'),('flags','u16'),(None,'pad4'),('sha256','hash256')]),
    'Vec2': (4, 8, [('x','f32'),('y','f32')]),
    'Polygon': (5, 32, [('body_id','u32'),('owner_id','u32'),('first_vertex','u32'),('vertex_count','u16'),('flags','u16'),('minimum','vec2'),('maximum','vec2')]),
    'BodyRule': (6, 16, [('body_id','u32'),('source_path_string','u32'),('initially_enabled','u8'),('flags','u8'),(None,'pad2'),(None,'pad4')]),
    'Overlay': (7, 32, [('stable_id','u32'),('resource_index','u32'),('x','f32'),('y','f32'),('sort_y','f32'),('u','u16'),('v','u16'),('w','u16'),('h','u16'),('flags','u16'),(None,'pad2')]),
    'MapDraw': (8, 24, [('stable_id','u32'),('resource_index','u32'),('x','f32'),('y','f32'),('w','u16'),('h','u16'),('flags','u32')]),
    'Clip': (9, 20, [('stable_id','u32'),('length','f32'),('first_key','u32'),('key_count','u16'),('flags','u8'),('visibility','u8'),('frame_count','u16'),('channel','u16')]),
    'Key': (10, 8, [('time','f32'),('frame','u16'),(None,'pad2')]),
    'DirectionFrame': (11, 2, [('frame','u16')]),
    'ActorProfile': (12, 80, [('stable_id','u32'),('execution_kind','u16'),('flags','u16'),('primary_resource','u32'),('shadow_resource','u32'),('emote_resource','u32'),('animation_binding_first','u32'),('animation_binding_count','u16'),('initial_frame','u16'),('emote_initial_frame','u16'),(None,'pad2'),('sprite_position','vec2'),('sprite_offset','vec2'),('emote_offset','vec2'),('shadow_offset','vec2'),('direction_first','u32'),('direction_count','u16'),(None,'pad2'),('idle_clip','u32'),('emote_clip','u32')]),
    'ActorInstance': (13, 40, [('stable_id','u32'),('profile_index','u32'),('binding_kind','u16'),('flags','u16'),('display_name_string','u32'),('position','vec2'),('direction','vec2'),('initial_clip','u32'),(None,'pad4')]),
    'CameraArea': (14, 24, [('stable_id','u32'),('flags','u32'),('center','vec2'),('extents','vec2')]),
    'Flag': (15, 16, [('stable_id','u32'),('name_string','u32'),('default_value','u8'),('flags','u8'),(None,'pad2'),(None,'pad4')]),
    'InitialFlag': (16, 8, [('flag_index','u32'),('value','u8'),(None,'pad3')]),
    'Condition': (17, 8, [('flag_index','u32'),('expected_value','u8'),('domain','u8'),(None,'pad2')]),
    'Trigger': (18, 32, [('stable_id','u32'),('first_vertex','u32'),('vertex_count','u16'),('flags','u16'),('condition_first','u32'),('condition_count','u32'),('program_index','u32'),('actor_instance_index','u32'),(None,'pad4')]),
    'Program': (19, 20, [('stable_id','u32'),('first_command','u32'),('command_count','u32'),('phrase_count','u32'),('source_path_string','u32')]),
    'Command': (20, 48, [('opcode','u16'),('actor_index','u16'),('phrase','u32'),('target_index','u32'),('flags','u32'),('vector','vec2'),('value','f64'),('duration','f64'),('auxiliary_index','u32'),(None,'pad4')]),
    'Binding': (21, 32, [('stable_id','u32'),('kind','u16'),('flags','u16'),('target_index','u32'),('auxiliary_index','u32'),('value','f64'),('duration','f64')]),
    'Battle': (22, 32, [('stable_id','u32'),('enemy_string','u32'),('actor_instance_index','u32'),('win_flag_index','u32'),('advantage','i32'),('flags','u32'),('win_cutscene_string','u32'),('battle_resource_index','u32')]),
    'Rule': (23, 16, [('key','u16'),('scalar_type','u16'),(None,'pad4'),('value','rule_value')]),
    'Experience': (24, 4, [('required_total_exp','u32')]),
    'Scene': (25, 80, [('stable_id','u32'),('display_name_string','u32'),('version_string','u32'),('source_scene_string','u32'),('player_instance_index','u32'),('initial_program_index','u32'),('actor_hull_first','u32'),('actor_hull_count','u32'),('spawn','vec2'),('start_direction','vec2'),('initial_motion_state','u16'),('initial_frame','u16'),('initial_flag_first','u32'),('initial_flag_count','u32'),('body_rule_first','u32'),('body_rule_count','u32'),('default_camera_area','u32'),('rule_profile_id','u32'),('flags','u32')]),
    'AnimationBinding': (26, 8, [('actor_profile_index','u16'),('motion_state','u8'),('direction','u8'),('clip_index','u32')]),
    'MovementPath': (27, 24, [('stable_id','u32'),('first_entry','u32'),('entry_count','u16'),('flags','u16'),('animation_motion','u16'),(None,'pad2'),('speed','f64')]),
    'MovementEntry': (28, 24, [('kind','u16'),(None,'pad6'),('vector','vec2'),('duration','f64')]),
}
RULE_TYPES = {**{i:1 for i in (1,2,3,4,5,20)}, **{i:2 for i in list(range(6,20))+list(range(21,28))}}
FORMATS = {'u8':'B','u16':'H','u32':'I','i32':'i','f32':'f','f64':'d','vec2':'2f','hash256':'32s','rule_value':'8s'}
WIDTHS = {k:struct.calcsize('<'+v) for k,v in FORMATS.items()}
OPCODES = ['BeginCutscene','BindActor','ActorPersistent','StartWait','MusicFadeOut','SetTalker','CallObjectDeferred','OverworldBattleMusic','PlaySound','MoveActor','TurnActor','ShakeActor','JumpActor','AnimateActor','EmoteActor','ShakeCamera','ChangeCamera','MoveCamera','QueueBattle','StopInteraction','RestoreActor','ReleaseBattleActor','CutsceneEnded','DialogueDone','RequestBattle','YieldIdle','AwaitTimer','SetActorDirection','TeleportActor','MoveActorPath','ReturnCamera','SetFlag','ShowDialogue','AwaitDialogue','PlayMusicImmediate','HideDialogue','Jump','BranchFlag','BranchLeader','AwaitChoices','OpenSave','AwaitSubmenu','StopActorLoop','OpenStorage','GrantKeyItem','LearnSkill','AnimateSpecialActor','BranchInventorySpace','GrantInventoryItem']
ROOT_FIELDS = {'schema','family','rules','capabilities','scene_id','upstream_commit','exporter_version','adapter_revision','strings','sections','provenance'}

class ContentError(ValueError):
    pass

def check(condition, message):
    if not condition:
        raise ContentError(message)

def fields(value, expected, label):
    check(type(value) is dict and set(value)==set(expected), label+': missing or unknown fields')

def integer(value, lo, hi, label):
    check(type(value) is int and lo<=value<=hi, label+': integer out of range')
    return value

def number(value, label):
    check(type(value) in (int,float) and math.isfinite(value),label+': finite number required')
    return value

def digest_hex(value, length, label):
    check(type(value) is str and len(value)==length and all(c in '0123456789abcdef' for c in value),label+': invalid hex fingerprint')
    raw=bytes.fromhex(value)
    check(any(raw),label+': zero fingerprint is not allowed')
    return raw

def canonical(value):
    return json.dumps(value,ensure_ascii=False,sort_keys=True,separators=(',',':'),allow_nan=False).encode('utf-8')

def _kind_width(kind):
    return int(kind[3:]) if kind.startswith('pad') else WIDTHS[kind]

for _name,(_, _stride,_schema) in SCHEMAS.items():
    if _schema:
        assert sum(_kind_width(k) for _,k in _schema)==_stride, _name

def encode_record(name, record):
    _,stride,schema=SCHEMAS[name]
    fields(record,[f for f,_ in schema if f is not None],name)
    out=bytearray()
    for field,kind in schema:
        label=name+'.'+str(field)
        if field is None:
            out+=bytes(_kind_width(kind));continue
        value=record[field]
        if kind.startswith('u'):
            value=integer(value,0,(1<<(_kind_width(kind)*8))-1,label)
        elif kind=='i32':value=integer(value,-(1<<31),(1<<31)-1,label)
        elif kind in ('f32','f64'):value=number(value,label)
        elif kind=='vec2':
            check(type(value) is list and len(value)==2,label+': two components required')
            value=[number(v,label) for v in value]
        elif kind=='hash256':value=digest_hex(value,64,label)
        elif kind=='rule_value':
            typ=integer(record['scalar_type'],1,3,label)
            if typ==1:value=struct.pack('<f',number(value,label))+bytes(4)
            elif typ==2:value=struct.pack('<d',number(value,label))
            else:value=struct.pack('<I',integer(value,0,NONE,label))+bytes(4)
        try:out+=struct.pack('<'+FORMATS[kind],*(value if kind=='vec2' else [value]))
        except (OverflowError,struct.error) as exc:raise ContentError(label+': not representable')from exc
    check(len(out)==stride,name+': internal stride mismatch')
    return bytes(out)

def decode_record(name, data, offset):
    _,_,schema=SCHEMAS[name];record={}
    for field,kind in schema:
        n=_kind_width(kind);part=data[offset:offset+n];offset+=n
        check(len(part)==n,name+': truncated record')
        if field is None:
            check(not any(part),name+': nonzero reserved bytes');continue
        value=struct.unpack('<'+FORMATS[kind],part)
        value=list(value) if kind=='vec2' else value[0]
        if kind=='hash256':value=value.hex()
        if kind=='rule_value':
            typ=record['scalar_type'];check(typ in (1,2,3),'Rule: unknown scalar type')
            if typ!=2:check(value[4:]==bytes(4),'Rule: nonzero unused payload')
            value=struct.unpack('<'+{1:'f',2:'d',3:'I'}[typ],value[:8 if typ==2 else 4])[0]
        if kind in ('f32','f64','rule_value') and type(value)is float:check(math.isfinite(value),name+': nonfinite number')
        if kind=='vec2':check(all(math.isfinite(v)for v in value),name+': nonfinite vector')
        record[field]=value
    return record

# Defensive limits are schema/runtime capacity contracts, not content counts.
LIMITS = {'Resource':1024,'Vec2':65536,'Polygon':4096,'BodyRule':16384,'Overlay':16384,'MapDraw':4096,'Clip':4096,'Key':32768,'DirectionFrame':32768,'ActorProfile':64,'ActorInstance':64,'CameraArea':64,'Flag':4096,'InitialFlag':4096,'Condition':8192,'Trigger':64,'Program':1024,'Command':65535,'Binding':4096,'Battle':1024,'Rule':64,'Experience':10000,'Scene':1,'AnimationBinding':8192,'MovementPath':1024,'MovementEntry':8192}
ACTOR_OPS = {1,2,9,10,11,12,13,14,16,18,19,20,21,24,27,28,29,32,42,46}
VECTOR_OPS = {9,10,11,15,17,27,28}
VALUE_OPS = {7,9,12,15,31,37,47}
DURATION_OPS = {3,4,10,11,12,15,17,23,30}
TARGET_TABLE = {6:'Binding',8:'Resource',13:'Clip',14:'Clip',18:'Battle',24:'Battle',29:'MovementPath',31:'Flag',34:'Resource',37:'Flag'}

def validate_tables(strings, sections, scene_id, rules=RULES, capabilities=None):
    capabilities=rules if capabilities is None else capabilities
    check(type(strings)is list and 0<len(strings)<=65536,'Invalid string table count')
    check(strings[0]=='','String zero must be empty')
    check(all(type(s)is str for s in strings),'String value is not text')
    check(len(set(strings))==len(strings),'Duplicate strings')
    for s in strings:
        try:raw=s.encode('utf-8')
        except UnicodeError as exc:raise ContentError('Invalid UTF-8 string')from exc
        check('\0'not in s and len(raw)<=4096,'Invalid/oversized string')
    fields(sections,set(SCHEMAS)-{'StringRef','StringBytes'},'sections')
    for name,rows in sections.items():
        check(type(rows)is list and len(rows)<=LIMITS[name],name+': count exceeds schema limit')
        for row in rows:
            # Also validates exact fields, integer widths and float encodability.
            decode_record(name,encode_record(name,row),0)
        ids=[r['stable_id']for r in rows if 'stable_id'in r]
        check(all(x!=0 for x in ids)and len(ids)==len(set(ids)),name+': duplicate/zero stable ID')
    def ref(index,name,label,optional=False):
        if optional and index==NONE:return None
        check(0<=index<len(sections[name]),label+': invalid '+name+' reference')
        return sections[name][index]
    def string(index,label,empty=True):
        check(0<=index<len(strings),label+': invalid string reference')
        check(empty or bool(strings[index]),label+': nonempty string required')
        return strings[index]
    def span(first,count,name,label,minimum=0,maximum=None):
        check(count>=minimum and (maximum is None or count<=maximum),label+': unsupported count')
        check(first<=len(sections[name]) and count<=len(sections[name])-first,label+': range outside '+name)
        return sections[name][first:first+count]
    def vec(value,label):
        check(all(abs(v)<=1000000 for v in value),label+': coordinates exceed bounds')
    def polygon(first,count,label):
        vs=span(first,count,'Vec2',label,3,64)
        coords=[(v['x'],v['y'])for v in vs]
        area=sum(a[0]*b[1]-a[1]*b[0]for a,b in zip(coords,coords[1:]+coords[:1]))
        check(abs(area)>0.00001,label+': degenerate polygon')
        sign=1 if area>0 else -1
        for i,a in enumerate(coords):
            b=coords[(i+1)%count]
            for j,c in enumerate(coords):
                if j in (i,(i+1)%count):continue
                check(sign*((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))>0.00001,label+': nonconvex/collinear polygon')
        return coords
    for v in sections['Vec2']:vec([v['x'],v['y']],'Vec2')
    for r in sections['Resource']:
        path=string(r['path_string'],'Resource.path',False)
        check(r['kind']in(1,2,3,4)and r['flags']==0,'Unsupported resource kind/flags')
        check('\\'not in path and '..'not in path.split('/')and all(ord(c)>=32 and ord(c)!=127 for c in path),'Noncanonical resource path')
        if r['kind']==1:
            check(not path.startswith('/')and ':'not in path and path.endswith('.t3x')and all(part not in ('','.')for part in path.split('/')),'Texture path must be canonical relative t3x')
            check(0<r['width']<=1024 and 0<r['height']<=1024 and 0<r['columns']<=r['width'] and 0<r['rows']<=r['height'],'Unsupported texture layout')
            check(r['columns']*r['rows']<=65535,'Too many texture frames')
            check(r['width']%r['columns']==0 and r['height']%r['rows']==0,'Nonintegral texture grid')
        elif r['kind']==2:
            check(path.startswith('res://')and all(part not in ('','.','..')for part in path[6:].split('/'))and all(r[k]==0 for k in ('width','height','columns','rows')),'Invalid request-only audio resource')
        else:
            check(not path.startswith('/')and ':'not in path and path.endswith('.encbattle' if r['kind']==3 else '.encfx')and all(part not in ('','.')for part in path.split('/'))and all(r[k]==0 for k in ('width','height','columns','rows')),'Invalid checked content resource')
    bodies={r['body_id']:r for r in sections['BodyRule']}
    check(len(bodies)==len(sections['BodyRule'])and 0 not in bodies,'Duplicate/zero body ID')
    for b in bodies.values():
        string(b['source_path_string'],'BodyRule.path',False)
        check(b['initially_enabled']in(0,1)and b['flags']==0,'Invalid body rule')
    for p in sections['Polygon']:
        check(p['body_id']in bodies and p['owner_id']!=0 and p['flags']<=1,'Unknown polygon body/owner/flags')
        vs=polygon(p['first_vertex'],p['vertex_count'],'Polygon')
        check(p['minimum']==[min(v[0]for v in vs),min(v[1]for v in vs)]and p['maximum']==[max(v[0]for v in vs),max(v[1]for v in vs)],'Polygon bounds mismatch')
    for name in ('Overlay','MapDraw'):
        for r in sections[name]:
            tex=ref(r['resource_index'],'Resource',name)
            check(tex['kind']==1 and r['flags']in((0,1)if name=='Overlay'else(0,)) and r['w']>0 and r['h']>0,name+': invalid texture/flags/size')
            u,v=(r['u'],r['v'])if name=='Overlay' else(0,0)
            check(u<=tex['width']and r['w']<=tex['width']-u and v<=tex['height']and r['h']<=tex['height']-v,name+': source rect out of bounds')
            vec([r['x'],r['y']],name)
    for c in sections['Clip']:
        check(c['length']>0 and c['flags']<=31 and c['channel']in(0,1)and c['frame_count']>0,'Invalid clip profile')
        check(c['visibility']&0xf0==0 and c['visibility']&3!=3 and (c['visibility']>>2)&3!=3,'Invalid visibility encoding')
        keys=span(c['first_key'],c['key_count'],'Key','Clip',1,16)
        check(0<keys[0]['time']<=c['length'] if c['flags']&16 else keys[0]['time']==0,'Clip initial-key policy')
        check(all(k['time']>=0 and k['frame']<c['frame_count']for k in keys),'Invalid animation key')
        check(all(a['time']<b['time']for a,b in zip(keys,keys[1:])),'Unordered animation keys')
        check(not(c['flags']&2 and c['flags']&1),'Absolute-latched clips may not loop in current capability')
    binding_coverage=[0]*len(sections['AnimationBinding'])
    for index,p in enumerate(sections['ActorProfile']):
        check(p['execution_kind']in(1,2,3)and p['flags']<=1,'Unsupported actor execution profile')
        primary=ref(p['primary_resource'],'Resource','ActorProfile.primary')
        check(primary['kind']==1,'Actor primary resource must be texture')
        for key in ('sprite_position','sprite_offset','emote_offset','shadow_offset'):vec(p[key],'ActorProfile.'+key)
        frame_count=primary['columns']*primary['rows']
        check(p['initial_frame']<frame_count,'Initial actor frame outside grid')
        shadow=ref(p['shadow_resource'],'Resource','ActorProfile.shadow',True)
        check(shadow is None or shadow['kind']==1,'Shadow must be texture')
        check(not(p['flags']&1)or shadow is not None,'Visible shadow lacks texture')
        emote=ref(p['emote_resource'],'Resource','ActorProfile.emote',True)
        check(emote is None or emote['kind']==1,'Emote must be texture')
        check(emote is None and p['emote_initial_frame']==0 or emote is not None and p['emote_initial_frame']<emote['columns']*emote['rows'],'Initial emote frame outside grid')
        for field,channel,frames in [('idle_clip',0,frame_count),('emote_clip',1,0 if emote is None else emote['columns']*emote['rows'])]:
            clip=ref(p[field],'Clip','ActorProfile.'+field,True)
            check(clip is None or clip['channel']==channel and clip['frame_count']==frames,'Actor clip texture/channel mismatch')
        direction=span(p['direction_first'],p['direction_count'],'DirectionFrame','ActorProfile.direction')
        check(p['direction_count']in(0,4,8)and(p['execution_kind']!=1 or len(direction)==8)and(p['execution_kind']!=3 or len(direction)==4),'Unsupported directional actor profile')
        check(p['execution_kind']!=2 or p['direction_count']in(0,8),'Unsupported timeline actor direction count')
        check(all(f['frame']<frame_count for f in direction),'Direction frame outside grid')
        bindings=span(p['animation_binding_first'],p['animation_binding_count'],'AnimationBinding','ActorProfile.bindings')
        check(all(b['actor_profile_index']==index for b in bindings),'AnimationBinding profile range mismatch')
        for i in range(p['animation_binding_first'],p['animation_binding_first']+p['animation_binding_count']):binding_coverage[i]+=1
    check(all(n==1 for n in binding_coverage),'Unreferenced/overlapping animation binding ranges')
    seen_animation=set()
    for b in sections['AnimationBinding']:
        p=ref(b['actor_profile_index'],'ActorProfile','AnimationBinding');c=ref(b['clip_index'],'Clip','AnimationBinding')
        tex=ref(p['primary_resource'],'Resource','AnimationBinding')
        check(b['motion_state']<=5 and b['direction']<p['direction_count'] and c['channel']==0 and c['frame_count']==tex['columns']*tex['rows'],'Invalid movement animation binding')
        check(p['execution_kind']==1 and b['motion_state']<=4 or p['execution_kind']==2 and b['motion_state']<=3 or p['execution_kind']==3 and b['motion_state']in(0,4,5),'Unsupported actor motion binding')
        key=(b['actor_profile_index'],b['motion_state'],b['direction']);check(key not in seen_animation,'Duplicate animation binding');seen_animation.add(key)
    for index,profile in enumerate(sections['ActorProfile']):
        if profile['execution_kind']==1:
            for motion in range(4):
                for direction in range(8):check((index,motion,direction)in seen_animation,'Incomplete directional actor animation mapping')
            if any((index,4,d)in seen_animation for d in range(8)):
                check(all((index,4,d)in seen_animation for d in range(8)),'Incomplete actor walk mapping')
        if profile['execution_kind']==3:
            check(all((index,m,d)in seen_animation for m in (0,4,5)for d in range(4)),'Incomplete NPC actor animation mapping')
    for a in sections['ActorInstance']:
        p=ref(a['profile_index'],'ActorProfile','ActorInstance');c=ref(a['initial_clip'],'Clip','ActorInstance.initial',True)
        check(a['binding_kind']in(1,2)and a['flags']<=1,'Unknown actor binding kind/flags')
        check(p['execution_kind']!=3 or a['binding_kind']==2,'NPC profile requires replacement binding')
        string(a['display_name_string'],'ActorInstance.name',False);vec(a['position'],'ActorInstance.position');vec(a['direction'],'ActorInstance.direction')
        check(a['direction']!=[0,0],'Actor direction may not be zero')
        tex=ref(p['primary_resource'],'Resource','ActorInstance')
        check(c is None or c['channel']==0 and c['frame_count']==tex['columns']*tex['rows'],'Actor initial clip mismatch')
    for c in sections['CameraArea']:
        check(c['flags']==0 and all(0<v<=1000000 for v in c['extents']),'Invalid camera area');vec(c['center'],'CameraArea.center')
    flag_names=[]
    for f in sections['Flag']:
        flag_names.append(string(f['name_string'],'Flag.name',False));check(f['default_value']in(0,1)and f['flags']<=1,'Invalid flag definition')
    check(len(set(flag_names))==len(flag_names),'Duplicate flag name')
    seen_initial=set()
    for f in sections['InitialFlag']:
        flag=ref(f['flag_index'],'Flag','InitialFlag');check(f['value']in(0,1)and flag['flags']&1,'Invalid initial flag override')
        check(f['flag_index']not in seen_initial,'Duplicate initial flag override');seen_initial.add(f['flag_index'])
    for c in sections['Condition']:
        ref(c['flag_index'],'Flag','Condition');check(c['expected_value']in(0,1)and c['domain']==0,'Unsupported flag condition')
    for t in sections['Trigger']:
        check(t['flags']==0,'Unsupported trigger flags');polygon(t['first_vertex'],t['vertex_count'],'Trigger')
        span(t['condition_first'],t['condition_count'],'Condition','Trigger');ref(t['program_index'],'Program','Trigger');ref(t['actor_instance_index'],'ActorInstance','Trigger')
    for b in sections['Binding']:
        check(b['kind']in(range(1,13)if capabilities>=9 else range(1,10)if rules>=5 else range(1,8))and b['flags']==0 and(b['kind']==9 or b['auxiliary_index']==NONE),'Unsupported binding descriptor')
        if b['kind']==1:check(ref(b['target_index'],'Resource','Binding.music')['kind']==2 and b['value']==b['duration']==0,'Invalid music binding')
        elif b['kind']==2:check(b['target_index']==NONE and b['value']==0 and 0<b['duration']<=3600,'Invalid delayed-boundary binding')
        elif b['kind']==3:check(ref(b['target_index'],'Resource','Binding.periodic_shake')['kind']==2 and 0<b['value']<=1000000 and 0<b['duration']<=3600,'Invalid periodic camera shake binding')
        elif b['kind']==4:check(ref(b['target_index'],'Binding','Binding.stop_shake')['kind']==3 and b['value']==b['duration']==0,'Invalid stop camera shake binding')
        elif b['kind']==5:check(ref(b['target_index'],'Resource','Binding.stop_music')['kind']==2 and b['value']==b['duration']==0,'Invalid targeted stop music binding')
        elif b['kind']in(11,12):
            path=string(b['target_index'],'Binding.region',False)
            check(not path.startswith('/') and ':' not in path and '\\' not in path and all(x not in ('','.','..') for x in path.split('/')) and b['value']==0 and (0<=b['duration']<=3600 if b['kind']==11 else b['duration']==0),'Invalid checked music-region binding')
        elif b['kind']==10:check(b['target_index']==NONE and b['value']==b['duration']==0,'Invalid source white fade binding')
        elif b['kind']==8:check(0<=b['target_index']<64 and b['value']==b['duration']==0,'Invalid phone-ring binding')
        elif b['kind']==9:
            ref(b['auxiliary_index'],'Flag','Binding.body_flag')
            check(any(r['body_id']==b['target_index']for r in sections['BodyRule'])and b['value']in(0,1)and b['duration']==0,'Invalid deferred flag-body binding')
        else:check(ref(b['target_index'],'Resource','Binding.effect')['kind']==4 and b['value']==b['duration']==0,'Invalid checked world effect binding')
    for b in sections['Battle']:
        string(b['enemy_string'],'Battle.enemy',False);ref(b['actor_instance_index'],'ActorInstance','Battle.actor');ref(b['win_flag_index'],'Flag','Battle.win_flag',True)
        cutscene=string(b['win_cutscene_string'],'Battle.win_cutscene')
        check(not cutscene or not cutscene.startswith('/')and ':'not in cutscene and '\\'not in cutscene and all(x not in ('','.','..')for x in cutscene.split('/')),'Invalid battle cutscene path')
        check(ref(b['battle_resource_index'],'Resource','Battle.resource')['kind']==3,'Battle must reference checked pack')
        check(b['flags']<=15 and not(b['flags']&8 and b['flags']&2)and -100<=b['advantage']<=100,'Invalid battle config')
    owned_entries=set()
    for p in sections['MovementPath']:
        entries=span(p['first_entry'],p['entry_count'],'MovementEntry','MovementPath',1,16 if rules>=7 else 8)
        check(p['flags']<=(15 if rules>=7 else 3) and p['animation_motion']in(NO_ACTOR,4)and 0<p['speed']<=10000,'Invalid movement path')
        for i in range(p['first_entry'],p['first_entry']+len(entries)):
            check(i not in owned_entries,'Overlapping movement entries');owned_entries.add(i)
    check(len(owned_entries)==len(sections['MovementEntry']),'Orphan movement entries')
    for e in sections['MovementEntry']:
        vec(e['vector'],'MovementEntry.vector')
        check(e['kind']in(0,1),'Unknown movement entry')
        check(e['duration']==0 if e['kind']==0 else e['vector']==[0,0]and 0<e['duration']<=3600,'Invalid movement entry payload')
    for c in sections['Command']:
        op=c['opcode'];check(0<=op<(len(OPCODES)if capabilities==10 else 47 if capabilities==9 else 44 if capabilities==8 else 43 if rules>=7 else 42 if rules>=6 else 36 if rules>=5 else 35),'Unknown command opcode')
        allowed_flags=({12:15,10:15,17:3,16:1,19:1,32:1,33:7,35:1}if rules>=7 else{10:15,17:3,16:1,19:1,32:1,33:7,35:1}if rules>=5 else{16:1,19:1,32:1,33:1}).get(op,0)
        check(c['flags']&~allowed_flags==0 and (op in(37,38,47)or c['auxiliary_index']==NONE),'Unknown command flags/auxiliary field')
        if op in(16,19,32)and c['flags']&1:
            check(c['actor_index']==NO_ACTOR,'Inherited or dialogue-owned command must not bind actor')
        elif op in ACTOR_OPS:actor=ref(c['actor_index'],'ActorInstance','Command.actor')
        elif op==5:
            if c['actor_index']!=NO_ACTOR:ref(c['actor_index'],'ActorInstance','SetTalker.actor')
        else:check(c['actor_index']==NO_ACTOR,f"Unexpected command actor for opcode {op}: {c['actor_index']}")
        if op in TARGET_TABLE:target=ref(c['target_index'],TARGET_TABLE[op],'Command.target')
        elif op==10:
            if c['flags']&2:
                ref(c['target_index'],'ActorInstance','TurnActor.target')
                check((c['flags']&4 or c['vector'][0]==0)and(c['flags']&8 or c['vector'][1]==0),'Unused relative turn axis')
            else:check(c['target_index']==NONE and not(c['flags']&12),'Turn override without actor')
        elif op==17:
            check(c['target_index']==1,'Unsupported camera easing')
            check(not(c['flags']==1 and c['vector'][1])and not(c['flags']==2 and c['vector'][0]),'Unused partial camera axis')
        elif op in(44,45,46):check(0<c['target_index']<NONE,'Invalid typed progression identity')
        elif op==32:check(0<c['target_index']<NONE,'Invalid stable dialogue ID')
        elif op==36:check(c['target_index']<NONE,'Missing jump target')
        elif op==38:
            leader=string(c['target_index'],'BranchLeader.identity',False)
            check(re.fullmatch(r'[a-z][a-z0-9_]{0,63}',leader)is not None,'Invalid stable leader identity')
        elif op==48:check(0<=c['target_index']<NONE,'Invalid typed inventory template index')
        elif op==39:check(0<=c['target_index']<32,'Invalid dialogue choice group index')
        else:check(c['target_index']==NONE,'Unexpected command target')
        if op not in VECTOR_OPS:check(c['vector']==[0,0],'Unexpected command vector')
        else:vec(c['vector'],'Command.vector')
        if op not in VALUE_OPS:check(c['value']==0,'Unexpected command scalar')
        if op not in DURATION_OPS:check(c['duration']==0,'Unexpected command duration')
        if op in(7,31,37,47):check(c['value']in(0,1),'Invalid command boolean')
        if op==9:check(0<c['value']<=10000,'Invalid movement speed')
        if op==12:check(0<=c['value']<=1024,'Invalid jump height')
        if op==15:check(c['value']>=0,'Invalid shake magnitude')
        if op in DURATION_OPS and not(op==11 and rules>=7 and c['duration']==-1):
            maximum={10:10,11:10,12:60,15:60}.get(op,3600)
            check(0<=c['duration']<=maximum and (op in(4,17,23,30)or c['duration']>0),'Invalid command duration')
        if op in(8,34):check(target['kind']==2,'Audio playback must reference audio request resource')
        if op in(13,14):
            channel=0 if op==13 else 1;check(target['channel']==channel,'Wrong animation command channel')
            profile=ref(actor['profile_index'],'ActorProfile','Command')
            resource=ref(profile['primary_resource' if channel==0 else 'emote_resource'],'Resource','Command.clip')
            check(target['frame_count']==resource['columns']*resource['rows'],'Command clip does not match actor grid')
        if op in(18,24):check(target['actor_instance_index']==c['actor_index'],'Battle command actor mismatch')
        if op==20:check(actor['binding_kind']==1 or ref(actor['profile_index'],'ActorProfile','RestoreActor')['execution_kind']in(2,3),'RestoreActor requires player or NPC replacement')
        if op==27:check(c['vector']!=[0,0],'Zero actor direction')
        if op==29 and target['animation_motion']!=NO_ACTOR:
            profile=ref(actor['profile_index'],'ActorProfile','Command.path')
            check(profile['direction_count']>0 and all((actor['profile_index'],target['animation_motion'],d)in seen_animation for d in range(profile['direction_count'])),'Movement path lacks directional animation')
    coverage=[0]*len(sections['Command']);program_paths=set()
    for p in sections['Program']:
        path=string(p['source_path_string'],'Program.source_path',False)
        check(not path.startswith('/')and ':'not in path and '\\'not in path and all(x not in ('','.','..')for x in path.split('/'))and path not in program_paths,'Invalid/duplicate program source path')
        program_paths.add(path)
        cmds=span(p['first_command'],p['command_count'],'Command','Program',1,65536)
        check(0<p['phrase_count']<=65536 and all(c['phrase']<p['phrase_count']for c in cmds),'Program phrase out of bounds')
        check(cmds[-1]['opcode']in(23,24),'Program lacks supported terminal completion')
        check(cmds[-1]['opcode']!=23 or cmds[-1]['duration']>0,'World completion requires a positive camera return duration')
        branched=any(36<=c['opcode']<=41 or c['opcode']==47 for c in cmds)
        if branched:
            check(all(c['duration']>0 for c in cmds if c['opcode']==23),'Branch completion requires a positive camera return duration')
        else:check(all(c['duration']==0 for c in cmds[:-1]if c['opcode']==23),'Nonterminal DialogueDone duration must be zero')
        pending_timer=False
        for pc,c in enumerate(cmds):
            op=c['opcode']
            if op in(36,37,38,47):
                target=c['target_index']if op==36 else c['auxiliary_index']
                check(pc<target<len(cmds),'Branch must target a later command in the same program')
                check(cmds[target-1]['phrase']!=cmds[target]['phrase'],'Branch must target a phrase entry')
            if op in(40,43):check(pc+1<len(cmds)and cmds[pc+1]['opcode']==41,'OpenSave requires immediate submenu gate')
            if op==41:check(pc>0 and cmds[pc-1]['opcode']in(40,43),'Submenu gate requires a checked menu request')
            if (36<=op<=41 or op in(43,47)):check(not pending_timer,'Control transfer has unresolved phrase timer')
            if c['opcode']==3:
                check(not pending_timer,'Overlapping phrase timers');pending_timer=True
            if c['opcode']==26:
                check(pending_timer,'Timer wait without timer');pending_timer=False
            if c['opcode']==33:
                check(bool(c['flags']&1)==pending_timer,'Dialogue timer mode mismatch');pending_timer=False
        check(not pending_timer,'Unresolved phrase timer')
        for i in range(p['first_command'],p['first_command']+p['command_count']):coverage[i]+=1
    check(all(n==1 for n in coverage),'Command sections contain overlap or unreferenced commands')
    params={r['key']:r for r in sections['Rule']}
    check(len(params)==len(sections['Rule'])and set(params)=={k for k in RULE_TYPES if k<=27 if rules>=7 or k<=25},'Missing/duplicate/unknown rule parameter')
    for key,rule in params.items():
        check(rule['scalar_type']==RULE_TYPES[key],'Wrong rule type')
        if key in (24,25):check(-1<=rule['value']<=1,'Invalid room shake direction')
        elif key==22:check(0<=rule['value']<=3600,'Invalid room shake wait margin')
        else:check(0<rule['value']<=(3600 if key in (21,23)else 1000000),'Wrong rule range')
    check(params[22]['value']<params[21]['value']and (params[24]['value']!=0 or params[25]['value']!=0),'Invalid periodic camera shake rule relationships')
    check(params[14]['value']<=1 and params[16]['value']<=params[15]['value'],'Invalid camera weighting/threshold')
    check(abs(params[6]['value']+params[7]['value']-1)<1e-12,'Jump phase ratios must sum to one')
    xp=[r['required_total_exp']for r in sections['Experience']]
    check(len(xp)>0 and xp[0]==0 and all(a<b for a,b in zip(xp,xp[1:]))and xp[-1]<=0x7fffffff,'Invalid experience thresholds')
    check(len(sections['Scene'])==1,'Exactly one scene descriptor required')
    scene=sections['Scene'][0];check(scene['stable_id']==scene_id and scene['flags']==0 and scene['rule_profile_id']==1,'Scene identity/profile mismatch')
    for field in ('display_name_string','version_string','source_scene_string'):string(scene[field],'Scene.'+field,False)
    player=ref(scene['player_instance_index'],'ActorInstance','Scene.player');check(player['binding_kind']==1,'Scene player role mismatch')
    ref(scene['initial_program_index'],'Program','Scene.initial_program',True)
    polygon(scene['actor_hull_first'],scene['actor_hull_count'],'Scene.actor_hull')
    vec(scene['spawn'],'Scene.spawn');vec(scene['start_direction'],'Scene.direction')
    check(scene['start_direction']!=[0,0]and scene['initial_motion_state']<=3,'Invalid initial player motion')
    profile=ref(player['profile_index'],'ActorProfile','Scene.player');tex=ref(profile['primary_resource'],'Resource','Scene.player')
    check(scene['initial_frame']<tex['columns']*tex['rows'],'Scene initial frame outside player grid')
    span(scene['initial_flag_first'],scene['initial_flag_count'],'InitialFlag','Scene.initial_flags')
    span(scene['body_rule_first'],scene['body_rule_count'],'BodyRule','Scene.body_rules')
    ref(scene['default_camera_area'],'CameraArea','Scene.camera')
    for motion in range(4):
        for direction in range(8):check((player['profile_index'],motion,direction)in seen_animation,'Incomplete player animation mapping')


def validate_ir(ir):
    fields(ir,ROOT_FIELDS,'root')
    check(ir['schema']==1 and type(ir['schema'])is int,'Unsupported IR schema')
    check(type(ir['family'])is int and ir['family']==FAMILY,'Unsupported family')
    check(type(ir['rules'])is int and ir['rules']in(4,5,6,7,8)and type(ir['capabilities'])is int and ((ir['rules']<=7 and ir['capabilities']==ir['rules']) or (ir['rules']==7 and ir['capabilities']==8) or (ir['rules']==8 and ir['capabilities']==9) or (ir['rules']==8 and ir['capabilities']==10)),'Unsupported rules/capabilities')
    integer(ir['scene_id'],1,NONE,'scene_id');integer(ir['exporter_version'],1,NONE,'exporter_version');integer(ir['adapter_revision'],1,NONE,'adapter_revision')
    digest_hex(ir['upstream_commit'],40,'upstream_commit')
    check(type(ir['provenance'])is dict and type(ir['provenance'].get('sources'))is dict and ir['provenance']['sources'],'Missing provenance sources')
    for path,digest in ir['provenance']['sources'].items():
        check(type(path)is str and path and not Path(path).is_absolute()and '..'not in Path(path).parts and '\\'not in path,'Unsafe provenance path')
        digest_hex(digest,64,'provenance source hash')
    projections=ir['provenance'].get('projections',[])
    check(type(projections)is list and len(projections)<=16,'Invalid provenance projections')
    paths=set()
    for record in projections:
        check(type(record)is dict and set(record)=={'kind','path','identities','sha256'}and record['kind']=='catalog-bindings-v1'and record['path']=='content/native-resource-catalog.json','Unknown provenance projection')
        ids=record['identities']
        check(type(ids)is list and ids and all(type(i)is int and 0<i<=NONE for i in ids)and ids==sorted(set(ids)),'Invalid projected binding identities')
        digest_hex(record['sha256'],64,'projection source hash')
        check(record['path']not in paths and record['path']not in ir['provenance']['sources'],'Duplicate provenance dependency')
        paths.add(record['path'])
    validate_tables(ir['strings'],ir['sections'],ir['scene_id'],ir['rules'],ir['capabilities'])


def verify_provenance(ir, root=ROOT):
    from tools.catalog_projection import verify_projection
    catalog_path='content/native-resource-catalog.json'
    if ir['rules']>=8:
        records=[r for r in ir['provenance'].get('projections',[])if r['path']==catalog_path]
        check(catalog_path in ir['provenance']['sources']or len(records)==1,'Missing encounter catalog provenance')
        if records:
            # The scope is derived from this resource's actual consumers, not
            # a hard-coded encounter list or an optional provenance assertion.
            from tools.resource_catalog import load_ir,validate
            catalog=validate(load_ir(root/catalog_path))
            used={ir['strings'][r['path_string']]for r in ir['sections']['Resource']if r['kind']==3}
            rows=[r for r in catalog['bindings']if r['path']in used and r['role']in('Battle','EncounterBattle')]
            check({r['path']for r in rows}==used and records[0]['identities']==sorted(r['id']for r in rows),'Incomplete encounter catalog projection')
    for record in ir['provenance'].get('projections',[]):
        try:verify_projection(root,record)
        except (ValueError,KeyError,TypeError,OSError)as exc:raise ContentError(str(exc))from exc
    if ir['rules']>=7:
        check('content/pillow-source-bindings.json' in ir['provenance']['sources'],'Missing Pillow source binding provenance')
        check('content/programme-lowering-recipe.json' in ir['provenance']['sources'],'Missing programme recipe provenance')
        from tools.programme_lowering_recipe import verify_room
        if ir['capabilities']==10:
            from tools.house_return_inspection_programme import verify_extension
            verify_extension(ir,root)
        else:verify_room(ir,root)
    for name,expected in ir['provenance']['sources'].items():
        path=(root/name).resolve()
        try:path.relative_to(root.resolve())
        except ValueError as exc:raise ContentError('Provenance path escaped root')from exc
        check(path.is_file(),'Missing reviewed source: '+name)
        check(hashlib.sha256(path.read_bytes()).hexdigest()==expected,'Changed reviewed source: '+name)


def file_crc(data):
    check(len(data)>=HEADER_BYTES,'Truncated header')
    return zlib.crc32(data[:52]+bytes(4)+data[56:])


def compile_ir(ir):
    validate_ir(ir)
    strings=ir['strings'];sections=ir['sections']
    raw_strings=[s.encode('utf-8')+b'\0'for s in strings]
    string_pool=b''.join(raw_strings)
    data=bytearray(HEADER_BYTES+len(SCHEMAS)*DIRECTORY_BYTES)
    directory={}
    for name,(kind,stride,_) in SCHEMAS.items():
        if name=='StringRef':raw=bytes(len(strings)*stride);count=len(strings)
        elif name=='StringBytes':raw=string_pool;count=len(raw)
        else:raw=b''.join(encode_record(name,row)for row in sections[name]);count=len(sections[name])
        if count:
            pad=(-len(data))%8
            check(len(data)+pad+len(raw)<=MAX_BYTES,'Pack exceeds one MiB defensive limit')
            data+=bytes(pad);offset=len(data);data+=raw
        else:offset=0
        directory[name]=(offset,count,stride,len(raw))
        struct.pack_into('<HHIIIII',data,HEADER_BYTES+(kind-1)*DIRECTORY_BYTES,kind,1,offset,count,stride,len(raw),1)
    pool_offset=directory['StringBytes'][0];ref_offset=directory['StringRef'][0];cursor=pool_offset
    for i,raw in enumerate(raw_strings):
        struct.pack_into('<II',data,ref_offset+i*8,cursor,len(raw)-1);cursor+=len(raw)
    compiler_sha=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    fingerprint=hashlib.sha256(canonical({'ir':ir,'compiler_sha256':compiler_sha})).digest()
    header=struct.pack('<8sHH9IHHI20s32sIII8s',MAGIC,1,0,HEADER_BYTES,len(data),0x01020304,TARGET,FAMILY,ir['rules'],ir['capabilities'],ir['scene_id'],HEADER_BYTES,len(SCHEMAS),DIRECTORY_BYTES,0,bytes.fromhex(ir['upstream_commit']),fingerprint,ir['exporter_version'],ir['adapter_revision'],0,bytes(8))
    data[:HEADER_BYTES]=header
    struct.pack_into('<I',data,52,file_crc(data))
    blob=bytes(data)
    # Validate the quantized wire values as well, not only the authoring numbers.
    parse_pack(blob)
    manifest={'format':'ENCRMD01','major':1,'minor':0,'family':FAMILY,'rules':ir['rules'],'capabilities':ir['capabilities'],'scene_id':ir['scene_id'],'upstream_commit':ir['upstream_commit'],'bytes':len(blob),'pack_sha256':hashlib.sha256(blob).hexdigest(),'build_input_sha256':fingerprint.hex(),'compiler_sha256':compiler_sha,'sections':{name:{'offset':v[0],'count':v[1],'stride':v[2],'bytes':v[3]}for name,v in directory.items()},'sources':ir['provenance']['sources'],'cpp_compiler_invoked':False,'scope':'Reviewed native opening content; schema validity does not approve new behavior'}
    return blob,manifest


def parse_pack(data):
    check(type(data)in(bytes,bytearray)and HEADER_BYTES<=len(data)<=MAX_BYTES,'Invalid pack size')
    h=struct.unpack_from('<8sHH9IHHI20s32sIII8s',data)
    magic,major,minor,header_bytes,file_bytes,endian,target,family,rules,caps,scene_id,directory_offset,directory_count,entry_bytes,checksum,commit,input_sha,exporter_version,adapter_revision,flags,reserved=h
    check(magic==MAGIC and (major,minor)==(1,0),'Unsupported magic/format')
    check(header_bytes==HEADER_BYTES and file_bytes==len(data)and endian==0x01020304,'Header length/endian mismatch')
    check((target,family)==(TARGET,FAMILY)and rules in(4,5,6,7,8)and ((rules<=7 and caps==rules)or (rules==7 and caps==8)or(rules==8 and caps==9)or(rules==8 and caps==10)),'Unsupported target/family/rules/capabilities')
    check(any(commit)and any(input_sha),'Missing source fingerprint')
    check(scene_id>0 and exporter_version>0 and adapter_revision>0 and flags==0 and reserved==bytes(8),'Invalid header fields')
    check(directory_offset==HEADER_BYTES and directory_count==len(SCHEMAS)and entry_bytes==DIRECTORY_BYTES,'Invalid section directory')
    end=directory_offset+directory_count*entry_bytes
    check(end<=len(data),'Truncated section directory')
    check(checksum==file_crc(data),'CRC mismatch')
    ranges=[];directory={};names={v[0]:k for k,v in SCHEMAS.items()}
    for i in range(directory_count):
        kind,version,offset,count,stride,nbytes,section_flags=struct.unpack_from('<HHIIIII',data,directory_offset+i*entry_bytes)
        check(kind==i+1 and kind in names and kind not in directory,'Unknown/duplicate/reordered required section')
        name=names[kind]
        check(version==1 and section_flags==1 and stride==SCHEMAS[name][1],'Unsupported section version/flags/stride')
        check(count<=({'StringRef':65536,'StringBytes':MAX_BYTES}.get(name,LIMITS.get(name,0))),'Section count exceeds schema cap')
        if count==0:check(offset==0 and nbytes==0,'Noncanonical empty section')
        else:
            check(offset>=end and offset%8==0 and offset<=len(data),'Bad section offset/alignment')
            check(count<=(len(data)-offset)//stride and nbytes==count*stride,'Section range overflow/outside file')
            ranges.append((offset,offset+nbytes))
        directory[kind]=(offset,count,stride,nbytes)
    check(set(directory)==set(names),'Missing required section')
    cursor=end
    for start,stop in sorted(ranges):
        check(start>=cursor,'Overlapping sections')
        check(not any(data[cursor:start]),'Nonzero inter-section padding');cursor=stop
    check(cursor==len(data),'Unexpected trailing bytes')
    pool_offset,pool_count,_,pool_bytes=directory[2];ref_offset,ref_count,_,_=directory[1]
    check(pool_count>0 and ref_count>0,'Missing strings')
    strings=[];string_cursor=pool_offset
    for i in range(ref_count):
        offset,length=struct.unpack_from('<II',data,ref_offset+i*8)
        check(offset==string_cursor and length<=4096 and offset>=pool_offset and offset<=pool_offset+pool_bytes,'Noncanonical string pool reference')
        check(length<pool_offset+pool_bytes-offset,'String outside pool or missing terminator')
        check(data[offset+length]==0 and b'\0'not in data[offset:offset+length],'Invalid string terminator')
        try:s=bytes(data[offset:offset+length]).decode('utf-8','strict')
        except UnicodeError as exc:raise ContentError('Invalid UTF-8 string')from exc
        strings.append(s);string_cursor=offset+length+1
    check(string_cursor==pool_offset+pool_bytes,'Unreferenced string pool bytes')
    sections={}
    for name,(kind,stride,_)in SCHEMAS.items():
        if kind<=2:continue
        offset,count,_,_=directory[kind]
        sections[name]=[decode_record(name,data,offset+i*stride)for i in range(count)]
    validate_tables(strings,sections,scene_id,rules,caps)
    return {'schema':1,'family':family,'rules':rules,'capabilities':caps,'scene_id':scene_id,'upstream_commit':commit.hex(),'exporter_version':exporter_version,'adapter_revision':adapter_revision,'strings':strings,'sections':sections,'build_input_sha256':input_sha.hex()}


def atomic_write(path,data):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    temporary=path.with_suffix(path.suffix+'.tmp');temporary.write_bytes(data);temporary.replace(path)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('action',choices=['compile','verify','inspect'],nargs='?',default='compile')
    p.add_argument('--input',type=Path,default=ROOT/'content/native-opening.json')
    p.add_argument('--out',type=Path,default=ROOT/'romfs/data/opening.encroom')
    p.add_argument('--manifest',type=Path,default=ROOT/'build/native-content-manifest.json')
    a=p.parse_args()
    try:
        if a.action in('compile','verify'):
            check(a.input.stat().st_size<=16*1024*1024,'IR exceeds authoring limit')
            ir=json.loads(a.input.read_text(encoding='utf-8'));validate_ir(ir);verify_provenance(ir)
            if ir['rules']>=7:
                check('content/world-program-bindings.json'in ir['provenance']['sources'],'Missing world binding provenance')
                from tools.extract_native_content import Extractor
                from tools.world_program_bindings import load as load_world_bindings,verify_room
                source=Extractor(ROOT);verify_room(load_world_bindings(source),ir,source)
            blob,manifest=compile_ir(ir)
            manifest['ir_source']=a.input.resolve().relative_to(ROOT).as_posix()if a.input.resolve().is_relative_to(ROOT)else a.input.name
            manifest['ir_sha256']=hashlib.sha256(a.input.read_bytes()).hexdigest()
            if a.action=='compile':
                atomic_write(a.out,blob);atomic_write(a.manifest,(json.dumps(manifest,indent=2,ensure_ascii=False,sort_keys=True)+'\n').encode())
                print(f'Native data: {len(blob)} bytes, {len(SCHEMAS)} sections; no C++ generation or compilation')
            else:
                check(a.out.read_bytes()==blob,'Native pack is stale or modified')
                parse_pack(a.out.read_bytes());print('Verified native-content pack against external IR and reviewed source fingerprints')
        else:
            check(a.out.stat().st_size<=MAX_BYTES,'Pack exceeds one MiB defensive limit')
            result=parse_pack(a.out.read_bytes())
            print(json.dumps({'scene_id':result['scene_id'],'strings':len(result['strings']),'sections':{k:len(v)for k,v in result['sections'].items()}},indent=2))
        return 0
    except (ContentError,ValueError,KeyError,TypeError,OSError,OverflowError,struct.error)as e:
        print('NATIVE CONTENT ERROR: '+str(e),file=sys.stderr);return 1

if __name__=='__main__':raise SystemExit(main())
