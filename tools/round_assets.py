#!/usr/bin/env python3
"""Reviewed, source-pinned first-round textures and presentation IR.

Only the bounded source tracks below are translated. Procedural tween records
are reviewed source data, never C++ game constants. Upstream remains read-only.
"""
from __future__ import annotations
import argparse, hashlib, json, math, re, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor, animation, node, properties, one
from tools.upstream import read_json,write_json,safe_path
from PIL import Image
RECIPE=ROOT/'content/round-assets.json'
OUT=ROOT/'romfs/round-preview'
REPORT=ROOT/'reports/battle-victory-presentation'
NIL=4294967295

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def validate_source(root,recipe,lock):
    if set(recipe)!={'schema','commit','game_version','licence_review','sources','resources'} or recipe['schema']!=1 or recipe['commit']!=lock['commit'] or recipe['game_version']!=lock['game_version'] or not recipe['licence_review']:
        raise ValueError('Unreviewed round assets schema/source/permission')
    seen=set()
    for path,digest in recipe['sources'].items():
        if sha(safe_path(root,path))!=digest:raise ValueError('Changed round source: '+path)
    for i,r in enumerate(recipe['resources']):
        if set(r)!={'id','name','source','size','grid','output_grid','output'} or r['id']!=i+1 or r['source'] not in recipe['sources'] or r['output'] in seen:raise ValueError('Unknown/duplicate resource')
        safe_path(ROOT/'romfs',r['output']);seen.add(r['output'])
        for field in ['size','grid','output_grid']:
            if len(r[field])!=2 or any(type(x)!=int or x<=0 for x in r[field]):raise ValueError('Invalid resource geometry')
        if any(s%g for s,g in zip(r['size'],r['grid'])) or math.prod(r['grid'])!=math.prod(r['output_grid']):raise ValueError('Invalid atlas mapping')
        if any(s//g*o>1024 for s,g,o in zip(r['size'],r['grid'],r['output_grid'])):raise ValueError('Atlas exceeds GPU extent')

def repack(image,grid,outgrid):
    if image.width%grid[0] or image.height%grid[1] or math.prod(grid)!=math.prod(outgrid):raise ValueError('Invalid atlas mapping')
    w,h=image.width//grid[0],image.height//grid[1]
    output=Image.new('RGBA',(w*outgrid[0],h*outgrid[1]))
    for f in range(math.prod(grid)):
        tile=image.crop((f%grid[0]*w,f//grid[0]*h,(f%grid[0]+1)*w,(f//grid[0]+1)*h))
        output.paste(tile,(f%outgrid[0]*w,f//outgrid[0]*h))
    return output

def compile_assets(root,tex3ds,out=OUT):
    recipe=read_json(RECIPE);validate_source(root,recipe,read_json(ROOT/'upstream.lock'))
    # Extractor checks immutable commit and full reviewed inventory hashes.
    ex=Extractor(ROOT);build=ROOT/'build/round-assets';build.mkdir(parents=True,exist_ok=True);out.mkdir(parents=True,exist_ok=True)
    resources=[]
    for r in recipe['resources']:
        image=Image.open(safe_path(root,r['source'])).convert('RGBA')
        if list(image.size)!=r['size']:raise ValueError('Image dimensions changed')
        converted=repack(image,r['grid'],r['output_grid']);png=build/(r['name']+'.png');converted.save(png)
        target=out/Path(r['output']).name
        subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(png)],check=True)
        resources.append(dict(id=r['id'],name=r['name'],path=r['output'],kind=1,width=converted.width,height=converted.height,columns=r['output_grid'][0],rows=r['output_grid'][1],sha256=sha(target)))
    receipt=dict(schema=1,recipe=recipe,resources=resources,outputs={Path(r['path']).name:dict(sha256=r['sha256'],bytes=(out/Path(r['path']).name).stat().st_size) for r in resources},tex3ds_sha256=sha(tex3ds),limits='Lossless repacking; nearest sampling; no GPU/hardware comparison claim')
    write_json(out/'source.json',receipt)
    export_presentation(ex,resources)
    print('Compiled source-pinned round assets and presentation IR')

def verify(root,out=OUT):
    recipe=read_json(RECIPE);validate_source(root,recipe,read_json(ROOT/'upstream.lock'));receipt=read_json(out/'source.json')
    if receipt.get('recipe')!=recipe:raise ValueError('Stale round asset receipt')
    if set(receipt['outputs'])!={Path(r['output']).name for r in recipe['resources']} or {p.name for p in out.iterdir()}!=set(receipt['outputs'])|{'source.json'}:raise ValueError('Missing/unexpected round files')
    for name,r in receipt['outputs'].items():
        p=safe_path(out,name)
        if sha(p)!=r['sha256'] or p.stat().st_size!=r['bytes']:raise ValueError('Changed round output: '+name)
    print('Verified source-pinned round assets')

class Presentation:
    def __init__(self,ex,resources):self.ex=ex;self.resources=resources;self.media=[];self.tracks=[];self.keys=[];self.events=[];self.bindings={};self.parameters={}
    def resource(self,name):return next(i for i,r in enumerate(self.resources) if r['name']==name)
    def add(self,name,role,resource=NIL,duration=0,rect=(0,0,0,0),flags=0,color=(1,1,1,1),anchor=(.5,.5)):
        m=dict(id=len(self.media)+1,name=name,role=role,resource=resource,first_track=len(self.tracks),track_count=0,first_event=len(self.events),event_count=0,flags=flags,duration=duration,rect=list(rect),color=list(color),anchor=list(anchor));self.media.append(m);return m
    def track(self,m,prop,times,values,update=0,interp=0,mode=0,eases=None):
        if len(times)!=len(values):raise ValueError('Track geometry mismatch')
        self.tracks.append(dict(media=m['id']-1,property=prop,first=len(self.keys),count=len(times),update=update,interpolation=interp,mode=mode));m['track_count']+=1
        for t,v,e in zip(times,values,eases or [1]*len(times)):
            v=list(v) if isinstance(v,(list,tuple)) else [int(v) if isinstance(v,bool) else v]
            self.keys.append(dict(time=t,ease=e,value=v+[0]*(4-len(v))))
    def event(self,m,t,kind):self.events.append(dict(media=m['id']-1,time=t,kind=kind));m['event_count']+=1
    def anim(self,path,name,role,res=NIL,rect=(0,0,0,0),nodepath='AnimationPlayer',flags=0,anchor=(.5,.5)):
        text=self.ex.text(path)
        rid=int(one(r'^anims/'+re.escape(name)+r' = SubResource\( (\d+) \)',text,name)[1]) if '!' in name else node(text,nodepath)['anims/'+name]['SubResource']
        if path=='Nodes/Ui/Battle/EnemySprite.tscn' and name=='defeat':
            # Exact reviewed audio track is retained separately as an audio
            # limitation; there is no audio backend mapping in this visual IR.
            body=one(r'^\[sub_resource type="Animation" id='+str(rid)+r'\]\n(.*?)(?=^\[|\Z)',text,'enemy defeat',re.M|re.S)[1]
            props=properties(body)
            if props.get('tracks/7/type')!='audio' or props.get('tracks/7/path')!='AudioStreamPlayer' or props.get('tracks/7/keys')!={'clips':[{'end_offset':0.0,'start_offset':0.0,'stream':{'ExtResource':4}}],'times':[0]}:raise ValueError('Unreviewed defeat audio track')
            text=text.replace(body,body[:body.index('tracks/7/type')])
        a=animation(text,rid,path,name)
        m=self.add(path+':'+name,role,res,a['length'],rect,flags|(1 if a['loop'] else 0),anchor=anchor)
        mapping={'Sprite:frame':6,'.:frame':6,'Sprite:offset':11,'.:rect_rotation':12,'.:material:shader_param/glow_color':13,'.:material:shader_param/glow_modifier':14,'.:material:shader_param/flash_color':9,'.:material:shader_param/flash_modifier':10,'.:rect_scale':4,'.:scale':4,'.:visible':7,'.:modulate':15,'.:color':8,'.:position':11,'.:rect_size':16,'Sprite:material:shader_param/width':17}
        for tr in a['tracks']:
            k=tr['keys'];p=tr['path']
            if not k['times']:continue
            if tr['type']=='method':
                for t,v in zip(k['times'],k['values']):
                    if v['args'] or v['method'] not in ['_apply_damage','_try_pause']:raise ValueError('Unreviewed action method')
                    self.event(m,t,1 if v['method']=='_apply_damage' else 2)
                continue
            if p in ['Sprite:hframes','Sprite:vframes','Sprite:texture']:
                if len(k['values'])!=1:raise ValueError('Changing resource binding not reviewed')
                r=self.resources[res]
                expected=r['columns'] if p=='Sprite:hframes' else r['rows']
                if p!='Sprite:texture' and k['values'][0]!=expected:raise ValueError('Effect atlas mismatch')
                continue
            if p not in mapping:raise ValueError('Unreviewed presentation track '+path+':'+p)
            if p.endswith('/width') and k['values']!=[0.0]:raise ValueError('Outline rendering outside scope')
            if tr['interp'] not in [1,2]:raise ValueError('Unknown source interpolation')
            self.track(m,mapping[p],k['times'],k['values'],k.get('update',0),7 if tr['interp']==2 else 0,eases=k['transitions'])
        return m
    def bind(self,name,m):self.bindings[name]=m['id']-1;return m

def export_presentation(ex,resources):
    p=Presentation(ex,resources);party='Nodes/Ui/Battle/BattleSpriteNinten.tscn';enemy='Nodes/Ui/Battle/EnemySprite.tscn';battle='Nodes/Ui/Battle/Battle.tscn';dim='Nodes/Ui/Battle/BGDarkinator.tscn';num='Nodes/Ui/Battle/FlyingNumber.tscn';rising='Nodes/Ui/Battle/RisingNumber.tscn'
    for path in ['Scripts/UI/Battle/BattleSystem.gd','Scripts/UI/Battle/BattleSpriteParty.gd','Scripts/UI/Battle/EnemySprite.gd','Scripts/UI/Battle/PartyInfoPlate.gd','Scripts/UI/Battle/BattleDialogueBox.gd','Scripts/UI/AbstractDialogueBox.gd','Scripts/UI/Battle/FlyingNumber.gd','Scripts/UI/Battle/RisingNumber.gd','Scripts/UI/Battle/BattleParticipant.gd','Nodes/Ui/Battle/TargetsBox.gd','Scripts/global/globalData.gd','Scripts/global/Slowmo.gd','Fonts/HitNumber.tres','Nodes/Ui/Battle/BattleDialogueBox.tscn']:
        ex.data(path)
    pr=p.resource('party');en=p.resource('enemy');fx=p.resource('bash');point=p.resource('pointer');font=p.resource('numbers');miss=p.resource('miss');box=p.resource('box');smash=p.resource('smash')
    for slot,name in [('PartyIdle','lookIntoYourSoul'),('PartyPrepare','bashPrep'),('PartyReturn','lookIntoYourSoul'),('PartyGuard','guard'),('PartyHit','hit1'),('PartyHit2','hit2'),('PartyHit3','hit3')]:
        p.bind(slot,p.anim(party,name,1,pr,rect=(0,0,64,64),flags=2,anchor=(.5,1)))
    p.bind('PartyGuardPrepare',p.anim(party,'guardPrep',1,pr,rect=(0,0,64,64),flags=2,anchor=(.5,1)))
    bash=p.anim(party,'bash',1,pr,rect=(0,0,64,64),flags=2,anchor=(.5,1))
    for slot,name in [('EnemyFlash','flash'),('EnemyHit','hit')]:p.bind(slot,p.anim(enemy,name,2,en,rect=(0,0,33,49),flags=2))
    # Source defeat continues independently into the bounded victory scheduler.
    p.bind('EnemyDefeat',p.anim(enemy,'defeat',2,en,rect=(0,0,33,49),flags=2|4))
    effect=p.anim(battle,'bash',3,fx,rect=(0,0,32,32),nodepath='HitEffect/AnimationPlayer',flags=2|4)
    p.bind('TargetPointer',p.anim(battle,'point',5,point,rect=(0,0,25,25),nodepath='TargetsBox/TargetPointer/AnimationPlayer',anchor=(.5,.5)))
    for slot,name in [('BackgroundDim','darken'),('BackgroundUndim','undarken'),('SmashBackground','smash')]:p.bind(slot,p.anim(dim,name,9,rect=(0,0,320,180),anchor=(0,0)))
    p.bind('Smash',p.anim('Nodes/Ui/Battle/Smash.tscn','Smaaaash!!',3,smash,rect=(0,0,resources[smash]['width']/resources[smash]['columns'],resources[smash]['height']/resources[smash]['rows']),flags=2|4))
    flying=p.bind('FlyingNumber',p.anim(num,'start',7,font,rect=(0,0,55,11),flags=4))
    # FlyingNumber.run owns lifetime .8s (AnimationPlayer start is 1s).
    flying['duration']=.8;p.track(flying,2,[0,.8],[0,1],mode=2);p.track(flying,3,[0,.4],[0,-20],interp=3,mode=1);p.track(flying,3,[.4,.8],[-20,180],interp=4,mode=1)
    rising_m=p.bind('RisingNumber',p.anim(rising,'start',8,miss,rect=(-4,-4,40,19),flags=4));p.track(rising_m,3,[0,.6],[0,-16],interp=3,mode=1)
    # Reviewed procedural SceneTreeTween equivalents. Durations/values originate
    # in the pinned scripts above and are kept exclusively in external IR.
    def proc(slot,role,res,duration,rect=(0,0,0,0),flags=0):return p.bind(slot,p.add(slot,role,res,duration,rect,flags))
    m=proc('EnemyAttack',2,en,.2,(0,0,33,49),2);p.track(m,11,[0,.1],[[0,0],[0,-10]],interp=1);p.track(m,11,[.1,.2],[[0,-10],[0,0]],interp=2);p.event(m,.2,1)
    for slot,values in [('PartyShow',[0,1]),('PartyHide',[0,1])]:
        m=proc(slot,1,pr,.12,(0,0,64,64),2);p.track(m,3,[0,.12],values,mode=2)
    m=proc('PartyBounce',1,pr,.52,(0,0,64,64),2);p.track(m,4,[0,.2],[[.6,1.6],[1,1]],interp=1);p.track(m,3,[0,.2],[0,-1],interp=1,mode=2);p.track(m,3,[.32,.52],[[-1,0],[0,1]],interp=4,mode=2)
    m=proc('PartyShake',1,pr,.2,(0,0,64,64),2);p.track(m,3,[0,.15],[5,0],interp=6,mode=1);p.track(m,2,[0,.2],[10,0],interp=5,mode=1)
    m=proc('PlateQuake',4,NIL,.6);p.track(m,3,[.1,.15],[0,8],mode=2);p.track(m,3,[.15,.25],[8,-8],interp=3,mode=2);p.track(m,3,[.25,.35],[-4,4],interp=3,mode=2);p.track(m,3,[.35,.45],[4,-4],interp=3,mode=2);p.track(m,3,[.45,.6],[-4,0],interp=3,mode=2)
    for slot,role,res,rect in [('EnemyDodge',2,en,(0,0,33,49)),('PartyDodge',1,pr,(0,0,64,64))]:
        m=proc(slot,role,res,.2,rect,2);p.track(m,2,[0,.1],[0,8],interp=1,mode=2);p.track(m,2,[.1,.2],[8,0],interp=2,mode=2)
    dialogue=node(ex.text('Nodes/Ui/Battle/BattleDialogueBox.tscn'),'Dialoguebox')
    dr=[dialogue['margin_left'],dialogue['margin_top'],dialogue['margin_right']-dialogue['margin_left'],dialogue['margin_bottom']-dialogue['margin_top']]
    proc('Dialogue',6,box,0,dr)['anchor']=[.5,0]
    proc('DialogueText',6,NIL,0,(19,12,282,32))['anchor']=[.5,0]
    cursor=node(ex.text('Nodes/Ui/Battle/BattleDialogueBox.tscn'),'Dialoguebox/Cursor_Down')
    m=proc('DialogueCursor',3,p.resource('dialogue_cursor'),.8,(dr[0]+cursor['position'][0],dr[1]+cursor['position'][1],8,8),3);m['anchor']=[.5,0]
    p.track(m,6,[0,.2,.4,.6],[0,1,2,1],update=1);p.track(m,12,[0],[math.degrees(cursor['rotation'])])
    bt=ex.text(battle);name=node(bt,'TargetNameBox');label=node(bt,'TargetNameBox/Label')
    # Final target name box follows the already source-validated AnimAction y=0.
    proc('TargetNameBox',6,box,0,(name['margin_left'],0,name['margin_right']-name['margin_left'],-name['margin_top']))['anchor']=[1,0]
    proc('TargetNameText',6,NIL,0,(name['margin_left']+label['margin_left'],label['margin_top']+(label['margin_bottom']-label['margin_top']-12)/2,label['margin_right']-label['margin_left'],label['margin_bottom']-label['margin_top']),2)['anchor']=[1,0]
    abstract=ex.text('Scripts/UI/AbstractDialogueBox.gd');globaldata=ex.text('Scripts/global/globalData.gd');plate=ex.text('Scripts/UI/Battle/PartyInfoPlate.gd')
    def constant(text,key):return float(one(r'^const '+key+r'\s*:?=\s*([0-9.]+)',text,key)[1])
    p.parameters=dict(TextTiming=[json.loads(one(r'const TEXT_SPEEDS := (\[[^\n]+\])',globaldata,'text speeds')[1])[0],constant(abstract,'SPEED_UP_FROM_PRESS_A'),constant(abstract,'SPEED_UP_FROM_PRESS_B'),node(ex.text('Nodes/Ui/Battle/BattleDialogueBox.tscn'),'Timer')['wait_time']],TextTagSpeeds=[constant(abstract,'NORMAL_SPEED'),constant(abstract,'FASTER_SPEED'),constant(abstract,'SLOWER_SPEED'),0],HpDigits=[10,constant(plate,'TRANSITION_FRAMES'),0,0],PartyHit=[16,2,3,node(ex.text(party),'.')['_hits']],PartyShown=[20,.12,.12,1.3],FlyingNumberRandom=[32,64,0,0],DamageGlyphGrid=[32,11,11,11],TargetPointerOffset=[1.5,0,0,0],TargetGlow=[1,1,1,.25],DialogueMargins=[dialogue['patch_margin_'+s] for s in ['left','top','right','bottom']],DialogueTextLayout=[19,12,282,15],DamageShadow=[1,1,0,0],SmashTiming=[.5,.5,0,0],SmashOffset=[0,-32,0,0])
    # Additional source-derived bounce/plate response coefficients are media data.
    # Store them in PartyBounce's rect x/y, which are not a visual layout origin.
    # Instead dedicated parameter schema keeps their intended interpretation explicit.
    root_rising=node(ex.text(rising),'.');p.parameters['RisingNumberSize']=[root_rising['margin_right']-root_rising['margin_left'],root_rising['margin_bottom']-root_rising['margin_top'],0,0]
    p.parameters['PartyBounceMotion']=[32,16,.32,.52];p.parameters['PlateHitIntensity']=[1,1.5,.1,0]
    # Victory is a source actor clip plus the independent, looping YouWin UI.
    p.bind('PartyVictory',p.anim(party,'victory',1,pr,rect=(0,0,64,64),flags=2,anchor=(.5,1)))
    dialog_scene='Nodes/Ui/Battle/BattleDialogueBox.tscn';dt=ex.text(dialog_scene)
    win_anim=animation(dt,node(dt,'AnimationPlayer')['anims/YouWin']['SubResource'],dialog_scene,'YouWin')
    yn=node(dt,'Dialoguebox/ClipBox/YouWin');clipbox=node(dt,'Dialoguebox/ClipBox')
    yr=p.resource('victory');yr_size=resources[yr]
    banner=p.bind('VictoryBanner',p.add('Dialoguebox:YouWin',3,yr,win_anim['length'],
        (dr[0]+clipbox['margin_left']+yn['position'][0],dr[1]+clipbox['margin_top']+yn['position'][1],yr_size['width']/yr_size['columns'],yr_size['height']/yr_size['rows']),flags=3,anchor=(.5,0)))
    banner_paths={'ClipBox/YouWin:frame':6,'ClipBox/YouWin:visible':7}
    for tr in win_anim['tracks']:
        k=tr['keys'];path=tr['path']
        if tr['type']!='value' or tr['interp']!=1:raise ValueError('Unreviewed victory banner track')
        if path in banner_paths:p.track(banner,banner_paths[path],k['times'],k['values'],k['update'],eases=k['transitions'])
        elif path in ['ClipBox/HBoxContainer:visible','Cursor_Down:visible']:
            if k!={'times':[0],'transitions':[1],'update':1,'values':[False]}:raise ValueError('Changed victory UI visibility')
        else:raise ValueError('Unreviewed victory banner target')
    # Split one source AnimationPlayer into typed visual lanes and method events.
    # The shared sampler executes these tracks; there is no secondary interpreter.
    outgoing=animation(bt,node(bt,'AnimScene')['anims/transitionOut']['SubResource'],battle,'transitionOut')
    return_paths={'top:rect_position':('ReturnTop',10,1),'bottom:rect_position':('ReturnBottom',10,1),'PlayerInfo:rect_position:y':('ReturnPlate',4,3)}
    callbacks={'_turn_party_to_overworld':4,'_hide_battle_BG':5,'_hide_enemies':6,'_jump_to_overworld':7,'_rotate_party_to_original_direction':8}
    deferred=[]
    for tr in outgoing['tracks']:
        k=tr['keys'];path=tr['path']
        if tr['interp']!=1:raise ValueError('Unreviewed return interpolation')
        if tr['type']=='method':
            if path!='.':raise ValueError('Unknown return callback target')
            for t,v in zip(k['times'],k['values']):
                if v['args'] or v['method'] not in callbacks:raise ValueError('Unreviewed return callback')
                deferred.append((t,callbacks[v['method']]))
        elif path in return_paths:
            slot,role,prop=return_paths[path]
            if slot=='ReturnPlate':rect=(0,k['values'][0],0,0);anchor=(.5,1);color=(1,1,1,1)
            else:
                n=node(bt,path.split(':')[0]);rect=(0,0,n['margin_right']-n.get('margin_left',0),n.get('margin_bottom',0)-n['margin_top']);anchor=(0,0 if slot=='ReturnTop' else 1);color=n['color']
            m=p.bind(slot,p.add(slot,role,NIL,outgoing['length'],rect,color=color,anchor=anchor))
            p.track(m,prop,k['times'],k['values'],k['update'],eases=k['transitions'])
        else:raise ValueError('Unreviewed return track')
    timeline=p.bind('ReturnTimeline',p.add('Battle:transitionOut',10,NIL,outgoing['length']))
    for time,kind in deferred:p.event(timeline,time,kind)
    # Exact reviewed _jump_to_overworld SceneTreeTween; x and base-y normalize
    # against the fresh world projection supplied by the camera/world bridge.
    world=p.resource('world_party');wr=resources[world]
    jump=p.bind('PartyJumpToWorld',p.add('BattleSystem:_jump_to_overworld',11,world,.6,(0,0,wr['width']/wr['columns'],wr['height']/wr['rows']),flags=2|4,anchor=(.5,.5)))
    p.track(jump,2,[0,.55],[0,1],mode=2)
    p.track(jump,3,[0,.35],[0,1],interp=3,mode=2)
    p.track(jump,11,[0,.35],[[0,0],[0,-24]],interp=3)
    p.track(jump,11,[.4,.6],[[0,-24],[0,4]],interp=8)
    p.track(jump,4,[0,.4],[[.6,1.2],[1,1]],interp=8)
    p.track(jump,4,[.4,.6],[[1,1],[.8,1.1]],interp=8)
    p.track(jump,6,[0],[3+18*wr['columns']],update=1)
    p.event(jump,.6,9)
    p.parameters['ReturnPartyGeometry']=[0,-4,24,4]
    p.parameters['ReturnPartyFrames']=[3+18*wr['columns'],3+3*wr['columns'],0,0]
    p.parameters['ReturnPartyTurn']=[0,-1,.05,0]
    for path,digest in read_json(RECIPE)['sources'].items():ex.data(path)
    report=dict(schema=1,sources=ex.sources,resources=[{k:v for k,v in r.items() if k!='name'} for r in resources],media=p.media,tracks=p.tracks,keys=p.keys,events=p.events,bindings=p.bindings,parameters=p.parameters,skill_media={'attack':dict(user_media=bash['id']-1,hit_media=effect['id']-1),'tackle':dict(user_media=bash['id']-1,hit_media=effect['id']-1),'float':dict(user_media=NIL,hit_media=NIL),'guard':dict(user_media=p.bindings['PartyGuard'],hit_media=NIL)})
    REPORT.mkdir(parents=True,exist_ok=True);write_json(REPORT/'presentation.json',report)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('action',choices=['compile','verify','extract']);parser.add_argument('--root',type=Path,default=ROOT/'upstream/MOTHER-Encore');parser.add_argument('--tex3ds',type=Path);args=parser.parse_args()
    try:
        if args.action=='compile':
            if args.tex3ds is None:raise ValueError('Need official tex3ds path')
            compile_assets(args.root,args.tex3ds)
        elif args.action=='extract':export_presentation(Extractor(ROOT),read_json(OUT/'source.json')['resources'])
        else:verify(args.root)
        return 0
    except (ValueError,OSError,KeyError,TypeError,subprocess.SubprocessError) as e:print('ROUND ASSET ERROR:',e,file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
