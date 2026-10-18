"""Source NPC candidates for the next field scene; never stages a release.

Resolve instance geometry and runtime sprite offsets before encoding motion.
Dialogue, return timers and replacement Actor profiles are separate pending
consumers; these candidates alone cannot enable an original scene.
"""
import argparse,hashlib,json,math,re,shlex,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools.godot_text import Document,Vec2,instantiate
from tools import mick_treats as m,field_npc_motion


def policy(path):
    def unique(pairs):
        out={}
        for k,v in pairs:require(k not in out,'Duplicate NPC candidate field');out[k]=v
        return out
    p=json.loads(Path(path).read_text(encoding='utf8'),object_pairs_hook=unique)
    require(set(p)==set('schema kind commit scene npc_scene npc_script field_recipe actors'.split())and type(p['schema'])is int and p['schema']==2 and p['kind']=='encore.field-npc.candidate-recipe'and p['commit']==PIN,'NPC candidate schema/pin')
    require(isinstance(p['actors'],list)and 1<=len(p['actors'])<=16,'NPC candidate count')
    require(all(isinstance(a,dict)and set(a)==set('id alias path texture motion'.split())for a in p['actors']),'NPC candidate actor fields')
    for k in ('id','alias','path','motion'):require(len({a[k]for a in p['actors']})==len(p['actors']),'Duplicate NPC candidate identity')
    for k in ('scene','npc_scene','npc_script','field_recipe'):
        v=p[k];require(isinstance(v,str)and v and not any(c in v for c in ('\\',':'))and not v.startswith('/')and all(s not in ('','.','..')for s in v.split('/')),'NPC candidate input path')
    for a in p['actors']:
        require(type(a['id'])is int and 2<=a['id']<=256,'NPC candidate actor ID')
        for k in ('alias','path','texture','motion'):
            v=a[k];require(isinstance(v,str)and v and not any(c in v for c in ('\\',':'))and not v.startswith('/')and all(s not in ('','.','..')for s in v.split('/')),'NPC candidate path')
        require(a['texture'].startswith('graphics/world/')and a['texture'].endswith('.t3x')and a['motion'].startswith('world/')and a['motion'].endswith('.encmick'),'NPC candidate output type')
    return p


def validate_sprite_policy(idle,talk_idle,connections,has_dialogue):
    require(type(idle)is str and idle in ('Idle','Walk')and type(talk_idle)is str and talk_idle==idle,'NPC candidate unreviewed talk-idle policy')
    require(connections==[["Talk","Idle",2]]and type(connections[0][2])is int,'NPC candidate unreviewed transition policy')
    require(type(has_dialogue)is bool and (idle=='Idle'or not has_dialogue),'Animated idle dialogue requires separate source policy review')


