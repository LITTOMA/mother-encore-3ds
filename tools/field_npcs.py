#!/usr/bin/env python3
"""Checked source NPC motion owners for the admitted Podunk story slice.

No dialogue is copied from Mick. The common npc.tscn/4dir geometry and motion
executor are reused after verifying effective source instances. Programmes
remain independently compiled by the field story adapter.
"""
from __future__ import annotations
import argparse,copy,hashlib,json,math,re,shlex,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import mick_treats as m
from tools.extract_battle_entry import Extractor,require
from tools.godot_text import Document,instantiate,Vec2
POLICY=ROOT/'content/podunk-npcs.json'

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,value):
    p.parent.mkdir(parents=True,exist_ok=True)
    p.write_bytes((json.dumps(value,indent=2,ensure_ascii=False)+'\n').encode('utf-8'))
def load_policy(path=POLICY):
    def unique(pairs):
        result={}
        for key,value in pairs:
            require(key not in result,'Duplicate NPC policy field '+key);result[key]=value
        return result
    p=json.loads(Path(path).read_text(encoding='utf-8'),object_pairs_hook=unique)
    require(set(p)==({'schema','kind','commit','scene','npc_scene','npc_script','animation','receipt','actors'}|({'texture_aliases'}if p.get('schema')==3 else set())) and type(p['schema'])is int and p['schema']in(1,2,3) and p['kind']=='encore.field-npc.recipe' and p['commit']==m.PIN,'Unknown field NPC policy')
    require(isinstance(p['actors'],list) and 1<=len(p['actors'])<=16,'Field NPC count')
    actor_fields={'id','alias','path','texture','motion'}|({'interaction_policy'}if p['schema']>=2 else set())
    require(all(type(a)is dict and set(a)==actor_fields for a in p['actors']),'Unknown NPC actor fields')
    if p['schema']>=2:
        require(all(type(a['interaction_policy'])is bool for a in p['actors'])and any(a['interaction_policy']for a in p['actors']),'Field NPC schema2 requires an explicit interaction policy')
    for key in ('id','alias','path','motion'):
        require(len({a[key]for a in p['actors']})==len(p['actors']),'Duplicate NPC '+key)
    aliases=p.get('texture_aliases',[])
    require(type(aliases)is list and (bool(aliases)==(p['schema']==3)),'NPC texture alias schema coverage')
    declared={}
    for alias in aliases:
        require(type(alias)is dict and set(alias)=={'owner','actor'} and all(type(alias[k])is int for k in alias),'NPC texture alias fields')
        owner=next((a for a in p['actors']if a['id']==alias['owner']),None)
        actor=next((a for a in p['actors']if a['id']==alias['actor']),None)
        require(owner is not None and actor is not None and owner['id']<actor['id'] and actor['id']not in declared and owner['id']not in declared and owner['texture']==actor['texture'],'Invalid NPC texture alias owner')
        declared[actor['id']]=owner['id']
    seen={}
    for a in p['actors']:
        previous=seen.get(a['texture'])
        require((previous is None and a['id']not in declared)or(previous is not None and declared.get(a['id'])==previous),'Undeclared or unused NPC texture alias')
        if previous is None:seen[a['texture']]=a['id']
    for a in p['actors']:
        require(type(a['id'])is int and 2<=a['id']<=256,'Field NPC stable identity')
        require(all(type(a[k])is str and a[k]for k in ('alias','path','texture','motion')),'Field NPC binding')
        require(a['texture'].startswith('graphics/world/') and a['texture'].endswith('.t3x') and a['motion'].startswith('world/') and a['motion'].endswith('.encmick') and '..'not in a['texture'] and '..'not in a['motion'],'NPC output path')
    return p

def validate_sprite_overrides(actual,template,image,animation,texture):
    """Only source-ready-overwritten editor values may differ from npc.tscn."""
    allowed={'texture','offset','visible','yaml','sprite'}
    require(all(actual.get(k)==template.get(k)for k in (set(actual)|set(template))-allowed),'Unreviewed NPC sprite geometry/property override')
    require(actual.get('_auto_offset',True)is True and type(actual.get('visible'))is bool,'NPC automatic offset/visibility policy')
    require(actual.get('sprite')=='res://'+image and actual.get('yaml')=='res://'+animation and texture==image,'NPC editor sprite does not match its actual source binding')
    offset=actual.get('offset');require(isinstance(offset,Vec2)and offset.x==0 and math.isfinite(offset.y),'NPC editor offset outside overwritten source axes')

