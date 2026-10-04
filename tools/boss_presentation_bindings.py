"""Checked source selectors for the supported boss presentation and promotion."""
import hashlib,json,math,re
from pathlib import Path
from extract_battle_entry import ROOT,PIN,one,properties,node,require
IR=Path(__file__).resolve().parents[1]/'content/boss-presentation-bindings.json'
# Execution schemas; scene paths, names, timing and semantic bindings are IR.
MEDIA={'EnemySprite':2,'BossFlash':12}
PROPERTY={'color':8,'flash_color':9,'flash_modifier':10,'glow_modifier':14,'modulate':15,'radius':18}
EVENT={'BossFlashStart':10,'BossKillEnemies':11}
STAT={'maxhp':1,'maxpp':2,'offense':3,'defense':4,'speed':5,'iq':6,'guts':7}

def fields(value,names,label):
    require(type(value)is dict and set(value)==set(names),'Unknown/missing '+label)
def unique(pairs):
    value={}
    for key,item in pairs:
        require(key not in value,'Duplicate boss binding JSON field');value[key]=item
    return value
def read(path):return json.loads(Path(path).read_text(encoding='utf-8'),object_pairs_hook=unique)
def safe(path):return type(path)is str and 0<len(path)<=512 and ':'not in path and '\\'not in path and all(x not in('','.','..')for x in path.split('/'))
def identifier(value):return type(value)is str and re.fullmatch('[A-Za-z_][A-Za-z_0-9]*',value)
def finite(value):return type(value)in(int,float)and math.isfinite(value)

def animation(source,recipe):
    player=node(source,recipe['player_node'])
    require('anims/'+recipe['clip']in player,'Unknown boss animation clip')
    rid=player['anims/'+recipe['clip']]['SubResource']
    body=one(r'^\[sub_resource type="Animation" id='+str(rid)+r'\]\n(.*?)(?=^\[|\Z)',source,recipe['clip'],re.M|re.S)[1]
    props=properties(body);tracks={}
    for key,value in props.items():
        if key.startswith('tracks/'):
            _,index,field=key.split('/');tracks.setdefault(int(index),{})[field]=value
    require(set(tracks)==set(range(len(tracks))),'Missing boss animation track')
    require(finite(props.get('length'))and props['length']>0,'Invalid boss animation duration')
    seen_values=set();seen_methods=set();seen_audio=set()
    for track in tracks.values():
        require(track['enabled']and not track['imported']and track['interp']==1,'Disabled/imported/unreviewed boss track')
        keys=track['keys']
        if track['type']=='value':
            require(track['path']in recipe['value_properties']and track['path']not in seen_values,'Unbound/duplicate boss value target')
            seen_values.add(track['path'])
        elif track['type']=='method':
            require(track['path']==recipe['method_node'],'Boss callback receiver mismatch')
            require(len(keys['times'])==len(keys['values']),'Boss callback key span mismatch')
            for value in keys['values']:
                require(type(value)is dict and set(value)=={'method','args'}and value['method']in recipe['methods'],'Unknown boss callback')
                binding=recipe['methods'][value['method']];seen_methods.add(value['method'])
                if binding['operation']=='Shake':require(len(value['args'])==3 and all(finite(v)and v>0 for v in value['args']),'Unsupported boss shake arguments')
                else:require(value['args']==[],'Unsupported boss callback arguments')
        elif track['type']=='audio':
            binding=recipe['audio'];require(track['path']==binding['node']and track['path']not in seen_audio,'Unknown/duplicate boss audio target')
            seen_audio.add(track['path']);require(keys==binding['keys'],'Changed reviewed boss audio clip data')
            resource=one(r'^\[ext_resource path="res://([^"]+)" type="([^"]+)" id='+str(binding['resource_id'])+r'\]',source,'boss audio resource')
            require(resource[1]==binding['source']and resource[2]=='AudioStream','Boss audio source identity mismatch')
            node(source,binding['node'])
        else:raise ValueError('Unreviewed boss track type')
    require(seen_values==set(recipe['value_properties'])and seen_methods==set(recipe['methods'])and seen_audio=={recipe['audio']['node']},'Incomplete boss animation binding coverage')
    return props,tracks