def build(path=ROOT/'content/mayor-npcs.json'):
    p=policy(path);ex=Extractor(ROOT);docs={}
    from tools.podunk_field import load_recipe
    field=load_recipe(ROOT/p['field_recipe'])
    require(field['scene']['source']==p['scene']and {(a['id'],a['path'],a['scene'])for a in field['actor_bindings']}=={(a['id'],a['path'],p['npc_scene'])for a in p['actors']},'NPC candidate map actor bindings differ')
    def doc(path):
        if path not in docs:docs[path]=Document(path,ex.text(path))
        return docs[path]
    nodes={n.path:n for n in instantiate(doc,p['scene'])};script=ex.text(p['npc_script']);sprite_script=ex.text('Scripts/Main/character_sprite.gd')
    funcs={s[1]:s[0]for s in re.finditer(r'^func ([A-Za-z_][A-Za-z0-9_]*)[(].*?(?=^func |\Z)',script,re.M|re.S)}
    ready=funcs['_ready'];visibility=funcs['update_visibility_changed']
    require(ready.index('if no_collision:')<ready.rindex('update_visibility_changed()')and ready.rstrip().endswith('update_visibility_changed()')and 'collisions.disabled = !is_visible_in_tree()'in visibility,'NPC final collision callback order changed')
    require('interact_area.disabled = !is_visible_in_tree() or !has_dialog()'in visibility,'NPC dialogue interaction gate changed')
    require('get_tree().create_timer(1).connect("timeout", self, "return_to_init_dir")'in funcs['stop_interaction']and 'if !staring:'in funcs['stop_interaction'],'NPC stop return timer changed')
    require('yaml_path = "res://Data/Animations/PartyMember.yaml"'in funcs['_update_sprite_and_animations']and 'if !"Npcs" in sprite and !"Enemies" in sprite:'in funcs['_update_sprite_and_animations'],'NPC PartyMember dispatch changed')
    require('character_sprite.set_sprite_offset(sprite_offset)'in funcs['_update_sprite_and_animations']and 'offset = _default_offset + sprite_offset'in sprite_script and 'AnimationNodeBlendSpace2D.BLEND_MODE_DISCRETE'in sprite_script,'NPC sprite offset/direction source changed')
    def default(name,kind):
        found=re.search(r'^export(?:\s*[(][^\n]*[)])?\s+var '+name+r'\s*(?::=|=)\s*([^\n#]+)',script,re.M)
        if not found:
            require(kind in (bool,str)and re.search(r'^export[^\n]*\bvar '+name+r'(?:\s*:\s*String)?\s*(?:#.*)?$',script,re.M),'Unknown NPC default '+name)
            return False if kind is bool else ''
        raw=found[1].strip()
        if kind is bool:require(raw in ('true','false'),'NPC bool default');return raw=='true'
        if kind is str:return json.loads(raw)
        return float(raw)
    def at(path):
        value,scale=m.world(nodes,path);require(scale==Vec2(1,1),'NPC scaled geometry is unreviewed');return [m.f32(value.x),m.f32(value.y)]
    def shape(path,kind):
        n=nodes[path];owner=n.overrides.get('shape',n.resolve);require(owner is not None,'NPC shape owner');return owner.sub_resource(n.props['shape'],kind)
    records=[]
    for binding in p['actors']:
        path=binding['path'];require(path in nodes,'Missing candidate NPC');n=nodes[path]
        require(n.scene==p['npc_scene']and n.script==p['npc_script'],'Unreviewed candidate NPC class')
        def effective(name,kind):return n.props[name]if name in n.props else default(name,kind)
        require(not effective('wander',bool)and not n.props.get('event_positions')and not effective('_automatic_shadow',bool)and not effective('debug_npc',bool)and effective('extended_interact',float)==0,'NPC candidate lifecycle outside reviewed stationary scope')
        position=at(path);sprite=n.props['sprite'];animation='Data/Animations/'+('PartyMember.yaml'if 'Npcs'not in sprite and 'Enemies'not in sprite else '4dir.yaml')
        require(n.props.get('yaml','')in ('','res://'+animation),'NPC candidate animation override')
        anim=ex.yaml(animation);columns,rows=anim['size'];directions=len(anim['animations']['Idle']['directions']);require(directions in (4,8),'NPC candidate direction count')
        image='Graphics/Character Sprites/'+sprite+'.png';size=ex.png_size(image);ex.data(image+'.import')
        require(size[0]%columns==size[1]%rows==0,'NPC candidate sheet dimensions')
        sprite_at=at(path+'/CharacterSprite');offset=n.props.get('sprite_offset',Vec2(0,0));require(isinstance(offset,Vec2),'NPC sprite offset type')
        idle=effective('idle_animation',str);require(idle in ('Idle','Walk'),'NPC candidate idle state')
        validate_sprite_policy(idle,effective('talk_idle_animation',str),n.props.get('connections',[["Talk","Idle",2]]),bool(n.props.get('dialog','')))
        clips=[];spans={}
        for role,name in enumerate((idle,'Walk','Talk')):
            source=anim['animations'][name];require(source['type']==0 and len(source['directions'])==directions,'NPC candidate directional animation')
            first=len(clips)
            for direction,keys in enumerate(source['directions']):
                require(isinstance(keys,list)and len(keys)>=2 and math.isfinite(keys[0])and keys[0]>=0,'NPC candidate initial animation key')
                for frame,duration in keys[1:]:
                    require(type(frame)is int and 1<=frame<=columns*rows and math.isfinite(duration)and 0<duration<=10 and duration*1000==round(duration*1000),'NPC candidate key/frame duration')
                    clips.append(dict(anim=role,direction=direction,frame=frame-1,milliseconds=round(duration*1000)))
            spans[role]=(first,len(clips)-first)
        initial=n.props.get('initial_dir',Vec2(0,0));require(isinstance(initial,Vec2),'NPC candidate direction type')
        if initial==Vec2(0,0):initial=Vec2(0,1)
        body=path+'/CollisionShape2D';interact=path+'/interact/CollisionShape2D';near=path+'/NearPlayerArea/CollisionShape2D';view=path+'/ViewArea/CollisionShape2D2';wander=path+'/WanderRadius/CollisionShape2D2';ray=path+'/RayCast2D'
        rect=lambda path:[m.f32(v)for v in (shape(path,'RectangleShape2D')['extents'].x,shape(path,'RectangleShape2D')['extents'].y)]
        delta=lambda child:[m.f32(at(child)[i]-position[i])for i in range(2)]
        actor=dict(source_path=path,position=position,sprite_position=[m.f32(sprite_at[0]+anim['offset'][0]+offset.x),m.f32(sprite_at[1]-int(size[1]/(rows*2))+anim['offset'][1]+offset.y)],interact_center=at(interact),interact_extents=rect(interact),collision_center=at(body),collision_extents=rect(body),sort_y=position[1],item=None,require_flag=None,consume_flag=None,ray_length=json.loads((ROOT/'content/native-house.json').read_bytes())['interaction']['ray_length'],frame=clips[0]['frame'],speed=effective('speed',float),walk_frequency=effective('walk_frequency',float),wander_radius=shape(wander,'CircleShape2D')['radius'],near_offset=delta(near),near_extents=rect(near),view_offset=delta(view),view_radius=shape(view,'CircleShape2D')['radius'],ray_offset=delta(ray),bark_center=[0,0],bark_extents=[0,0],bark_path=None,bark_programme=None,initial_direction=[initial.x,initial.y],rng_lo=0,rng_hi=0)
        for role,name in enumerate(('idle','walk','talk')):actor[name+'_clip'],actor[name+'_clip_count']=spans[role]
        features=int(effective('staring',bool))<<1|int(idle=='Walk')<<2
        ir=dict(texture=dict(source=image,size=list(size),columns=columns,rows=rows,output=binding['texture']),actor=actor,texts=[],commands=[],programmes=[],clips=clips,rng=[])
        blob=field_npc_motion.encode(ir,binding['id'],features)if directions==8 else m.lower(ir,motion_owner=binding['id'],motion_flags=features)
        m.parse(blob,allow_motion=True)if directions==4 else field_npc_motion.parse(blob)
        dialog=n.props.get('dialog','');alternatives=n.props.get('_all_dialog',[])
        records.append(dict(binding=binding,ir=ir,blob=blob,directions=directions,idle_animation=idle,animation=animation,source_animations=anim['animations'],shadow_visible=not effective('no_shadow',bool),declared_no_collision=effective('no_collision',bool),effective_collision='visible_in_tree',has_initial_dialogue=bool(dialog),dialogue=dialog,alternatives=alternatives,return_after_interaction_seconds=0 if effective('staring',bool)else 1))
    return p,records,dict(sorted(ex.sources.items()))