def validate_sprite_ready_source(script,sprite_script):
    def method(code,name):
        found=re.search(r'^func '+name+r'[(].*?(?=^func |\Z)',code,re.M|re.S);require(found is not None,'Missing NPC sprite source method '+name);return found[0]
    update=method(script,'_update_sprite_and_animations')
    ordered=['character_sprite.set_sprite(', 'character_sprite.set_animation(', 'character_sprite.set_spritesheet()', 'character_sprite.set_sprite_offset(']
    offsets=[update.index(value)if value in update else -1 for value in ordered]
    require(all(x>=0 for x in offsets)and offsets==sorted(offsets),'NPC runtime sprite setup order changed')
    require('_update_sprite_and_animations()'in method(script,'_ready'),'NPC runtime sprite setup is not called at ready')
    sheet=method(sprite_script,'set_spritesheet')
    require('texture = load(sprite)'in sheet and 'if _auto_offset:'in sheet and 'offset.y = -int(texture.get_height()/float(vframes*2))'in sheet and '_default_offset = offset'in sheet and 'show()'in sheet,'NPC runtime sheet offset/visibility policy changed')
    require('offset = _default_offset + sprite_offset'in method(sprite_script,'set_sprite_offset'),'NPC final sprite offset policy changed')

def build():
    policy=load_policy();ex=Extractor(ROOT)
    base=m.build(ROOT)
    documents={}
    def doc(path):
        if path not in documents:documents[path]=Document(path,ex.text(path))
        return documents[path]
    nodes={n.path:n for n in instantiate(doc,policy['scene'])}
    script=ex.text(policy['npc_script']);anim=ex.yaml(policy['animation'])
    def default(name,kind):
        value=re.search(r'^export(?:\s*\([^\n]*\))?\s+var '+re.escape(name)+r'\s*(?::=|=)\s*([^\n#]+)',script,re.M)
        require(value is not None,'Missing NPC default '+name)
        raw=value[1].strip()
        if kind is bool:
            require(raw in ('true','false'),'Unsupported NPC bool default');return raw=='true'
        if kind is str:return json.loads(raw)
        return float(raw)
    results=[]
    for binding in policy['actors']:
        interaction_policy=binding.get('interaction_policy',False)
        path=binding['path'];require(path in nodes,'Missing source NPC '+path);node=nodes[path]
        require(node.scene==policy['npc_scene'] and node.script==policy['npc_script'],'Unreviewed NPC class/script')
        require(node.props.get('no_collision',False)is False and node.props.get('no_shadow',False)is False,'Unreviewed NPC collision/shadow mode')
        require(node.props.get('yaml','')in ('','res://'+policy['animation']) and node.props.get('sprite_offset',Vec2(0,0))==Vec2(0,0),'Unreviewed NPC animation/offset override')
        require(not node.props.get('event_positions') and not node.props.get('_automatic_shadow',False) and not node.props.get('debug_npc',False) and node.props.get('extended_interact',0)==0,'Unreviewed NPC lifecycle/geometry override')
        require(all(node.props.get(k)==nodes[m.ACTOR_PATH].props.get(k)for k in ('collision_layer','collision_mask')),'Unreviewed NPC collision filter')
        # All geometry must be the shared npc.tscn geometry, including instance
        # overrides. New source variants require an explicit adapter review.
        children=('CollisionShape2D','interact/CollisionShape2D','NearPlayerArea/CollisionShape2D','ViewArea/CollisionShape2D2','WanderRadius/CollisionShape2D2','RayCast2D')+(()if interaction_policy else('CharacterSprite',))
        for child in children:
            require(nodes[path+'/'+child].props==nodes[m.ACTOR_PATH+'/'+child].props,'Unreviewed NPC child override '+path+'/'+child)
        def origin(child):
            at=child;value=Vec2(0,0)
            while at is not None:
                n=nodes[at]
                require(n.props.get('rotation',0)==0 and n.props.get('scale',Vec2(1,1))==Vec2(1,1),'Unreviewed NPC transform '+at)
                local=n.props.get('position',Vec2(0,0));require(isinstance(local,Vec2),'NPC position type')
                value=Vec2(value.x+local.x,value.y+local.y)
                at=m.parent_path(at)
            return [m.f32(value.x),m.f32(value.y)]
        position=origin(path);initial=node.props.get('initial_dir')
        require(isinstance(initial,Vec2) and (initial.x or initial.y),'Missing NPC initial direction')
        source='Graphics/Character Sprites/'+node.props['sprite']+'.png';size=ex.png_size(source);ex.data(source+'.import')
        if interaction_policy:
            require('Npcs'in node.props['sprite']or'Enemies'in node.props['sprite'],'Unreviewed field NPC directional class')
            sprite=nodes[path+'/CharacterSprite'];owner=sprite.overrides.get('texture',sprite.resolve)
            require(owner is not None and sprite.script=='Scripts/Main/character_sprite.gd' and sprite.scene==nodes[m.ACTOR_PATH+'/CharacterSprite'].scene,'NPC editor texture/script owner is not reviewed')
            validate_sprite_overrides(sprite.props,nodes[m.ACTOR_PATH+'/CharacterSprite'].props,source,policy['animation'],owner.ext_path(sprite.props['texture'],'Texture'))
            validate_sprite_ready_source(script,ex.text('Scripts/Main/character_sprite.gd'))
        columns,rows=anim['size'];require(not(size[0]%columns or size[1]%rows),'NPC texture grid')
        def effective(name,kind):return node.props[name]if name in node.props else default(name,kind)
        idle=effective('idle_animation',str)
        require(idle in ('Idle','Walk'),'NPC idle animation is not admitted')
        wander=effective('wander',bool);staring=effective('staring',bool)
        require(type(wander)is bool and type(staring)is bool,'NPC motion flags')
        if interaction_policy:
            from tools.field_scene_npcs import validate_sprite_policy
            require(not wander,'NPC interaction policy is limited to stationary source actors')
            validate_sprite_policy(idle,effective('talk_idle_animation',str),node.props.get('connections',[["Talk","Idle",2]]),bool(node.props.get('dialog','')))
        ir=copy.deepcopy(base);actor=ir['actor'];before=actor['position'];delta=[position[i]-before[i]for i in range(2)]
        for name in ('sprite_position','interact_center','collision_center'):
            actor[name]=[m.f32(actor[name][i]+delta[i])for i in range(2)]
        character=origin(path+'/CharacterSprite');offset=anim.get('offset',[0,0])
        actor['sprite_position']=[m.f32(character[0]+offset[0]),m.f32(character[1]-int(size[1]/(rows*2))+offset[1])]
        actor.update(source_path=path,position=position,sort_y=position[1],initial_direction=[initial.x,initial.y],speed=effective('speed',float),walk_frequency=effective('walk_frequency',float),item=None,require_flag=None,consume_flag=None,bark_path=None,bark_programme=None,bark_center=[0,0],bark_extents=[0,0])
        clips=[]
        for role,original in ((0,1 if idle=='Walk'else 0),(1,1),(2,2)):
            first=len(clips)
            clips.extend(dict(c,anim=role)for c in base['clips']if c['anim']==original)
            name=('idle','walk','talk')[role];actor[name+'_clip']=first;actor[name+'_clip_count']=len(clips)-first
        require(all(c['frame']<columns*rows for c in clips),'NPC clip frame bound')
        ir.update(texts=[],commands=[],programmes=[],clips=clips)
        direction=(1 if initial.x<0 else 2)if abs(initial.x)>abs(initial.y)else(3 if initial.y<0 else 0)
        actor['frame']=next(c['frame']for c in clips if c['anim']==0 and c['direction']==direction)
        ir['texture']=dict(source=source,size=size,columns=columns,rows=rows,output=binding['texture'],frame=actor['frame'])
        events=copy.deepcopy(base['rng'])
        for event in events:
            if event['kind']=='ArmWander':event['kind']='RandRange'
        if wander:
            rect=m.notifier_rect(nodes,path)
            candidates=[event for event in events if event['kind']=='RandRange' and all(event[k]==rect[k]for k in ('x','y','w','h')) and event['a']==0.1 and event['b']==actor['walk_frequency']]
            require(len(candidates)==1,'Ambiguous NPC source screen-entered wander draw')
            candidates[0]['kind']='ArmWander'
        ir['rng']=events
        if interaction_policy:ir['rng']=[]
        flags=int(wander)|int(staring)<<1|int(idle=='Walk')<<2
        blob=m.lower(ir,motion_owner=binding['id'],motion_flags=flags);m.parse(blob,allow_motion=True)
        if interaction_policy:
            from tools.field_scene_npcs import interaction_blob
            blob=interaction_blob(dict(binding=binding,ir=ir,blob=blob,directions=4,dialogue=node.props.get('dialog',''),alternatives=node.props.get('_all_dialog',[]),has_initial_dialogue=bool(node.props.get('dialog','')),return_after_interaction_seconds=0 if staring else 1))
        results.append(dict(binding=binding,ir=ir,blob=blob,features=flags,appear=node.props.get('appear_flag',''),disappear=node.props.get('disappear_flag','')))
    validate_texture_aliases(policy,results)
    sources=dict(base['sources']);sources.update(ex.sources)
    return policy,results,dict(sorted(sources.items()))