def load(root=ROOT,recipe=None):
    root=Path(root);recipe=read(IR)if recipe is None else recipe
    fields(recipe,['schema','commit','sources','animations','scene_links','encounter','progression','texts','translation_sources','source_facts'],'boss bindings')
    require(type(recipe['schema'])is int and recipe['schema']==1 and recipe['commit']==PIN,'Unsupported boss binding schema/pin')
    inventory=read(root/'compatibility/upstream-inventory.json');lock=read(root/'upstream.lock')
    require(inventory['commit']==lock['commit']==PIN and type(recipe['sources'])is dict and 8<=len(recipe['sources'])<=32,'Boss source coverage')
    sources={}
    for path,sha in recipe['sources'].items():
        require(safe(path)and path in inventory['files']and sha==inventory['files'][path]['sha256'],'Unreviewed boss source '+str(path))
        data=(root/'upstream/MOTHER-Encore'/path).read_bytes();require(hashlib.sha256(data).hexdigest()==sha,'Changed boss source '+path)
        if Path(path).suffix in('.gd','.tscn','.yaml','.csv'):sources[path]=data.decode('utf-8')
    require(type(recipe['animations'])is dict and set(recipe['animations'])=={'enemy_defeat','defeat_flash'},'Missing boss animation purposes')
    for purpose,row in recipe['animations'].items():
        fields(row,['source','player_node','clip','role','resource','geometry','flags','anchor','binding_slot','value_properties','method_node','methods','audio'],'boss animation binding')
        require(type(row['source'])is str and row['source']in sources and safe(row['player_node'])and identifier(row['clip'])and type(row['role'])is str and row['role']in MEDIA,'Unknown boss animation selector/role')
        require(row['resource']in('enemy','none')and row['geometry']in('enemy','viewport')and row['binding_slot']in('EnemyDefeat',''),'Unknown boss presentation binding')
        require(type(row['flags'])is int and 0<=row['flags']<=7 and type(row['anchor'])is list and len(row['anchor'])==2 and all(finite(v)and 0<=v<=1 for v in row['anchor']),'Invalid boss presentation geometry')
        require(type(row['value_properties'])is dict and 1<=len(row['value_properties'])<=16,'Missing boss property mapping')
        for target,prop in row['value_properties'].items():
            require(type(target)is str and target and type(prop)is int and prop==PROPERTY.get(target.split('/')[-1].split(':')[-1]),'Unknown/mismapped boss property schema')
            node(sources[row['source']],target.split(':')[0])
        require(row['method_node']=='.'and type(row['methods'])is dict and 1<=len(row['methods'])<=8,'Unsupported boss method receiver')
        for method,binding in row['methods'].items():
            fields(binding,['operation','script','signal'],'boss method binding')
            require(identifier(method)and binding['operation']in set(EVENT)|{'Shake'}and binding['script']in sources,'Unknown boss method operation/source')
            require((binding['operation']=='BossKillEnemies')==(row['role']=='BossFlash'),'Incompatible boss callback media role')
            script_id=node(sources[row['source']],'.')['script']['ExtResource']
            actual_script=one(r'^\[ext_resource path="res://([^"]+)" type="Script" id='+str(script_id)+r'\]',sources[row['source']],'boss receiver script')[1]
            require(actual_script==binding['script'],'Boss method bound to unrelated scene script')
            body=one(r'^func '+re.escape(method)+r'\([^\n]*\):[^\n]*\n(.*?)(?=^func |\Z)',sources[binding['script']],method,re.M|re.S)[1]
            if binding['operation']=='Shake':require(binding['signal']==''and 'Shaker.new('in body,'Changed boss shake mechanism')
            else:require(identifier(binding['signal'])and 'emit_signal("'+binding['signal']+'")'in body and 'signal '+binding['signal']in sources[binding['script']],'Changed boss callback signal binding')
        audio=row['audio'];fields(audio,['node','resource_id','source','keys'],'boss audio binding')
        require(safe(audio['node'])and type(audio['resource_id'])is int and audio['resource_id']>0 and audio['source']in recipe['sources'],'Unknown boss audio binding')
        fields(audio['keys'],['times','clips'],'boss audio keys')
        require(type(audio['keys']['times'])is list and 1<=len(audio['keys']['times'])<=16 and len(audio['keys']['times'])==len(audio['keys']['clips'])and all(finite(v)and v>=0 for v in audio['keys']['times']),'Invalid boss audio times')
        for clip in audio['keys']['clips']:
            fields(clip,['end_offset','start_offset','stream'],'boss audio clip')
            fields(clip['stream'],['ExtResource'],'boss audio resource reference')
            require(finite(clip['end_offset'])and finite(clip['start_offset'])and clip['end_offset']==0 and clip['start_offset']==0 and type(clip['stream']['ExtResource'])is int and clip['stream']['ExtResource']==audio['resource_id'],'Unsupported boss audio clip')
        animation(sources[row['source']],row)
    links=recipe['scene_links'];fields(links,['battle','enemy'],'boss scene source links')
    battle=links['battle'];fields(battle,['source','flash_node','animation'],'battle flash instance link')
    require(battle['source']in sources and safe(battle['flash_node'])and battle['animation']in recipe['animations'],'Unknown battle flash instance selector')
    match=one(r'^\[node name="'+re.escape(battle['flash_node'])+r'" parent="\." instance=ExtResource\( (\d+) \)\]',sources[battle['source']],'battle boss flash instance')
    actual=one(r'^\[ext_resource path="res://([^"]+)" type="PackedScene" id='+match[1]+r'\]',sources[battle['source']],'battle flash scene')[1]
    require(actual==recipe['animations'][battle['animation']]['source'],'Battle flash instance source mismatch')
    enemy_link=links['enemy'];fields(enemy_link,['source','constant','animation'],'enemy dynamic scene link')
    require(enemy_link['source']in sources and identifier(enemy_link['constant'])and enemy_link['animation']in recipe['animations'],'Unknown enemy scene creation selector')
    actual=one(r'^const '+re.escape(enemy_link['constant'])+r' := preload\("res://([^"]+)"\)',sources[enemy_link['source']],'enemy scene preload')[1]
    require(actual==recipe['animations'][enemy_link['animation']]['source'],'Enemy scene creation source mismatch')
    encounter=recipe['encounter'];fields(encounter,['battle_id','enemy_source','cutscene_source','cutscene_step','actor','body_source','sprite_source','receipt_resource','party_articles_key','system_source'],'boss encounter binding')
    require(type(encounter['battle_id'])is int and encounter['battle_id']>0 and identifier(encounter['actor'])and identifier(encounter['receipt_resource'])and encounter['enemy_source']in sources and encounter['cutscene_source']in sources and encounter['sprite_source']in recipe['sources']and safe(encounter['body_source'])and type(encounter['cutscene_step'])is str and encounter['cutscene_step'].isdigit()and identifier(encounter['party_articles_key']),'Unknown boss encounter identity')
    require(encounter['system_source']in sources,'Unknown boss system source')
    entry=read(root/'content/doll-entry.json');require(entry['binding']['stable_id']==encounter['battle_id'],'Boss stable encounter identity mismatch')
    import yaml
    enemy=yaml.safe_load(sources[encounter['enemy_source']]);require(entry['enemy']['id']==encounter['actor']and entry['enemy']['data']==enemy and enemy.get('boss')is True and not enemy.get('items'),'Boss enemy source/actor mismatch')
    asset=entry['presentation']['assets'].get(encounter['receipt_resource'])
    require(asset and asset['kind']=='texture'and asset['source']==encounter['sprite_source']and asset['texture_size']==entry['enemy']['sprite_size'],'Boss sprite asset source binding mismatch')
    cutscene=yaml.safe_load(sources[encounter['cutscene_source']]);require(encounter['cutscene_step']in cutscene and 'startbattle'in cutscene[encounter['cutscene_step']],'Unknown boss cutscene step');request=cutscene[encounter['cutscene_step']]['startbattle']
    require(request['battlers']==[{encounter['actor']:encounter['actor']}]and request['actorskeep']=={encounter['actor']:True}and safe(request['wincutscene']),'Boss cutscene binding mismatch')
    from native_content import parse_pack
    room=parse_pack((root/'romfs/data/opening.encroom').read_bytes())
    require(sum(room['strings'][b['source_path_string']]==encounter['body_source']for b in room['sections']['BodyRule'])==1,'Unknown/ambiguous boss body binding')
    progression=recipe['progression'];fields(progression,['source','character_source','party_constant','party_id','learn_table','promoted_level','following_level','learned_skill','skill_source','stat_order','stat_table','stat_labels'],'boss progression binding')
    require(progression['source']in sources and progression['character_source']in sources and progression['skill_source']in sources and all(identifier(progression[k])for k in('party_constant','party_id','learn_table','learned_skill','stat_table')),'Unknown progression source/identity')
    require(type(progression['promoted_level'])is int and 1<progression['promoted_level']<10 and type(progression['following_level'])is int and progression['following_level']==progression['promoted_level']+1,'Unsupported promotion range')
    pm=sources[progression['source']];actual=one(r'^const '+re.escape(progression['party_constant'])+r' := "([^"]+)"',pm,'progression party')[1]
    require(actual==progression['party_id']==read(root/'content/native-battle.json')['party']['id'],'Party progression identity mismatch')
    member=one(r'^const '+re.escape(progression['learn_table'])+r' := \{\n\t'+re.escape(progression['party_constant'])+r': \{(.*?)^\t\}',pm,'learn table member',re.M|re.S)[1]
    learned=json.loads(one(r'^\t\t'+str(progression['promoted_level'])+r': (\[[^\n]+\])',member,'promotion learning')[1]);require(learned==[progression['learned_skill']],'Unsupported promotion learned skill binding')
    require(progression['skill_source']=='Data/BattleSkills/'+progression['learned_skill']+'.yaml','Learned skill source identity mismatch')
    skill=yaml.safe_load(sources[progression['skill_source']]);require(skill['use_cases']==-1 and 'required_weapon'not in skill,'Unreviewed learned skill usability')
    require(type(progression['stat_order'])is list and len(progression['stat_order'])==len(STAT)and set(progression['stat_order'])==set(STAT),'Unknown/duplicate progression stats')
    targets=one(r'^const '+re.escape(progression['stat_table'])+r' := \{\n\t'+re.escape(progression['party_constant'])+r': \{(.*?)^\t\}',pm,'stat table member',re.M|re.S)[1]
    character=sources[progression['character_source']]
    article_prefix=one(r'^\s*return "([^"]*)" \+ _name\.to_upper\(\)',character,'party article source convention')[1]
    require(encounter['party_articles_key']==article_prefix+progression['party_id'].upper(),'Party source article identity mismatch')
    names=[one(r'^const '+m+r' := "([^"]+)"',character,'stat constant')[1]for m in re.findall(r'^\t\t([A-Z]+):',targets,re.M)]
    require(names==progression['stat_order'],'Changed source progression stat order')
    require(type(recipe['translation_sources'])is list and 1<=len(recipe['translation_sources'])<=16 and len(set(recipe['translation_sources']))==len(recipe['translation_sources'])and all(p in sources for p in recipe['translation_sources']),'Unknown translation source binding')
    import csv,io
    keys={row['key']for path in recipe['translation_sources']for row in csv.DictReader(io.StringIO(sources[path]))}
    require(encounter['party_articles_key']in keys,'Unknown party article binding')
    require(type(progression['stat_labels'])is dict and set(progression['stat_labels'])==set(progression['stat_order'])and all(type(key)is str and key in keys for key in progression['stat_labels'].values()),'Unknown progression stat label binding')
    fields(recipe['texts'],['experience','growth','level','learning'],'progression text bindings')
    for purpose,role in [('experience',9),('growth',11),('level',10),('learning',12)]:
        row=recipe['texts'][purpose];fields(row,['key','role'],'progression text identity');require(row['key']in keys and type(row['role'])is int and row['role']==role,'Unknown progression text schema/key')
    require(type(recipe['source_facts'])is dict and len(recipe['source_facts'])>=4,'Missing boss semantic source facts')
    for path,facts in recipe['source_facts'].items():
        require(path in sources and type(facts)is list and facts and all(type(f)is str and f and f in sources[path]for f in facts)and len(set(facts))==len(facts),'Changed boss semantic fact '+str(path))
    return recipe

