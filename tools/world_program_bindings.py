"""Checked namespace/source selectors for the existing opening-world adapter.

No runtime script dispatch or new action semantics. The source node, original
YAML references, native receipt slots and object methods must agree with the IR.
"""
import hashlib
import json
from pathlib import Path
import re
import struct
import yaml

ROOT=Path(__file__).resolve().parents[1]
IR='content/world-program-bindings.json'
STAGES=('initial','attack','postwin','melody','guard')
# Supported method adapters are execution/schema contracts, not object names.
METHOD_KINDS={'play_music':1,'delayed_start':3,'stop_shake':4,
              'stop_music_immediately':5,'appear':6,'disappear':7}

def require(value,message):
    if not value:raise ValueError(message)

def fields(value,expected):
    require(type(value)is dict and set(value)==set(expected),'Unknown/missing world binding fields')

def safe(path):
    return type(path)is str and 0<len(path)<=512 and all(32<=ord(c)<=126 and c not in '\\:?#'for c in path)and all(p not in ('','.','..')for p in path.split('/'))

def load_json(path):
    def unique(pairs):
        value={}
        for key,item in pairs:
            require(key not in value,'Duplicate world binding JSON key');value[key]=item
        return value
    require(Path(path).stat().st_size<=512*1024,'World binding IR exceeds authoring limit')
    return json.loads(Path(path).read_text(encoding='utf-8'),object_pairs_hook=unique)

def block(text,path):
    matches=[]
    for row in re.finditer(r'^\[node ([^\n]+)\]\n(.*?)(?=^\[|\Z)',text,re.M|re.S):
        a=dict(re.findall(r'(\w+)="([^"]*)"',row[1]));parent=a.get('parent')
        actual='.'if parent is None else a['name']if parent=='.'else parent+'/'+a['name']
        if actual==path:matches.append((row[1],row[2]))
    require(len(matches)==1,'Unknown/ambiguous source node: '+path);return matches[0]

def property(text,name):
    values=re.findall(r'^'+re.escape(name)+r' = (.*)$',text,re.M)
    require(len(values)==1,'Missing/ambiguous source property: '+name)
    return json.loads(values[0])

def phrase(doc,label):
    require(type(label)is str and label in doc and type(doc[label])is dict,'Unknown source phrase selector');return doc[label]

def motion(ir,animation):
    matches=[r['state']for r in ir['animation']['motions']if r['animation']==animation]
    require(len(matches)<=1,'Ambiguous animation motion binding');return matches[0]if matches else 65535

def clip_name(prefix,animation,direction=None):
    return ' '.join(v for v in (prefix,animation,direction)if v)