def validate_texture_aliases(policy,results):
    by_id={r['binding']['id']:r for r in results}
    for alias in policy.get('texture_aliases',[]):
        a=by_id[alias['owner']]['ir']['texture'];b=by_id[alias['actor']]['ir']['texture']
        require(all(a[k]==b[k]for k in ('source','size','columns','rows','output')),'NPC texture aliases differ in actual source image/grid')


def verify_receipt(policy,results,sources):
    receipt=json.loads((ROOT/policy['receipt']).read_text(encoding='utf-8'))
    require(receipt['commit']==policy['commit'] and receipt['policy_sha256']==sha(POLICY) and receipt['producer_sha256']==sha(__file__) and receipt['sources']==sources,'Stale NPC asset receipt')
    if policy['schema']>=2:require(receipt.get('interaction_producer_sha256')==sha(ROOT/'tools/field_scene_npcs.py'),'Stale NPC interaction policy producer')
    require(set(receipt['textures'])=={r['binding']['texture']for r in results},'NPC texture receipt set')
    for path,item in receipt['textures'].items():
        p=ROOT/'romfs'/path;require(p.is_file() and p.stat().st_size==item['bytes'] and sha(p)==item['sha256'],'NPC texture bytes differ from receipt')
    return receipt

def add_room(sections,strings,S,house,resource):
    """Append original 4dir replacement profiles to the field Room namespace.

    The source NPC scene/geometry was checked by build(); the reviewed House
    4dir profile supplies the same Actor executor and shared shadow/emote art.
    Every directional clip is lowered from the actual NPC motion keys.
    """
    policy,records,sources=build();verify_receipt(policy,records,sources)
    hs=house['sections'];animation=Extractor(ROOT).yaml(policy['animation'])
    template=next(p for p in hs['ActorProfile']if p['execution_kind']==3 and p['direction_count']==4)
    old=hs['Resource'][template['primary_resource']]
    bubble_extra=-template['emote_offset'][1]-old['height']/old['rows']
    def identity(table):return max((r['stable_id']for r in sections[table]),default=0)+1
    texture_resources={}
    for record in records:
        binding=record['binding'];ir=record['ir'];a=ir['actor'];t=ir['texture'];index=len(sections['ActorProfile'])
        require(index==len(sections['ActorInstance']) and index+1==binding['id'],'NPC stable append order changed')
        primary=texture_resources.get(t['output'])
        if primary is None:
            primary=len(sections['Resource']);sections['Resource'].append(dict(stable_id=identity('Resource'),path_string=S(t['output']),width=t['size'][0],height=t['size'][1],columns=t['columns'],rows=t['rows'],kind=1,flags=0,sha256=sha(ROOT/'romfs'/t['output'])))
            texture_resources[t['output']]=primary
        first=len(sections['AnimationBinding']);directions=len(sections['DirectionFrame']);idle_clips=[]
        for role,motion in ((0,0),(1,4),(2,5)):
            for direction in range(4):
                source_role='Walk'if role==0 and record['features']&4 else('Idle','Walk','Talk')[role]
                definition=animation['animations'][source_role]['directions'][direction]
                time=m.f32(definition[0]);keys=[]
                for frame,duration in definition[1:]:
                    keys.append(dict(time=time,frame=int(frame)-1));time=m.f32(time+duration)
                require(keys and time>0,'Missing source NPC animation')
                clip=len(sections['Clip']);key=len(sections['Key'])
                sections['Key'].extend(keys)
                sections['Clip'].append(dict(stable_id=identity('Clip'),length=time,first_key=key,key_count=len(keys),flags=1|(16 if keys[0]['time'] else 0),visibility=0,frame_count=t['columns']*t['rows'],channel=0))
                sections['AnimationBinding'].append(dict(actor_profile_index=index,motion_state=motion,direction=direction,clip_index=clip))
                if role==0:idle_clips.append(clip);sections['DirectionFrame'].append(dict(frame=keys[0]['frame']))
        profile=dict(template,stable_id=binding['id'],primary_resource=primary,shadow_resource=resource(template['shadow_resource']),emote_resource=resource(template['emote_resource']),animation_binding_first=first,animation_binding_count=len(sections['AnimationBinding'])-first,direction_first=directions,direction_count=4,idle_clip=idle_clips[0],emote_clip=0xffffffff,initial_frame=a['frame'],sprite_position=list(template['sprite_position']),sprite_offset=[0,-int(t['size'][1]/(t['rows']*2))],emote_offset=[0,m.f32(-(t['size'][1]/t['rows']+bubble_extra))])
        sections['ActorProfile'].append(profile)
        direction=a['initial_direction'];d=(1 if direction[0]<0 else 2)if abs(direction[0])>abs(direction[1])else(3 if direction[1]<0 else 0)
        sections['ActorInstance'].append(dict(stable_id=binding['id'],profile_index=index,binding_kind=2,flags=1,display_name_string=S(binding['alias']),position=a['position'],direction=direction,initial_clip=idle_clips[d]))
    return {**{'upstream/MOTHER-Encore/'+p:h for p,h in sources.items()},'content/podunk-npcs.json':sha(POLICY),'tools/field_npcs.py':sha(__file__)}