def check_round(ir,recipe,root=ROOT):
    if not ir.get('encounter',{}).get('boss'):return
    require(ir['binding']['battle_id']==recipe['encounter']['battle_id'],'Unknown boss round identity')
    encounter=ir['encounter'];progression=recipe['progression'];require(encounter['promoted_level']==progression['promoted_level']and encounter['learned_skill']==progression['learned_skill'],'Boss round progression binding mismatch')
    require([g['stat']for g in ir['growth']]==[STAT[s]for s in progression['stat_order']],'Boss round stat binding mismatch')
    root=Path(root)
    import yaml
    binding=recipe['encounter'];request=yaml.safe_load((root/'upstream/MOTHER-Encore'/binding['cutscene_source']).read_text(encoding='utf-8'))[binding['cutscene_step']]['startbattle']
    require(encounter['post_win_script']==request['wincutscene']and encounter['keep_actor']==int(request['actorskeep'][binding['actor']]),'Boss round retained actor/post-win source mismatch')
    from native_content import parse_pack
    room=parse_pack((root/'romfs/data/opening.encroom').read_bytes())
    body=next(b for b in room['sections']['BodyRule']if room['strings'][b['source_path_string']]==binding['body_source'])
    require(ir['victory']['enemy_body_id']==body['body_id']and encounter['following_level_exp']==room['sections']['Experience'][progression['following_level']-1]['required_total_exp'],'Boss body/next-level stable binding mismatch')
    for purpose,index in [('experience',ir['victory']['exp_text']),('level',encounter['level_text']),('learning',encounter['learned_text'])]:
        text=ir['texts'][index];require(text['key']==recipe['texts'][purpose]['key']and text['role']==recipe['texts'][purpose]['role'],'Boss progression text binding mismatch')
    party=read(root/'content/native-battle.json')['party']
    for row,stat in zip(ir['growth'],progression['stat_order']):
        targets=party['stat_targets'][stat];after=int(targets[0]+(targets[1]-targets[0])*(progression['promoted_level']/10.0));before=party['base_stats'][stat];boost=party['effective_stats'][stat]-before
        require(row['before']==party['effective_stats'][stat]and row['after']==after+boost,'Boss progression source stat value mismatch')
        if after!=before:require(ir['texts'][row['text']]['key']==recipe['texts']['growth']['key'],'Boss growth text binding mismatch')
    presentation=ir['presentation']
    for purpose,binding in recipe['animations'].items():
        props,source_tracks=animation((Path(root)/'upstream/MOTHER-Encore'/binding['source']).read_text(encoding='utf-8'),binding)
        index=presentation['bindings'][binding['binding_slot']]if binding['binding_slot']else encounter['boss_flash_media']
        media=presentation['media'][index]
        require(media['name']==binding['source']+':'+binding['clip']and media['role']==MEDIA[binding['role']]and media['flags']==binding['flags']and media['anchor']==binding['anchor'],'Boss round media binding mismatch')
        require(media['duration']==props['length'],'Boss round duration differs from source')
        tracks=presentation['tracks'][media['first_track']:media['first_track']+media['track_count']]
        require([t['property']for t in tracks]==list(binding['value_properties'].values()),'Boss round property mapping mismatch')
        from round_assets import Presentation
        expected=Presentation(None,[]);expected_media=expected.add('',MEDIA[binding['role']])
        expected_events=[];expected_shakes=[]
        for tr in source_tracks.values():
            keys=tr['keys']
            if tr['type']=='value':expected.track(expected_media,binding['value_properties'][tr['path']],keys['times'],keys['values'],keys.get('update',0),eases=keys['transitions'])
            elif tr['type']=='method':
                for time,value in zip(keys['times'],keys['values']):
                    operation=binding['methods'][value['method']]['operation']
                    if operation=='Shake':expected_shakes.append((time,*value['args']))
                    else:expected_events.append((time,EVENT[operation]))
        for actual,wanted in zip(tracks,expected.tracks):
            require({k:v for k,v in actual.items()if k not in('first','media')}=={k:v for k,v in wanted.items()if k not in('first','media')},'Boss round track schema differs from source')
            require(presentation['keys'][actual['first']:actual['first']+actual['count']]==expected.keys[wanted['first']:wanted['first']+wanted['count']],'Boss round keys differ from source')
        events=presentation['events'][media['first_event']:media['first_event']+media['event_count']]
        require([e['kind']for e in events]==[EVENT[v['operation']]for v in binding['methods'].values()if v['operation']in EVENT],'Boss round callback mapping mismatch')
        require([(e['time'],e['kind'])for e in events]==expected_events,'Boss round callback timing differs from source')
        if expected_shakes:require([(s['time'],s['magnitude'],s['length'],s['interval'])for s in ir['boss_shakes']]==expected_shakes,'Boss round shakes differ from source')