def validate_animation(ir,ex,docs):
    a=ir['animation'];fields(a,('sprite_review','sprite_manifest','direction_review','actor_script','npc_scene','party_source','directions','motions','assets','base_profiles','actor_walk','emotes'))
    for key in ('sprite_review','sprite_manifest','direction_review'):require(safe(a[key]),'Unsafe animation review/manifest')
    for key in ('actor_script','npc_scene','party_source'):require(a[key]in ir['sources'],'Unreviewed animation source')
    recipe=ex.document(a['sprite_review']);manifest=ex.document(a['sprite_manifest']);review=ex.document(a['direction_review'])
    require(recipe['commit']==ir['commit']and manifest['recipe']==recipe and review['commit']==ir['commit'],'Animation reviewed source mismatch')
    script=ex.text(recipe['character_sprite_script']);actor_script=ex.text(a['actor_script'])
    directional=script.split('if animationSize == 2:',1)[1].split('else:',1)[1].split('directionalAnims.append',1)[0]
    tags=re.findall(r'(\d+):\s*\n\s*dirTitle = " ([^"]+)"',directional)
    require(tags==[(str(i),name)for i,name in enumerate(a['directions'])]and a['directions']==review['directions'],'Source direction index/name order mismatch')
    idle=re.findall(r'^var _idle_anim := "([^"]+)"',actor_script,re.M);talk=re.findall(r'if talking.*?character_sprite.travel\("([^"]+)"\)',actor_script,re.S)
    # Execution selectors 0/4/5 are schema; their source animation names are data.
    require(type(a['motions'])is list and len(a['motions'])==3,'Missing animation motion bindings')
    movements={v['animation']for doc in docs.values()for p in doc.values()for v in p.get('actorsmove',{}).values()if 'animation'in v}
    for row,(state,selector)in zip(a['motions'],((0,'idle'),(4,'movement'),(5,'talk'))):
        fields(row,('state','animation','selector'));require(type(row['state'])is int and row['state']==state and row['selector']==selector,'Unsupported/reordered animation motion')
        require(row['animation']in (idle if selector=='idle'else talk if selector=='talk'else movements),'Motion/source animation mismatch')
    require(len({r['animation']for r in a['motions']})==3 and movements=={a['motions'][1]['animation']},'Unsupported source movement animation')
    require(type(a['assets'])is list and len(a['assets'])==4,'Missing actor asset bindings')
    roles=set();paths=set();keys=set();emote=block(ex.text(recipe['emote_scene']),'.')[1]
    for index,row in enumerate(a['assets']):
        fields(row,('id','role','path','recipe_key','grid'));require(type(row['id'])is int and row['id']==index+1 and safe(row['path'])and row['path'].endswith('.t3x'),'Invalid/reordered actor resource identity')
        require(type(row['role'])is str and row['role']not in roles and row['path']not in paths and row['recipe_key']not in keys,'Duplicate actor asset binding')
        roles.add(row['role']);paths.add(row['path']);keys.add(row['recipe_key'])
        require(row['recipe_key']in recipe and type(recipe[row['recipe_key']])is str and recipe[row['recipe_key']]in ir['sources'],'Unknown actor asset recipe/source selector')
        name=Path(row['path']).name;require('romfs/'+row['path']==a['sprite_manifest'].rsplit('/',1)[0]+'/'+name and name in manifest['outputs'],'Unknown actor asset output mapping')
        output=ex.file('romfs/'+row['path']);require(hashlib.sha256(output.read_bytes()).hexdigest()==manifest['outputs'][name]['sha256'],'Stale actor resource bytes')
        raw=output.read_bytes();source=ex.source(recipe[row['recipe_key']]).read_bytes()
        require(len(raw)>=21 and len(source)>=24 and source[:8]==b'\x89PNG\r\n\x1a\n','Invalid actor texture source/header')
        size=struct.unpack('>II',source[16:24]);image_size=struct.unpack('<HH',raw[5:9])
        width=1<<((raw[2]&7)+3);height=1<<(((raw[2]>>3)&7)+3)
        require(raw[:2]==b'\x01\x00'and not(raw[2]&0xc0)and raw[3:5]==b'\x00\x00'and raw[17]==0 and image_size==size and len(raw)==21+width*height*4 and int.from_bytes(raw[18:21],'little')==width*height*4,'Actor output/source image format or dimensions mismatch')
        if row['recipe_key']=='texture':expected=recipe['grid']
        elif row['recipe_key']=='lamp_texture':expected=recipe['lamp_layout']['grid']
        elif row['recipe_key']=='emote_texture':expected=[property(emote,k)for k in ('hframes','vframes')]
        elif row['recipe_key']=='shadow_texture':expected=[1,1]
        else:raise ValueError('Unsupported actor texture recipe field')
        require(row['grid']==expected and all(type(v)is int and v>0 for v in row['grid']),'Actor asset/source grid mismatch')
    require(set(manifest['outputs'])=={Path(r['path']).name for r in a['assets']},'Incomplete actor output mapping')
    require(roles=={ir['actors'][0]['resource_role'],ir['actors'][1]['resource_role'],'emote','shadow'},'Unknown/missing actor resource role')
    require(next(r['recipe_key']for r in a['assets']if r['role']=='emote')=='emote_texture'and next(r['recipe_key']for r in a['assets']if r['role']=='shadow')=='shadow_texture','Shared actor resource/source mapping mismatch')
    require(type(a['base_profiles'])is list and len(a['base_profiles'])==2,'Missing base actor bindings')
    for index,row in enumerate(a['base_profiles']):
        fields(row,('actor_id','execution_kind','animation_source','prefix','idle_animation','emote_animation','clips'))
        require(type(row['actor_id'])is int and row['actor_id']==index+1 and type(row['execution_kind'])is int and row['execution_kind']==index+1,'Base actor stable/execution order mismatch')
        source=ir['actors'][index];asset=[r for r in a['assets']if r['role']==source['resource_role']];require(len(asset)==1,'Unknown base actor resource role')
        require(asset[0]['recipe_key']==('texture'if row['execution_kind']==1 else'lamp_texture'),'Base actor/source texture mapping mismatch')
        require(row['animation_source']in ir['sources']and row['idle_animation']==idle[0],'Base actor animation/idle mismatch')
        if index==0:
            defaults=re.findall(r'character_sprite.set_animation\("res://([^"]+)"',actor_script)
            require(row['animation_source']==a['party_source']and defaults==[row['animation_source']]and row['prefix']==''and row['clips']==[],'Player native animation binding mismatch')
        else:
            require(row['emote_animation']is None,'Unsupported scripted base emote binding')
            require(row['animation_source']==recipe['lamp_yaml']and row['prefix']==source['alias'],'Scripted base actor prefix/source mismatch')
            data=yaml.safe_load(ex.text(row['animation_source']));require(type(row['clips'])is list and [r['animation']for r in row['clips']]==list(data['animations']),'Base actor clip coverage/order mismatch')
            for c in row['clips']:
                fields(c,('animation','flags'));track=data['animations'][c['animation']]
                require(track['type']==1 and len(track['directions'])==1 and c['flags']==(4 if c['animation']==idle[0]else 2),'Unsupported scripted base clip behavior')
    for row in ir['npc_profiles']:
        data=yaml.safe_load(ex.text(row['animation_source']));require(row['idle_animation']==idle[0],'NPC idle source mismatch')
        attrs,_=block(ex.text(ir['scene']),ir['actors'][row['actor_id']-1]['node']);instance=re.search(r'instance=ExtResource\( (\d+) \)',attrs)
        refs={int(i):p for p,i in re.findall(r'^\[ext_resource path="res://([^"]+)"[^\n]* id=(\d+)\]',ex.text(ir['scene']),re.M)}
        require(instance is not None and refs[int(instance[1])]==a['npc_scene'],'NPC scene inheritance mismatch')
        require(type(row['clips'])is list and len(row['clips'])==len(data['animations'])and {c['animation']for c in row['clips']}==set(data['animations']),'NPC clip coverage/duplicates mismatch')
        for c in row['clips']:
            fields(c,('animation','flags'));require(type(c['flags'])is int and c['flags']==(4 if row['execution_kind']==2 and c['animation']==idle[0]else 0),'Unsupported NPC native clip flags')
            count=len(data['animations'][c['animation']]['directions']);require(count==(1 if row['execution_kind']==2 else 4),'Unsupported NPC animation direction count')
        if row['execution_kind']==3:require([c['animation']for c in row['clips']]==[m['animation']for m in a['motions']],'Directional motion clip order mismatch')
        else:require([c['animation']for c in row['clips']]==[row['idle_animation']]+[name for name in data['animations']if name!=row['idle_animation']],'Scripted NPC clip order mismatch')
    walk=a['actor_walk'];fields(walk,('actor_id','prefix','animation','motion','receipt','yaml_index','animation_index'))
    require(walk['actor_id']==a['base_profiles'][0]['actor_id']and walk['prefix']==ir['actors'][walk['actor_id']-1]['alias']+' Actor'and walk['animation']==a['motions'][1]['animation']and walk['motion']==a['motions'][1]['state'],'Player actor movement mapping mismatch')
    from tools.doll_dialogue import decode
    require(safe(walk['receipt'])and type(walk['yaml_index'])is int and type(walk['animation_index'])is int,'Invalid player native receipt selector')
    native=decode(ex.document(walk['receipt']));require(0<=walk['yaml_index']<len(native['yaml'])and native['yaml'][walk['yaml_index']]==yaml.safe_load(ex.text(a['party_source']))and walk['animation_index']==walk['yaml_index']-1,'Player native receipt slot mismatch')
    require(type(a['emotes'])is list and len(a['emotes'])==3,'Missing source emote bindings')
    for index,row in enumerate(a['emotes']):
        fields(row,('animation','phase','actor_id','receipt_key'));require(row['phase']==('initial','attack','postwin')[index]and type(row['animation'])is str,'Unknown emote phase/name')
        require(len(re.findall(r'^resource_name = "'+re.escape(row['animation'])+'"$',ex.text(recipe['emote_scene']),re.M))==1,'Unknown original emote animation')
        require(type(row['actor_id'])is int and 1<=row['actor_id']<=len(ir['actors']),'Unknown emote actor reference')
        if index==0:require(row['actor_id']==1 and row['receipt_key']is None and a['base_profiles'][0]['emote_animation']==row['animation'],'Base player emote mismatch')
        else:require(row['receipt_key']==row['animation'],'Emote native receipt key mismatch')
        refs={(ir['actors'][row['actor_id']-1]['source_alias'],row['animation'])}
        calls={(name,emote)for p in docs[program(ir,row['phase'])['id']].values()for name,emote in p.get('actorsemote',{}).items()}
        require(refs&calls,'Emote/source actor call mismatch')
    return a