def stage_files(source_root=ROOT/'romfs'):
    policy,records,sources=build();verify_receipt(policy,records,sources);files={}
    for record in records:
        motion=Path(record['binding']['motion']);texture=Path(record['binding']['texture'])
        require((source_root/motion).read_bytes()==record['blob'],'Stale field NPC motion')
        files[motion]=record['blob'];files[texture]=(source_root/texture).read_bytes()
    return files

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=('assets','compile','verify'));p.add_argument('--tex3ds',type=Path);p.add_argument('--tex3ds-command');a=p.parse_args()
    policy,results,sources=build()
    if a.action=='assets':
        require(a.tex3ds and a.tex3ds.is_file(),'Real tex3ds required for NPC assets');textures={}
        command=shlex.split(a.tex3ds_command)if a.tex3ds_command else None
        for record in results:
            ir=record['ir']
            if ir['texture']['output']in textures:continue
            target=ROOT/'romfs'/ir['texture']['output'];target.parent.mkdir(parents=True,exist_ok=True);source=ROOT/'upstream/MOTHER-Encore'/ir['texture']['source']
            rel=lambda v:v.resolve().relative_to(ROOT).as_posix()
            args=(command+['-f','rgba8','-z','none','-o',rel(target),rel(source)])if command else[str(a.tex3ds),'-f','rgba8','-z','none','-o',str(target),str(source)]
            subprocess.run(args,cwd=ROOT,check=True)
            textures[ir['texture']['output']]=dict(bytes=target.stat().st_size,sha256=sha(target))
        receipt=dict(schema=1,commit=policy['commit'],policy_sha256=sha(POLICY),producer_sha256=sha(__file__),tex3ds_sha256=sha(a.tex3ds),sources=sources,textures=textures,scope='Original source NPC images for this game-related port; no fonts or unrelated binaries; motion owners have no dialogue programmes.')
        if policy['schema']>=2:receipt['interaction_producer_sha256']=sha(ROOT/'tools/field_scene_npcs.py')
        write(ROOT/policy['receipt'],receipt)
    else:verify_receipt(policy,results,sources)
    for record in results:
        target=ROOT/'romfs'/record['binding']['motion']
        if a.action=='verify':require(target.read_bytes()==record['blob'],'Stale NPC motion resource')
        else:target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(record['blob'])
    print('Field NPC motion:',len(results),'checked owners; legacy motion and explicitly admitted interaction policies')
    return 0

if __name__=='__main__':
    try:raise SystemExit(main())
    except (ValueError,KeyError,TypeError,OSError,subprocess.SubprocessError)as e:print('FIELD NPC ERROR:',e,file=sys.stderr);raise SystemExit(1)