def interaction_blob(row):
    # Durations and gates are source data, not native defaults or tuning.
    from tools.reference_movement import extract
    from tools.run_npc_return_reference import validate_review
    review=json.loads((ROOT/'compatibility/reviews/npc-return-v0410.json').read_bytes());ex=Extractor(ROOT);validate_review(review,ex.lock['commit'])
    functions,hashes=extract(ex.data(review['source']),review['source_sha256'],review['symbols']);require(hashes==review['function_sha256'],'NPC return source methods changed')
    values=re.findall(r'get_tree[(][)].create_timer[(]([0-9.]+)[)]',functions)
    require(len(values)==2 and len(set(values))==1,'NPC return timer source durations differ')
    milliseconds=float(values[0])*1000;require(milliseconds==int(milliseconds),'NPC return duration is not exact milliseconds')
    choices=[row['dialogue']]+[value for group in row['alternatives']for value in group[1:]]
    require(all(bool(value)==row['has_initial_dialogue']for value in choices),'Dynamic empty NPC dialogue is outside reviewed scope')
    require(row['return_after_interaction_seconds']==(0 if int.from_bytes(row['blob'][28:32],'little')&2 else float(values[0])),'NPC return source policy differs')
    features=int.from_bytes(row['blob'][28:32],'little')&7
    return field_npc_motion.encode_policy(row['ir'],row['binding']['id'],features,directions=row['directions'],has_dialogue=row['has_initial_dialogue'],return_milliseconds=int(milliseconds))