def verify_animation(ir,room,ex):
    a=ir['animation'];s=room['sections'];strings=room['strings'];recipe=ex.document(a['sprite_review'])
    for binding in a['assets']:
        value=s['Resource'][binding['id']-1];source=ex.source(recipe[binding['recipe_key']]).read_bytes();size=struct.unpack('>II',source[16:24])
        require(value['stable_id']==binding['id']and strings[value['path_string']]==binding['path']and (value['width'],value['height'])==size and [value['columns'],value['rows']]==binding['grid'],'Compiled actor resource identity/source mismatch')
    from tools.world_geometry import f32
    from tools.doll_dialogue import decode
    def check_clip(index,native,frames,extra=0,channel=0):
        require(type(index)is int and 0<=index<len(s['Clip']),'Missing compiled actor clip')
        clip=s['Clip'][index];flags=int(native['loop'])|extra|(16 if native['keys'][0][0]>0 else 0)
        require(clip['stable_id']==index+1 and clip['length']==f32(native['length'])and clip['frame_count']==frames and clip['channel']==channel and clip['flags']==flags,'Compiled native clip metadata mismatch')
        keys=s['Key'][clip['first_key']:clip['first_key']+clip['key_count']]
        require(keys==[dict(time=f32(t),frame=int(frame))for t,frame in native['keys']],'Compiled native actor key mismatch')
    for binding in a['base_profiles']:
        identity=binding['actor_id'];value=s['ActorProfile'][identity-1];asset=next(r for r in a['assets']if r['role']==ir['actors'][identity-1]['resource_role'])
        require(value['stable_id']==identity and value['execution_kind']==binding['execution_kind']and value['primary_resource']==asset['id']-1,'Compiled base actor identity mismatch')
        if binding['execution_kind']==2:
            data=yaml.safe_load(ex.text(binding['animation_source']));idle=value['idle_clip']
            for index,row in enumerate(binding['clips']):
                direction=data['animations'][row['animation']]['directions'][0];time=direction[0];keys=[]
                for frame,duration in direction[1:]:keys.append((time,frame-1));time+=duration
                check_clip(idle+index,dict(loop=False,length=time,keys=keys),data['size'][0]*data['size'][1],row['flags'])
    walk=a['actor_walk'];native=decode(ex.document(walk['receipt']));profile=s['ActorProfile'][walk['actor_id']-1]
    bindings=s['AnimationBinding'][profile['animation_binding_first']:profile['animation_binding_first']+profile['animation_binding_count']]
    rows=[r for r in bindings if r['motion_state']==walk['motion']]
    require([r['direction']for r in rows]==list(range(len(a['directions']))),'Compiled player actor direction/motion mismatch')
    data=native['yaml'][walk['yaml_index']]
    for row,name in zip(rows,a['directions']):check_clip(row['clip_index'],native['animations'][walk['animation_index']][clip_name('',walk['animation'],name)],data['size'][0]*data['size'][1])
    for binding in ir['npc_profiles']:
        actor_row=ir['actors'][binding['actor_id']-1];value=s['ActorProfile'][binding['actor_id']-1];native=decode(ex.document(binding['receipt']));data=native['yaml'][binding['yaml_index']];clips=native['animations'][binding['animation_index']];frames=data['size'][0]*data['size'][1]
        if binding['execution_kind']==2:
            for offset,row in enumerate(binding['clips']):check_clip(value['idle_clip']+offset,clips[row['animation']],frames,row['flags'])
        else:
            expected=[(motion(ir,row['animation']),direction,clips[clip_name('',row['animation'],name)],row['flags'])for row in binding['clips']for direction,name in enumerate(a['directions'][:len(data['animations'][row['animation']]['directions'])])]
            rows=s['AnimationBinding'][value['animation_binding_first']:value['animation_binding_first']+value['animation_binding_count']]
            require(len(rows)==len(expected),'Compiled NPC clip coverage mismatch')
            for row,(state,direction,clip,flags)in zip(rows,expected):
                require(row['actor_profile_index']==binding['actor_id']-1 and row['motion_state']==state and row['direction']==direction,'Compiled NPC animation mapping mismatch');check_clip(row['clip_index'],clip,frames,flags)
    frames=next(r['grid'][0]*r['grid'][1]for r in a['assets']if r['recipe_key']=='emote_texture')
    for binding in a['emotes']:
        if binding['phase']=='initial':
            text=ex.text(recipe['emote_scene']);match=re.findall(r'^\[sub_resource type="Animation" id=\d+\]\nresource_name = "'+re.escape(binding['animation'])+r'"\n(.*?)(?=^\[|\Z)',text,re.M|re.S)
            require(len(match)==1,'Unknown source emote track');text=match[0]
            times=re.findall(r'"times": PoolRealArray\( ([^)]*) \)',text);values=re.findall(r'"values": \[ ([^]]*) \]',text)
            require(len(times)==len(values)==1,'Unsupported source emote timeline')
            clip=dict(loop=False,length=property(text,'length'),keys=list(zip([float(v)for v in times[0].split(',')],[int(v)for v in values[0].split(',')])));index=s['ActorProfile'][binding['actor_id']-1]['emote_clip']
        else:
            receipt=next(r['receipt']for r in ir['npc_profiles']if r['phase']==binding['phase']);clip=decode(ex.document(receipt))[binding['receipt_key']]
            if binding['phase']=='attack':index=s['ActorProfile'][binding['actor_id']-1]['emote_clip']
            else:
                matches=[i for i,c in enumerate(s['Clip'])if c['channel']==1 and c['frame_count']==frames and c['length']==f32(clip['length'])and s['Key'][c['first_key']:c['first_key']+c['key_count']]==[dict(time=f32(t),frame=int(f))for t,f in clip['keys']]]
                require(len(matches)==1,'Missing/ambiguous compiled emote keys');index=matches[0]
        check_clip(index,clip,frames,8,1)

