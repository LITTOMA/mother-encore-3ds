#!/usr/bin/env python3
"""Complete source Ready roster and audited flag/AreaRoom lifecycle -> ENCFSCN1.

This does not admit the scene. JSON is authoring input only. Full native exports
are needed for extraction, never clean-clone compilation or runtime loading.
"""
from __future__ import annotations
import argparse, hashlib, re, struct, sys, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,SCENE,decode,stable,require,sha,read,write
IR=ROOT/'content/podunk-scene-lifecycle.json'
REVIEW=ROOT/'compatibility/reviews/podunk-scene-lifecycle-v0410.json'
OUTPUT=ROOT/'romfs/data/podunk.encfieldscene'
FORMATS={1:'2I',2:'B',3:'6I32s',4:'4I',5:'4I',6:'6I6f',7:'2I',8:'2I',9:'I',10:'I2f',11:'I32s'}
ROLES={'Scripts/misc/grass spawner.gd':1,'Scripts/Main/npc.gd':2,'Scripts/Main/Enemy Spawner.gd':3,'Scripts/misc/character_tint.gd':4,'Scripts/Main/character_sprite.gd':5,'Scripts/Main/SpriteDataFetcher.gd':6,'Scripts/Main/Flag Landmarks.gd':7,'Scripts/Main/Present.gd':20,'Scripts/Main/DroppedItem.gd':21,'Scripts/Main/RoomTypes/AreaRoom.gd':9,'Scripts/debug/DebugStartPos.gd':10,'Nodes/Ui/emotes.tscn::6':11,'Scripts/misc/dandelion spawner.gd':12,'Scripts/Main/Door.gd':13,'Scripts/UI/Button Prompt.gd':14,'Scripts/Main/Dead Bush.gd':15,'Scripts/Main/Interact Dialog.gd':18,'Scripts/Main/Openable Door.gd':16,'Scripts/misc/sparkles.gd':17,'Maps/Testing/phone.gd':19,'Scripts/misc/butterfly.gd':22,'Scripts/Main/CutsceneArea.gd':23,'Scripts/misc/birds.gd':24,'Scripts/Main/camarea.gd':25,'Nodes/Overworld/MusicChanger.tscn::3':26}
BASE='Scripts/Main/FlaggableObject.gd'
def extract(native,source):
    d,s=read(native),read(source);grass=read(ROOT/'content/podunk-scene.json')
    require(d['source']=='res://'+SCENE and s['scene']==SCENE and s['commit']==PIN and grass['export_sha256']==sha(native),'Lifecycle export identity rejected')
    require([d['godot'].get(k) for k in ('major','minor','patch','status')]==[3,6,2,'stable'],'Lifecycle engine rejected')
    nodes={n['path']:n for n in d['nodes']};resources={r['id']:r for r in d['resources']};states={r['source'][6:]:r for r in d['scene_states']}
    require(len(nodes)==8686,'Incomplete lifecycle scene')
    roots=[]
    def visit(root,filename):
        roots.append((root,filename))
        for n in states[filename]['nodes']:
            if n['instance'] is not None:
                p=n['path'].removeprefix('./');visit(p if root=='.' else root+'/'+p,resources[n['instance']['id']]['path'][6:])
    visit('.',SCENE);overrides={}
    for root,filename in sorted(roots,key=lambda x:x[0].count('/') if x[0]!='.' else -1,reverse=True):
        for n in states[filename]['nodes']:
            p=n['path'].removeprefix('./');p=root if p=='.' else p if root=='.' else root+'/'+p
            if p in nodes:overrides.setdefault(p,{}).update(decode(n['properties']))
    sources=dict(grass['sources']);inv=read(ROOT/'compatibility/upstream-inventory.json')['files']
    for p in [BASE,'Scripts/Main/ItemHolder.gd','Scripts/Main/DisappearingFlaggableObject.gd','Scripts/global/globalData.gd']:
        sources[p]=inv[p]['sha256']
    for p,h in sources.items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Changed lifecycle source '+p)
    roster=[];landmarks=[];flags=[];debug=[];rooms=[]
    bindings=list(grass['pending'])+[dict(stable_id=b['stable_id'],node=b['node'],script='Scripts/misc/grass spawner.gd',source_sha256=sources['Scripts/misc/grass spawner.gd'],ready_ordinal=b['ready_ordinal']) for b in grass['grass']]
    for b in sorted(bindings,key=lambda x:x['ready_ordinal']):
        p=b['node'];n=nodes[p];props=overrides.get(p,{});role=ROLES.get(b['script'],0);profile=0xffffffff
        if role==7:
            profile=len(landmarks);landmarks.append(dict(id=b['stable_id'],appear=props.get('appear_flag',''),disappear=props.get('disappear_flag',''),delete_if_hidden=props.get('delete_if_hidden',True),initial_visible=decode(n['properties']).get('visible',True)))
        elif role in (8,20,21):
            profile=len(flags);bits=sum((1<<i)*int(props.get(k,False)) for i,k in enumerate(['is_object_flag','emit_flag_updated_signal','reset_when_leaving_region','reset_when_leaving_area']))
            flags.append(dict(id=b['stable_id'],flag=props.get('flag','') or '',flags=bits,leaf_script=b['script']))
        elif role==9:
            profile=len(rooms);text=(ROOT/'upstream/MOTHER-Encore'/b['script']).read_text(encoding='utf-8')
            def dictionary(name):
                raw=re.search(r'const '+name+r'\s*:=\s*\{(.*?)\}',text,re.S)[1]
                pairs=re.findall(r'"([^"]+)"\s*:\s*"([^"]+)"',raw);require(len(pairs)>0,'Missing source AreaRoom table');return pairs
            magicant=re.search(r'var is_magicant = \(_region_name == "([^"]+)"\)',text)[1];flying=re.search(r'globaldata.flags\["([^"]+)"\]',text)[1]
            color=re.search(r'area_bg_color := Color\("([0-9a-f]{6})"\)',text)[1]
            rooms.append(dict(id=b['stable_id'],name=n['name'],region=props.get('_region_name',''),sub_area=props.get('_is_sub_area',False),offset=props.get('_player_map_offset',[0,0]),color=[int(color[i:i+2],16)/255 for i in (0,2,4)]+[1],magicant=magicant,flying_flag=flying,overrides=dictionary('MAP_AREA_OVERRIDES'),visits=dictionary('REGION_VISIT_FLAGS')))
        elif role==10:
            profile=len(debug);debug.append(dict(id=b['stable_id'],position=decode(n['world_transform'])[-1]))
        roster.append(dict(id=b['stable_id'],node=p,name=n['name'],ready=b['ready_ordinal'],role=role,profile=profile,script=b['script'],sha256=b['source_sha256']))
    text=(ROOT/'upstream/MOTHER-Encore/Scripts/global/globalData.gd').read_text(encoding='utf-8')
    raw=re.search(r'var flag_names := \[(.*?)\n\s*\]',text,re.S)[1];raw=re.sub(r'#.*','',raw);registry=re.findall(r'"([^"]+)"',raw)
    require(len(registry)==len(set(registry)) and len(registry)>100,'Source normal flag registry rejected')
    require(len(roster)==2157 and len(landmarks)==13 and len(flags)==19 and len(rooms)==1 and len(debug)==1,'Lifecycle coverage differs')
    embedded={a['script']:a['embedded_script']['sha256'] for a in s['script_attachments'] if '::' in a['script']}
    for n in roster:
        if '::' in n['script']:require(n['sha256']==embedded[n['script']],'Embedded lifecycle source proof differs')
    write(IR,dict(schema=1,kind='encore.field-scene-lifecycle.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),source_sha256=sources[SCENE],scene_admitted=False,native_sha256=sha(native),sources=sources,embedded_sources=embedded,roster=roster,landmarks=landmarks,flaggables=flags,rooms=rooms,debug=debug,normal_flags=registry))
    review=read(REVIEW);require(review['commit']==PIN and review['scene']==SCENE and review['scene_admitted'] is False,'Lifecycle review identity changed')
    updated=read(IR);bush=read(ROOT/'content/native-field-dead-bush.json');rows=[r for r in updated['roster']if r['role']==15]
    require(len(rows)==len(bush['records'])==13,'DeadBush lifecycle source coverage')
    for r,n in zip(rows,bush['records']):require(r['id']==n['id']and r['node']==n['node']and r['ready']==n['ready_ordinal']and r['sha256']==bush['script_sha256'],'DeadBush actual lifecycle mapping changed')
    review['ir_sha256']=sha(IR);review['typed_bridges_added']=[v for v in review['typed_bridges_added']if 'DeadBush'not in v]+['13 DeadBush actual onready, VisibilityNotifier, source animation, prompt and deferred Roots callbacks']
    review['dead_bush_review']=dict(ir_sha256=sha(ROOT/'content/native-field-dead-bush.json'),source_sha256=bush['script_sha256'],semantics=bush['semantics'],unsupported=bush['unsupported'])
    inter=read(ROOT/'content/native-field-interact-dialog.json');rr=[v for v in updated['roster']if v['role']==18]
    require(len(rr)==len(inter['records'])==33,'InteractDialog lifecycle source coverage')
    for row,n in zip(rr,inter['records']):require(row['id']==n['id']and row['node']==n['node']and row['ready']==n['ready']and row['sha256']==inter['sources'][inter['script']],'InteractDialog actual lifecycle mapping changed')
    review['typed_bridges_added']=[v for v in review['typed_bridges_added']if 'InteractDialog'not in v]+['33 InteractDialog source Ready/flags/prompt/dialogue/item/thought consumers']
    review['interact_dialog_review']=dict(ir_sha256=sha(ROOT/'content/native-field-interact-dialog.json'),semantics=inter['semantics'],pending=inter['pending'])
    families=[('native-field-present',20,'holders','ready_ordinal',16),('native-field-dropped',21,'bindings','ready_ordinal',3),('podunk-openable-door',16,'records','ready',10),('podunk-sparkles',17,'records','ready',22),('native-field-payphone',19,'records','ready_ordinal',4),('native-field-butterfly',22,'bindings','ready_ordinal',94),('native-field-cutscene-area',23,'bindings','ready_ordinal',15),('podunk-birds',24,'records','ready',54),('podunk-camera-area',25,'records','ready',1),('native-field-music-changer',26,'bindings','ready_ordinal',13)]
    proofs={}
    for name,role,records_key,ordinal_key,count in families:
        family=read(ROOT/'content'/f'{name}.json');actual=[row for row in updated['roster']if row['role']==role]
        require(family['commit']==PIN and family['scene']==SCENE and len(actual)==len(family[records_key])==count,'Lifecycle family source coverage '+name)
        for row,n in zip(actual,family[records_key]):
            source_proof=family['sources'].get(row['script'])==row['sha256']
            if '::'in row['script']:
                leaf=row['script'].split('::')[0];source_proof=row['script']==family['script'] and family['sources'].get(leaf)==updated['sources'].get(leaf) and updated['embedded_sources'].get(row['script'])==row['sha256']
            require(row['id']==n['id'] and row['node']==n['node'] and row['ready']==n[ordinal_key] and source_proof,'Lifecycle exact family source differs '+name)
        proofs[name]=dict(ir_sha256=sha(ROOT/'content'/f'{name}.json'),role=role,count=count,semantics=family.get('semantics',[]),pending=family.get('pending',family.get('unsupported',[])))
    review['field_object_reviews']=proofs
    write(REVIEW,review)

def pack(ir):
    strings=[];lookup={};blob=bytearray();rows={k:[] for k in FORMATS}
    def string(v):
        if v not in lookup:
            lookup[v]=len(strings);raw=v.encode('utf-8');strings.append((len(blob),len(raw)));blob.extend(raw+b'\0')
        return lookup[v]
    scene=string(ir['scene'])
    for n in ir['roster']:rows[3].append((n['id'],string(n['node']),string(n['name']),n['ready'],n['role'],n['profile'],bytes.fromhex(n['sha256'])))
    for n in ir['landmarks']:rows[4].append((n['id'],string(n['appear']),string(n['disappear']),int(n['delete_if_hidden'])|(int(n['initial_visible'])<<1)))
    for n in ir['flaggables']:rows[5].append((n['id'],string(n['flag']),n['flags'],string(n['leaf_script'])))
    for n in ir['rooms']:
        rows[6].append((n['id'],string(n['name']),string(n['region']),int(n['sub_area']),string(n['magicant']),string(n['flying_flag']),*n['offset'],*n['color']))
        rows[7].extend((string(k),string(v)) for k,v in n['overrides']);rows[8].extend((string(k),string(v)) for k,v in n['visits'])
    rows[9]=[(string(v),) for v in ir['normal_flags']];rows[10]=[(n['id'],*n['position']) for n in ir['debug']]
    rows[11]=[(string(k),bytes.fromhex(v)) for k,v in dict(ir['sources'],**ir['embedded_sources']).items()]
    # Leaf script paths accompany the complete roster via the source proof:
    # add the exact path index after each record in its own section.
    rows[12]=[(string(n['script']),) for n in ir['roster']]
    formats=dict(FORMATS);formats[12]='I';rows[1]=strings;rows[2]=[(v,) for v in blob]
    result=bytearray(128+24*len(formats));directory=[]
    for k,fmt in formats.items():
        data=b''.join(struct.pack('<'+fmt,*v) for v in rows[k]);directory.append((k,len(rows[k]),struct.calcsize('<'+fmt),len(result),len(data),0));result.extend(data)
    struct.pack_into('<8s8I',result,0,b'ENCFSCN1',1,128,len(result),len(formats),zlib.crc32(result[128+24*len(formats):]),0x454e001c,4,ir['scene_id'])
    result[40:60]=bytes.fromhex(PIN);result[60:92]=bytes.fromhex(ir['source_sha256']);result[92:124]=hashlib.sha256(IR.read_bytes()).digest();struct.pack_into('<I',result,124,scene)
    for i,v in enumerate(directory):struct.pack_into('<6I',result,128+24*i,*v)
    return bytes(result)
def main():
    parser=argparse.ArgumentParser();parser.add_argument('action',choices=['extract','compile','verify']);parser.add_argument('--native',type=Path);parser.add_argument('--source',type=Path);a=parser.parse_args()
    if a.action=='extract':require(a.native and a.source,'Explicit full native/source exports required');extract(a.native,a.source);return
    ir=read(IR);review=read(REVIEW);inv=read(ROOT/'compatibility/upstream-inventory.json')
    require(ir['schema']==1 and ir['commit']==PIN and ir['scene']==SCENE and ir['scene_admitted'] is False and inv['commit']==PIN,'Lifecycle source identity rejected')
    require(review['ir_sha256']==sha(IR) and review['commit']==PIN and review['scene_admitted'] is False,'Lifecycle semantic review missing/stale')
    for p,h in ir['sources'].items():require(h==inv['files'][p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Lifecycle source pin rejected '+p)
    data=pack(ir)
    if a.action=='verify':require(OUTPUT.read_bytes()==data,'Lifecycle resource differs')
    else:OUTPUT.parent.mkdir(parents=True,exist_ok=True);OUTPUT.write_bytes(data)
    print('Field lifecycle:',len(data),'bytes;',len(ir['roster']),'ordered script Ready entries; scene admitted=False')
if __name__=='__main__':
    try:main()
    except (ValueError,KeyError,OSError) as e:sys.exit('FIELD SCENE LIFECYCLE ERROR: '+str(e))