def add_room(sections,strings,S,house,resource,*,asset_root,recipe=ROOT/'content/mayor-npcs.json'):
    p,records,sources=build(recipe);hs=house['sections']
    template=next(row for row in hs['ActorProfile']if row['execution_kind']==3 and row['direction_count']==4)
    def identity(name):return max((r['stable_id']for r in sections[name]),default=0)+1
    for row in records:
        binding=row['binding'];a=row['ir']['actor'];texture=row['ir']['texture'];index=len(sections['ActorProfile'])
        require(index+1==binding['id']and len(sections['ActorInstance'])==index,'NPC candidate room identity order')
        file=target(asset_root,binding['texture']);require(file.is_file(),'NPC Room requires actual compiled source texture')
        primary=len(sections['Resource']);sections['Resource'].append(dict(stable_id=identity('Resource'),path_string=S(binding['texture']),width=texture['size'][0],height=texture['size'][1],columns=texture['columns'],rows=texture['rows'],kind=1,flags=0,sha256=hashlib.sha256(file.read_bytes()).hexdigest()))
        first=len(sections['AnimationBinding']);directions=len(sections['DirectionFrame']);idle_clips=[]
        for motion,name in [(0,'Idle'),(4,'Walk'),(5,'Talk')]:
            for direction,keys in enumerate(row['source_animations'][name]['directions']):
                time=m.f32(keys[0]);start=len(sections['Key'])
                for frame,duration in keys[1:]:sections['Key'].append(dict(time=time,frame=frame-1));time=m.f32(time+duration)
                clip=len(sections['Clip']);sections['Clip'].append(dict(stable_id=identity('Clip'),length=time,first_key=start,key_count=len(keys)-1,flags=1|(16 if keys[0]else 0),visibility=0,frame_count=texture['columns']*texture['rows'],channel=0))
                sections['AnimationBinding'].append(dict(actor_profile_index=index,motion_state=motion,direction=direction,clip_index=clip))
                if motion==0:idle_clips.append(clip);sections['DirectionFrame'].append(dict(frame=keys[1][0]-1))
        profile=dict(template,stable_id=binding['id'],execution_kind=4 if row['directions']==8 else 3,flags=int(row['shadow_visible']),primary_resource=primary,shadow_resource=resource(template['shadow_resource']),emote_resource=resource(template['emote_resource']),animation_binding_first=first,animation_binding_count=len(sections['AnimationBinding'])-first,direction_first=directions,direction_count=row['directions'],idle_clip=idle_clips[0],emote_clip=0xffffffff,initial_frame=row['source_animations']['Idle']['directions'][0][1][0]-1,sprite_position=[0,0],sprite_offset=[m.f32(a['sprite_position'][i]-a['position'][i])for i in range(2)])
        sections['ActorProfile'].append(profile)
        sections['ActorInstance'].append(dict(stable_id=binding['id'],profile_index=index,binding_kind=2,flags=1,display_name_string=S(binding['alias']),position=a['position'],direction=a['initial_direction'],initial_clip=idle_clips[0]))
    return {**{'upstream/MOTHER-Encore/'+name:digest for name,digest in sources.items()},Path(recipe).resolve().relative_to(ROOT).as_posix():hashlib.sha256(Path(recipe).read_bytes()).hexdigest(),'tools/field_scene_npcs.py':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'tools/field_npc_motion.py':hashlib.sha256((ROOT/'tools/field_npc_motion.py').read_bytes()).hexdigest()}