def validate(ir,ex):
    fields(ir,('schema','kind','commit','sources','scene','npc_script','presentation','presentation_manifest','effect','programs','actors','npc_profiles','audio','melody_bindings','initial_bindings','stop_binding','animation','encounters','resource_catalog'))
    require(type(ir['schema'])is int and ir['schema']==1 and ir['kind']=='encore.world-program-bindings','Unknown world bindings schema')
    require(ir['commit']==ex.lock['commit'],'World binding source pin mismatch')
    require(type(ir['sources'])is dict and 0<len(ir['sources'])<=128,'Missing/excessive world binding sources')
    for path,digest in ir['sources'].items():
        require(safe(path)and type(digest)is str and re.fullmatch('[0-9a-f]{64}',digest),'Invalid world binding source')
        require(hashlib.sha256(ex.source(path).read_bytes()).hexdigest()==digest,'World binding source changed: '+path)
    require(ir['scene']in ir['sources']and ir['npc_script']in ir['sources']and safe(ir['presentation']),'Unreviewed world binding context')
    scene=ex.text(ir['scene']);docs={}
    require(type(ir['programs'])is list and len(ir['programs'])==len(STAGES),'Missing world programs')
    for index,row in enumerate(ir['programs']):
        fields(row,('id','stage','path','selector','labels'))
        require(type(row['id'])is int and row['id']==index+1 and row['stage']==STAGES[index]and safe(row['path']),'Unknown/reordered program identity/stage')
        path='Data/Dialogue/'+row['path']+'.yaml';require(path in ir['sources'],'Unreviewed program source')
        doc=yaml.safe_load(ex.text(path));require(type(doc)is dict and list(doc)==row['labels'],'Source phrase coverage/order mismatch')
        selector=row['selector'];require(type(selector)is dict,'Invalid program source selector')
        if selector.get('kind')=='scene_dialog':
            fields(selector,('kind','node'));require(safe(selector['node']),'Invalid program source node')
            require(property(block(scene,selector['node'])[1],'dialog')==row['path'],'Program source scene dialog mismatch')
        elif selector.get('kind')=='battle_win':
            fields(selector,('kind','program_id','phrase'));require(type(selector['program_id'])is int and selector['program_id']in docs,'Invalid program dependency')
            require(phrase(docs[selector['program_id']],selector['phrase'])['startbattle']['wincutscene']==row['path'],'Program battle continuation mismatch')
        else:raise ValueError('Unsupported program source selector')
        docs[row['id']]=doc
    require(len({row['path']for row in ir['programs']})==len(ir['programs']),'Duplicate source program')
    require(type(ir['actors'])is list and len(ir['actors'])==5,'Missing world actor namespace')
    actor_nodes={};actor_names=set()
    for index,row in enumerate(ir['actors']):
        fields(row,('id','alias','source_alias','node','program_id','resource_role'))
        require(type(row['id'])is int and row['id']==index+1 and type(row['alias'])is str and row['alias']==row['source_alias'].title(),'Unknown/reordered actor identity')
        require(type(row['program_id'])is int and row['program_id']in docs,'Unknown actor program reference')
        actor_map=phrase(docs[row['program_id']],'0')['actors']
        require(row['source_alias']in actor_map and actor_map[row['source_alias']]==row['node'],'Actor/YAML source node mismatch')
        require(row['alias']not in actor_names and row['node']not in actor_nodes,'Duplicate actor name/node')
        if row['node']!='leader':require(safe(row['node']),'Invalid actor node');block(scene,row['node'])
        require(type(row['resource_role'])is str and row['resource_role'],'Missing actor resource role')
        actor_names.add(row['alias']);actor_nodes[row['node']]=row
    for doc in docs.values():
        for p in doc.values():
            for name,path in p.get('actors',{}).items():
                require(path in actor_nodes,'Unsupported actor source reference')
                require(name==actor_nodes[path]['source_alias']or(name=='leader'and path=='leader'),'Unknown source actor alias')
    require(safe(ir['presentation_manifest']),'Unsafe NPC presentation manifest')
    presentation=ex.document(ir['presentation']);manifest=ex.document(ir['presentation_manifest']);profile_ids=set()
    require(type(ir['npc_profiles'])is list and len(ir['npc_profiles'])==3,'Missing NPC profile bindings')
    for row in ir['npc_profiles']:
        fields(row,('actor_id','execution_kind','texture_source','animation_source','receipt','yaml_index','animation_index','phase','idle_animation','clips'))
        identity=row['actor_id'];require(type(identity)is int and 3<=identity<=5 and identity not in profile_ids,'Unknown/duplicate NPC identity')
        profile_ids.add(identity);actor=ir['actors'][identity-1];source_node=block(scene,actor['node'])[1]
        require(row['texture_source']=='Graphics/Character Sprites/'+property(source_node,'sprite')+'.png'and row['texture_source']in ir['sources'],'NPC sprite binding mismatch')
        require(row['animation_source']in ir['sources'],'Unreviewed NPC animation source')
        override=re.findall(r'^yaml = (.*)$',source_node,re.M)
        if override:require(len(override)==1 and json.loads(override[0])=='res://'+row['animation_source'],'NPC animation override mismatch')
        else:
            script=ex.text(ir['npc_script']);default=re.findall(r'elif yaml_path == "":\s*\n\s*yaml_path = "res://([^"]+)"',script)
            require(default==[row['animation_source']],'NPC inherited animation mismatch')
        candidates=[r for r in presentation['resources']if r['role']==actor['resource_role']]
        require(len(candidates)==1,'Unknown/duplicate NPC resource role')
        source_resources=[r for r in manifest['recipe']['resources']if r['role']==actor['resource_role']]
        require(len(source_resources)==1 and source_resources[0]['source']==row['texture_source']and source_resources[0]['output']==candidates[0]['path'],'NPC presentation/source texture mapping mismatch')
        require(safe(row['receipt'])and type(row['yaml_index'])is int and type(row['animation_index'])is int,'Invalid NPC native receipt slot')
        receipt=ex.document(row['receipt']);require(receipt['commit']==ir['commit'],'NPC receipt pin mismatch')
        from tools.doll_dialogue import decode
        native=decode(receipt);data=yaml.safe_load(ex.text(row['animation_source']))
        require(0<=row['yaml_index']<len(native['yaml'])and native['yaml'][row['yaml_index']]==data,'NPC native YAML slot mismatch')
        require(row['animation_index']==row['yaml_index']-1 and 0<=row['animation_index']<len(native['animations']),'NPC native animation slot mismatch')
        require(row['idle_animation']in data['animations'],'Unknown NPC idle source animation')
        kind=2 if len(data['animations'][row['idle_animation']]['directions'])==1 else 3
        require(type(row['execution_kind'])is int and row['execution_kind']==kind and row['phase']==ir['programs'][actor['program_id']-1]['stage'],'Unsupported NPC execution profile/phase')
    require([row['actor_id']for row in ir['npc_profiles']]==sorted(profile_ids),'NPC stable profile order mismatch')
    require(safe(ir['resource_catalog'])and type(ir['encounters'])is list and len(ir['encounters'])==2,'Missing/invalid opening encounter bindings')
    catalog=ex.document(ir['resource_catalog']);require(catalog['commit']==ir['commit']and catalog['kind']=='encore.native-resource-catalog.source-ir','Unreviewed encounter catalog')
    for index,row in enumerate(ir['encounters']):
        fields(row,('id','phase','actor_id','program_id','phrase','source_ir','catalog_id','resource_path'))
        require(type(row['id'])is int and row['id']==index+1 and row['phase']==STAGES[index]and row['program_id']==program(ir,row['phase'])['id'],'Unsupported encounter identity/phase/program')
        require(type(row['actor_id'])is int and 1<=row['actor_id']<=len(ir['actors'])and safe(row['source_ir'])and safe(row['resource_path']),'Invalid encounter actor/source/resource')
        source=ex.document(row['source_ir']);require(source['commit']==ir['commit'],'Encounter source pin mismatch')
        actor_row=ir['actors'][row['actor_id']-1];boundary=phrase(docs[row['program_id']],row['phrase'])['startbattle']
        require(boundary['battlers']==[{actor_row['source_alias']:source['enemy']['id']}],'Encounter battler/source actor mapping mismatch')
        calls=[label for label,p in docs[row['program_id']].items()if 'startbattle'in p];require(calls==[row['phrase']],'Encounter source boundary coverage mismatch')
        if 'binding'in source:require(source['binding']['stable_id']==row['id']and source['binding']['enemy_instance']==row['actor_id']-1,'Encounter external instance identity mismatch')
        candidates=[r for r in catalog['bindings']if r['id']==row['catalog_id']]
        require(type(row['catalog_id'])is int and len(candidates)==1 and candidates[0]['role']in('Battle','EncounterBattle')and candidates[0]['path']==row['resource_path'],'Encounter catalog binding mismatch')
    require(type(ir['audio'])is list and len(ir['audio'])==3,'Missing melody audio namespace')
    audio_paths=set()
    for row in ir['audio']:
        fields(row,('id','path','selector'));require(type(row['id'])is int and row['id']>=1 and safe(row['path'])and row['path']in ir['sources'],'Invalid audio identity/source')
        require(row['path']not in audio_paths,'Duplicate audio source');audio_paths.add(row['path'])
        selector=row['selector'];require(type(selector)is dict,'Invalid audio source selector')
        if selector.get('kind')=='scene_loop':
            fields(selector,('kind','node'));expected='Audio/Music/'+property(block(scene,selector['node'])[1],'loop')
        elif selector.get('kind')in('music','soundeffect'):
            fields(selector,('kind','program_id','phrase'));require(type(selector['program_id'])is int and selector['program_id']in docs,'Unknown audio program')
            expected=('Audio/Music/'if selector['kind']=='music'else'Audio/Sound effects/')+phrase(docs[selector['program_id']],selector['phrase'])[selector['kind']]
        else:raise ValueError('Unknown audio source selector')
        require(row['path']==expected,'Audio source field mismatch')
    require([r['id']for r in ir['audio']]==list(range(ir['audio'][0]['id'],ir['audio'][0]['id']+len(ir['audio']))),'Reordered/duplicate audio identity')
    effect=ir['effect'];fields(effect,('id','path','source_ir','scene'))
    require(type(effect['id'])is int and effect['id']+1==ir['audio'][0]['id']and safe(effect['path'])and effect['path'].endswith('.encfx')and safe(effect['source_ir'])and effect['scene']in ir['sources'],'Invalid effect identity/source/resource')
    source_effect=ex.document(effect['source_ir'])
    require(source_effect['kind']=='encore.world-effect.source-ir'and source_effect['commit']==ir['commit']and effect['scene']in source_effect['sources'],'Unreviewed world effect resource source')
    all_bindings=ir['initial_bindings']+[ir['stop_binding']]+ir['melody_bindings']
    require(type(ir['initial_bindings'])is list and len(ir['initial_bindings'])==2 and type(ir['melody_bindings'])is list and len(ir['melody_bindings'])==4,'Missing object binding namespace')
    identities=set();calls=set()
    for index,row in enumerate(all_bindings):
        fields(row,('id','name','kind','node','method','program_id','phrase','target','method_source','target_binding_id'))
        require(type(row['id'])is int and row['id']==index+1 and row['name']not in identities,'Unknown/reordered/duplicate object binding identity')
        identities.add(row['name']);require(type(row['kind'])is int and METHOD_KINDS.get(row['method'])==row['kind'],'Unknown/mismatched object method mechanism')
        require(type(row['program_id'])is int and row['program_id']in docs,'Unknown object program reference')
        require(phrase(docs[row['program_id']],row['phrase'])['objectsfunction'].get(row['node'])==row['method'],'Object source call mismatch')
        require(row['method_source']in ir['sources']and re.search(r'^func '+re.escape(row['method'])+r'\(',ex.text(row['method_source']),re.M),'Missing original object method')
        attrs,_=block(scene,row['node']);instance=re.search(r'instance=ExtResource\( (\d+) \)',attrs)
        require(instance is not None,'Unreviewed object source inheritance')
        external={int(i):p for p,i in re.findall(r'^\[ext_resource path="res://([^"]+)"[^\n]* id=(\d+)\]',scene,re.M)}
        parent_source=external[int(instance[1])];require(parent_source in ir['sources'],'Unreviewed object parent scene');parent=ex.text(parent_source)
        if row['method_source']==parent_source:
            require(re.search(r'^script = SubResource\( \d+ \)',block(parent,'.')[1],re.M)and '[sub_resource type="GDScript"'in parent,'Object embedded method script mismatch')
        else:
            script_refs={int(i):p for p,i in re.findall(r'^\[ext_resource path="res://([^"]+)"[^\n]* id=(\d+)\]',parent,re.M)}
            root_script=re.findall(r'^script = ExtResource\( (\d+) \)',block(parent,'.')[1],re.M)
            require(len(root_script)==1 and script_refs[int(root_script[0])]==row['method_source'],'Object method script source mismatch')
        if row['kind']in(6,7):require(parent_source==effect['scene'],'Object/world-effect source scene mismatch')
        identity=(row['program_id'],row['phrase'],row['node'],row['method'])
        require(identity not in calls,'Duplicate source object call');calls.add(identity)
        expected_target='music'if index==0 else'shake_sound'if row['kind']==3 else'binding'if row['kind']==4 else'effect'if row['kind']in(6,7)else'house_music'
        require(row['target']==expected_target,'Unknown/mismatched object target role')
        if row['kind']==4:
            targets=[b for b in all_bindings if b['id']==row['target_binding_id']]
            require(type(row['target_binding_id'])is int and len(targets)==1 and targets[0]['kind']==3 and targets[0]['node']==row['node'],'Stop-shaker target source binding mismatch')
        else:require(row['target_binding_id']is None,'Unexpected object binding reference')
    expected={(identity,label,path,method)for identity,doc in docs.items()for label,p in doc.items()for path,method in p.get('objectsfunction',{}).items()}
    require(calls==expected,'Missing/unknown object method coverage')
    validate_animation(ir,ex,docs)
    return ir

