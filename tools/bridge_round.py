#!/usr/bin/env python3
"""Original bridge stray-dog mechanics, pre-hit keys and ordinary loot data.

This adapter inherits the reviewed shared action/rolling-HP/return mechanisms.
It does not re-use the opening enemy's stats, AI, reward pool or win policy.
"""
from __future__ import annotations
import argparse,copy,csv,io,json,re,shlex,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT));sys.path.insert(0,str(ROOT/'tools'))
from tools.extract_battle_entry import Extractor,require,properties,PIN
from tools import native_round as native,bridge_entry
IR=ROOT/'content/bridge-round.json'
PACK=ROOT/'romfs/battle/straydog/bridge.encround'
ASSET=ROOT/'content/asset-receipts/graphics/battle/straydog/pre-hit.json'
REVIEW=ROOT/'reports/bridge-battle/source-review.json'
BITE='Graphics/Battle Effects/Bite.png'
BITE_OUTPUT='graphics/battle/straydog/bite.t3x'
NIL=0xffffffff

def read(p):return bridge_entry.read(p)
def write(p,v):bridge_entry.write(p,v)
def sha(p):return bridge_entry.sha(p)
def effect(ex):
    source='Nodes/Ui/Battle/Battle.tscn';scene=ex.text(source)
    block=re.search(r'^\[sub_resource type="Animation" id=84\]\n(.*?)(?=^\[|\Z)',scene,re.M|re.S)
    require(block is not None and 'anims/bite = SubResource( 84 )'in scene,'Bite animation source binding')
    data=properties(block[1]);require(data['resource_name']=='bite' and data['length']==.55,'Changed reviewed bite lifetime')
    tracks={}
    for key,value in data.items():
        if key.startswith('tracks/'):
            _,index,field=key.split('/');tracks.setdefault(int(index),{})[field]=value
    require(set(tracks)==set(range(8)) and all(t['enabled'] and not t['imported']for t in tracks.values()),'Bite source track coverage')
    require([(t['type'],t['path'])for t in tracks.values()]==[('value','Sprite:texture'),('value','Sprite:hframes'),('value','Sprite:vframes'),('value','Sprite:frame'),('value','.:rect_size'),('value','.:visible'),('audio','../AudioStreamPlayer'),('method','.')],'Unknown bite track mechanism')
    require(tracks[0]['keys']['values']==[{'ExtResource':88}]*2 and tracks[1]['keys']['values']==[11] and tracks[2]['keys']['values']==[1],'Bite texture/grid projection')
    require(tracks[6]['keys']==dict(clips=[dict(end_offset=0.0,start_offset=.05,stream=None)],times=[0]),'Unreviewed non-null pre-hit audio track')
    calls=tracks[7]['keys'];require(calls['times']==[.25] and calls['values']==[dict(args=[],method='_apply_damage')],'Bite damage callback changed')
    require(tracks[4]['keys']['values']==[[91,94]] and tracks[5]['keys']['times']==[0,.55] and tracks[5]['keys']['values']==[True,False],'Bite size/visibility changed')
    size=ex.png_size(BITE);ex.data(BITE+'.import');require(size==[1001,85],'Bite source image grid changed')
    frame=tracks[3]['keys'];visible=[(time,value)for time,value in zip(frame['times'],frame['values'])if time<data['length']]
    require(visible==[(round(i*.05,2),i)for i in range(11)],'Changed visible bite frame keys')
    # The native source has stale frame 12 at the hidden endpoint and later
    # unreachable keys. Godot's setter rejects it and retains frame 10; the
    # extracted-script oracle records that actual result, not a patched source.
    require(list(zip(frame['times'],frame['values']))[11]==(.55,12),'Changed hidden invalid source frame')
    ex.text('Scripts/UI/Battle/PreHitEffect.gd')
    return dict(source=source,length=data['length'],apply_time=calls['times'][0],frames=visible,size=size,grid=[11,1])