def target(root,relative):
    value=(root/relative).resolve();require(value.is_relative_to(root)and value!=root,'NPC candidate output escapes owner');return value


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--recipe',type=Path,default=ROOT/'content/mayor-npcs.json');parser.add_argument('--out',type=Path,required=True);parser.add_argument('--interaction-policy',action='store_true');parser.add_argument('--tex3ds',type=Path);parser.add_argument('--tex3ds-command');a=parser.parse_args();out=a.out.resolve()
    require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build','NPC candidates must remain inside build')
    p,records,sources=build(a.recipe);out.mkdir(parents=True,exist_ok=True);report=[];textures={}
    require(not a.tex3ds_command or a.tex3ds is not None,'NPC texture command needs a fingerprinted real tool')
    if a.tex3ds:
        require(a.tex3ds.is_file(),'Real tex3ds required');command=shlex.split(a.tex3ds_command)if a.tex3ds_command else[str(a.tex3ds.resolve())]
        for row in records:
            image=row['ir']['texture'];relative=row['binding']['texture']
            if relative in textures:require(textures[relative]['source']==image['source'],'NPC texture shared by different sources');continue
            file=target(out,relative);file.parent.mkdir(parents=True,exist_ok=True);temporary=file.with_suffix('.t3x.candidate')
            def rel(path):return path.resolve().relative_to(ROOT).as_posix()
            try:
                subprocess.run(command+['-f','rgba8','-z','none','-o',rel(temporary),rel(ex_path:=ROOT/'upstream/MOTHER-Encore'/image['source'])],cwd=ROOT,check=True)
                require(temporary.is_file()and temporary.stat().st_size>20,'Texture compiler did not emit a real texture');temporary.replace(file)
            finally:
                if temporary.exists():temporary.unlink()
            textures[relative]={'source':image['source'],'source_sha256':hashlib.sha256(ex_path.read_bytes()).hexdigest(),'bytes':file.stat().st_size,'sha256':hashlib.sha256(file.read_bytes()).hexdigest()}
    for row in records:
        blob=interaction_blob(row)if a.interaction_policy else row['blob']
        file=target(out,row['binding']['motion']);file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes(blob)
        report.append({k:v for k,v in row.items()if k!='blob'}|{'bytes':len(blob),'sha256':hashlib.sha256(blob).hexdigest()})
    (out/'candidate.json').write_text(json.dumps(dict(schema=1,source_commit=PIN,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),sources=sources,tex3ds_sha256=hashlib.sha256(a.tex3ds.read_bytes()).hexdigest()if a.tex3ds else None,textures=textures,dependencies={str(v):hashlib.sha256((ROOT/v).read_bytes()).hexdigest()for v in [p['field_recipe'],'content/native-house.json','tools/field_npc_motion.py','tools/mick_treats.py','tools/podunk_field.py']},actors=report,interaction_policy=a.interaction_policy,release_staged=False,unimplemented=['Dialogue selection and choices','Complete Scene Room and lifecycle','Source NPC dialogue animation integration']+([]if a.interaction_policy else['NPC return policy data not selected'])),ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print('NPC candidates:',len(records),'source owners; no scene/release staging')


if __name__=='__main__':main()