def load(ex):return validate(load_json(ex.file(IR)),ex)

def actor_indices(ir):return {'None':65535,**{row['alias']:row['id']-1 for row in ir['actors']}}

def program(ir,stage):
    values=[row for row in ir['programs']if row['stage']==stage];require(len(values)==1,'Unknown world program stage');return values[0]

def actor(ir,identity):
    values=[row for row in ir['actors']if row['id']==identity];require(len(values)==1,'Unknown world actor ID');return values[0]

def encounter(ir,stage):
    values=[row for row in ir['encounters']if row['phase']==stage];require(len(values)==1,'Unknown encounter stage');return values[0]

def object_indices(ir,stage):
    rows=ir['initial_bindings']if stage=='initial'else[ir['stop_binding']]if stage=='attack'else ir['melody_bindings']if stage=='melody'else[]
    return {(row['node'],row['method']):row['id']-1 for row in rows}

def verify_room(ir,room,ex):
    """Keep ordinary checked compilation tied to the external stable bindings."""
    strings,sections=room['strings'],room['sections']
    verify_animation(ir,room,ex)
    for row in ir['encounters']:
        value=sections['Battle'][row['id']-1];resource=sections['Resource'][value['battle_resource_index']]
        require(value['stable_id']==row['id']and value['actor_instance_index']==row['actor_id']-1 and strings[resource['path_string']]==row['resource_path'],'Compiled encounter actor/resource mismatch')
    require(room['upstream_commit']==ir['commit'],'Compiled room binding source pin mismatch')
    for row,compiled in zip(ir['programs'],sections['Program']):
        require(compiled['stable_id']==row['id']and strings[compiled['source_path_string']]==row['path'],'Compiled program namespace mismatch')
    require(len(sections['Program'])>=len(ir['programs']),'Missing compiled source program')
    for row in ir['actors']:
        require(row['id']<=len(sections['ActorInstance']),'Missing compiled source actor')
        instance=sections['ActorInstance'][row['id']-1]
        require(instance['stable_id']==row['id']and instance['profile_index']==row['id']-1,'Compiled actor namespace mismatch')
    presentation=ex.document(ir['presentation'])
    for row in ir['npc_profiles']:
        identity=row['actor_id'];instance=sections['ActorInstance'][identity-1];profile=sections['ActorProfile'][instance['profile_index']]
        resource=sections['Resource'][profile['primary_resource']];role=ir['actors'][identity-1]['resource_role']
        source=[r for r in presentation['resources']if r['role']==role]
        require(len(source)==1 and profile['execution_kind']==row['execution_kind']and strings[resource['path_string']]==source[0]['path'],'Compiled NPC role/profile mismatch')
    for row in ir['audio']:
        require(row['id']<=len(sections['Resource']),'Missing compiled audio binding')
        resource=sections['Resource'][row['id']-1]
        require(resource['stable_id']==row['id']and resource['kind']==2 and strings[resource['path_string']]=='res://'+row['path'],'Compiled audio stable/source binding mismatch')
    effect=ir['effect'];resource=sections['Resource'][effect['id']-1]
    require(resource['stable_id']==effect['id']and resource['kind']==4 and strings[resource['path_string']]==effect['path'],'Compiled world-effect stable/source binding mismatch')
    for row in ir['initial_bindings']+[ir['stop_binding']]+ir['melody_bindings']:
        require(row['id']<=len(sections['Binding']),'Missing compiled object binding')
        value=sections['Binding'][row['id']-1]
        require(value['stable_id']==row['id']and value['kind']==row['kind'],'Compiled object stable/method binding mismatch')
        if row['kind']==4:require(value['target_index']==row['target_binding_id']-1,'Compiled stop-shaker target binding mismatch')
        else:
            target=sections['Resource'][value['target_index']]
            if row['kind']in(1,5):expected='res://Audio/Music/'+property(block(ex.text(ir['scene']),row['node'])[1],'loop')
            elif row['kind']==3:
                sound=re.findall(r'^export \(String\) var sound = ("[^"\n]+")',ex.text(row['method_source']),re.M)
                require(len(sound)==1,'Missing checked shaker sound source');expected='res://Audio/Sound effects/'+json.loads(sound[0])
            else:expected=effect['path']
            require(strings[target['path_string']]==expected,'Compiled object target source mismatch')