def assets(tex3ds,command):
    require(tex3ds and tex3ds.is_file(),'Real tex3ds required');ex=Extractor(ROOT);spec=effect(ex);ex.data('LICENSE');target=ROOT/'romfs'/BITE_OUTPUT;target.parent.mkdir(parents=True,exist_ok=True)
    rel=lambda p:p.resolve().relative_to(ROOT).as_posix()
    source=ex.upstream/BITE
    args=shlex.split(command)+['-f','rgba8','-z','none','-o',rel(target),rel(source)]if command else[str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(source)]
    subprocess.run(args,cwd=ROOT,check=True)
    write(ASSET,dict(schema=1,commit=PIN,sources=ex.sources,spec=spec,path=BITE_OUTPUT,sha256=sha(target),bytes=target.stat().st_size,producer_sha256=sha(__file__),tex3ds_sha256=sha(tex3ds),permission=bridge_entry.policy()['license_review']))

def build(root=ROOT):
    require(Path(root).resolve()==ROOT,'Bridge adapter requires its checked repository root')
    ex=Extractor(ROOT);base=read(ROOT/'content/pillow-round.json');native.verify_sources(base)
    entry=read(ROOT/'content/bridge-entry.json');bridge_entry.validate(entry);recipe=bridge_entry.policy()
    for source,digest in base['sources'].items():require(sha(ex.upstream/source)==digest,'Changed inherited round dependency');ex.data(source)
    enemy=ex.yaml('Data/Battlers/'+recipe['battler']+'.yaml');item=ex.yaml('Data/Items/'+enemy['items'][0]['item']+'.yaml')
    require(enemy['items']==[dict(chance=40,item='FleaBag')] and not item['keyitem'] and item['actions']==[] and item['slot']=='','Unreviewed ordinary bridge drop pool')
    require(enemy['skills']==[dict(skill='bite',weight=2),dict(skill='bash',weight=1)],'Changed source bridge AI')
    receipt=read(ASSET);spec=effect(ex)
    require(receipt['commit']==PIN and receipt['spec']==json.loads(json.dumps(spec)) and receipt['producer_sha256']==sha(__file__) and receipt['path']==BITE_OUTPUT and receipt['sha256']==sha(ROOT/'romfs'/BITE_OUTPUT),'Stale bite asset receipt')
    ir=copy.deepcopy(base);ir['schema']=6;ir['binding'].update(battle_id=recipe['battle_id'],win_flag=entry['entry']['win_flag'],show_intro_outro=int(entry['entry']['show_intro_outro']))
    translations={}
    for name in ('battletext','battlers','battleskills','items','menus'):
        for row in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/'+name+' - sheet.csv'))):translations[row['key']]=row['en']
    player=entry['party']['initial_save_data']['name'];name=translations[enemy['name']];party_articles=translations['ARTICLES_NINTEN'].split(',');enemy_articles=translations[enemy['article']].split(',')
    def context(actor):
        p=actor=='party';d=dict(name=player if p else name,target=name if p else player)
        for prefix,articles in (('n',party_articles if p else enemy_articles),('t',enemy_articles if p else party_articles)):
            d.update({prefix+str(i):a.replace('{0}',d['name']if prefix=='n'else d['target'])for i,a in enumerate(articles)})
        return d
    def text(key,role,ctx=None,literal=None):
        raw=translations[key]if key else literal;require(type(raw)is str,'Missing source battle text');value=raw
        for k,v in (ctx or {}).items():value=value.replace('{'+k+'}',str(v))
        require(not re.search(r'\{[^}]+\}',value),'Unreviewed bridge text context '+key)
        ir['texts'].append(dict(id=len(ir['texts'])+1,role=role,key=key,source_text=raw,text=value));return len(ir['texts'])-1
    for index,skill_name in ((1,'bite'),(2,'bash')):
        source='Data/BattleSkills/'+skill_name+'.yaml';skill=ex.yaml(source);old=ir['skills'][index]
        require(skill['action_type']==0 and skill['target_type']==0 and skill['skill_type']=='basic' and skill['damage_type']=='normal' and not skill['pp_cost'] and not skill['hp_cost'] and not skill['use_sound'],'Unreviewed bridge skill execution')
        require(skill.get('pre_hit_effect','')==('bite'if skill_name=='bite'else'') and skill['hit_effect']==(''if skill_name=='bite'else'bash') and skill['hit_sound']=='bash','Changed bridge skill presentation')
        ir['skills'][index]=dict(old,id=index+1,source=source,name=text(skill['name'],1),description=text(skill['description'],2),dialog=text(skill['dialog'],3,context('enemy')),action_type=0,target_type=0,skill_type=1,damage_type=1,traits=0,power=skill['damage_or_heal'],variance=skill['variance'],priority=skill['priority'],miss_chance=skill['miss_chance'],pp_cost=0,hp_cost=0,user_media=NIL,hit_media=NIL if skill_name=='bite'else base['skills'][1]['hit_media'])
    ir['enemy_choices']=[dict(skill=index+1,weight=row['weight'])for index,row in enumerate(enemy['skills'])]
    binding=ir['binding'];binding.update(enemy_name=text(enemy['name'],4),enemy_article=text(enemy['article'],5))
    for field,actor in (('enemy_outro','enemy'),('mortal_damage','enemy'),('no_effect','party')):
        row=base['texts'][binding[field]];binding[field]=text(row['key'],row['role'],context(actor),row['source_text'])
    ir['victory'].update(reward_exp=enemy['exp'],reward_cash=enemy['cash'],reward_item_count=len(enemy['items']),enemy_body_id=entry['binding']['enemy_instance']+1,exp_text=text('BATTLE_MSG_EXP_ONE_ALLY',9,dict(context('party'),value=enemy['exp'])))
    ir['encounter'].update(boss=0,keep_actor=0,post_win_script='',boss_flash_media=NIL,stop_area_music_if_overworld=1)
    ir['boss_shakes']=[]
    art=read(ROOT/recipe['receipt']);enemy_art=next(r for r in art['resources']if r['name']=='enemy');presentation=ir['presentation'];index=next(m['resource']for m in presentation['media']if m['role']==2)
    presentation['resources'][index].update(path=enemy_art['output'],width=enemy_art['width'],height=enemy_art['height'],columns=1,rows=1,sha256=sha(ROOT/'romfs'/enemy_art['output']))
    for media in presentation['media']:
        if media['role']==2:media['rect'][2:]=[enemy_art['width'],enemy_art['height']]
    resource=len(presentation['resources']);presentation['resources'].append(dict(id=max(r['id']for r in presentation['resources'])+1,path=BITE_OUTPUT,kind=1,width=spec['size'][0],height=spec['size'][1],columns=11,rows=1,sha256=receipt['sha256']))
    media=len(presentation['media']);first=len(presentation['tracks']);keys=presentation['keys']
    for property,values in [(6,[(t,[f,0,0,0])for t,f in spec['frames']]),(7,[(0,[1,0,0,0]),(spec['length'],[0,0,0,0])])]:
        start=len(keys);keys.extend(dict(time=t,ease=1,value=v)for t,v in values)
        presentation['tracks'].append(dict(media=media,property=property,first=start,count=len(values),update=1,interpolation=0,mode=0))
    presentation['media'].append(dict(id=max(m['id']for m in presentation['media'])+1,name='Nodes/Ui/Battle/Battle.tscn:bite',role=3,resource=resource,first_track=first,track_count=2,first_event=len(presentation['events']),event_count=0,flags=6,duration=spec['length'],rect=[0,0,spec['size'][0]/spec['grid'][0],spec['size'][1]/spec['grid'][1]],color=[1,1,1,1],anchor=[.5,.5]))
    ir['pre_hit']=[dict(skill=1,media=media,apply_time=spec['apply_time'])]
    # The source party atlas is fully retained (10x18 -> 15x12, same row-major
    # frame identity). Bat needs its own preparation and damage/pause events.
    from tools.round_assets import Presentation
    from tools.round_animation_bindings import load as animation_bindings
    animator=Presentation(ex,presentation['resources']);animator.animation_config=animation_bindings('content/round-animation-bindings.json',ex)
    for key in ('media','tracks','keys','events'):setattr(animator,key,presentation[key])
    bat_media=animator.anim('Nodes/Ui/Battle/BattleSpriteNinten.tscn','bat',1,0,[0,0,64,64],flags=2,anchor=(.5,1))['id']-1
    bat_prepare=animator.anim('Nodes/Ui/Battle/BattleSpriteNinten.tscn','batPrep',1,0,[0,0,64,64],flags=2,anchor=(.5,1))['id']-1
    attack=ex.yaml('Data/BattleSkills/attack.yaml');bat=ex.yaml('Data/BattleSkills/attack_bat.yaml')
    require(bat==dict(attack,user_anim='bat',use_sound='swing'),'Unreviewed weapon action differences')
    require(attack['use_sound']=='','Default source attack sound changed')
    ir['skills'].append(dict(ir['skills'][0],id=5,source='Data/BattleSkills/attack_bat.yaml',user_media=bat_media))
    ir['action_media']=[dict(skill=i,preparation=(presentation['bindings']['PartyPrepare']if i==0 else presentation['bindings']['PartyGuardPrepare']if i==3 else bat_prepare if i==4 else NIL),use_sound=int(i==4))for i in range(len(ir['skills']))]
    ctx=context('party');item_name=translations[item['name']];ctx.update(item=item_name)
    for i,article in enumerate(translations[item['article']].split(',')):ctx['i'+str(i)]=article.replace('{0}',item_name)
    ir['loot']=dict(item=enemy['items'][0]['item'],chance=enemy['items'][0]['chance'],doses=1,present_text=text('BATTLE_MSG_PRESENT',13,ctx),inside_text=text('BATTLE_MSG_PRESENT_INSIDE',13,ctx),taking_text=text('BATTLE_MSG_PRESENT_TAKING',13,ctx),sound='Audio/Sound effects/EB/itemget1.wav',flags=0)
    ex.data(ir['loot']['sound']);ex.data('Scripts/UI/Battle/BattleItemPool.gd');ex.data('Scripts/global/Item.gd')
    ir['sources']={**base['sources'],**entry['sources'],**ex.sources}
    ir['dependencies']={p:sha(ROOT/p)for p in ['content/pillow-round.json','content/bridge-entry.json','content/bridge-encounter.json','content/podunk-npcs.json',ASSET.relative_to(ROOT).as_posix()]}
    ir['scope']='Original bridge stray dog: bite/bash weighted AI, checked pre-hit timing, default or plastic-bat player attack with original preparation, damage/pause keys and swing SE, one non-rare FleaBag pool, 8 EXP/20 cash and beat_dog_bridge. No full inventory dropped-world-item branch, arbitrary enemies, battle items, statuses or multi-party.'
    return ir

def validate(ir,root=ROOT):
    require(ir==build(root),'Bridge round differs from fresh pinned source lowering')
    review=read(REVIEW);require(review==dict(schema=6,commit=PIN,ir_sha256=sha(IR),sources=ir['sources'],dependencies=ir['dependencies'],scope=ir['scope']),'Stale bridge round review')

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=('assets','extract','compile','verify'));p.add_argument('--tex3ds',type=Path);p.add_argument('--tex3ds-command');a=p.parse_args()
    if a.action=='assets':assets(a.tex3ds,a.tex3ds_command);print('Compiled original bite art; source callback timing retained');return
    if a.action=='extract':ir=build();write(IR,ir);write(REVIEW,dict(schema=6,commit=PIN,ir_sha256=sha(IR),sources=ir['sources'],dependencies=ir['dependencies'],scope=ir['scope']))
    ir=read(IR);validate(ir);blob=native.encode(native.lower(ir));native.parse_pack(blob)
    if a.action=='verify':require(PACK.read_bytes()==blob,'Stale bridge round pack')
    else:PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(blob)
    print('Original bridge round',a.action,len(blob),'bytes; v6 source admission only, integration requires live inventory/world consumers')

if __name__=='__main__':
    try:main()
    except (ValueError,KeyError,TypeError,OSError,subprocess.SubprocessError)as e:print('BRIDGE ROUND ERROR:',e,file=sys.stderr);raise SystemExit(1)
