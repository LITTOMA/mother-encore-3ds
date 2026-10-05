#!/usr/bin/env python3
"""Link reviewed key/diary graphs into the existing Room and House resources."""
from pathlib import Path
import hashlib
from tools.extract_battle_entry import require
from tools.basement_progression import load, append_room as programmes, append_house as texts

NONE=0xffffffff

def lower_music_binding(row,ex):
    """Reviewed MusicChanger methods keep their owned-player target namespace."""
    from tools.basement_music_regions import load as load_music
    music=load_music(ex.root)
    regions=[music['region']]+[r['region'] for r in music['additional_regions']]
    require(row['kind'] in (1,5) and row['method'] in ('play_music','stop_music_immediately'),'Unreviewed source region lowering')
    require(any(r['source_path']==row['node'] for r in regions),'Source music owner absent from checked scene')
    for path,h in music['sources'].items():
        if hasattr(ex,'source'):ex.source(path,h)
        else:ex.data(path);require(ex.sources[path]==h,'MusicChanger lowering source mismatch')
    ex.file('content/basement-music-regions.json')
    ex.file('tools/basement_music_regions.py')
    return dict(kind=12 if row['kind']==1 else 11,flags=0,target_index=ex.string(row['node']),auxiliary_index=NONE,value=0,duration=0)

def append_room(ex):
    d=load(ex.root);s=ex.sections
    for row in ex.world_bindings['initial_bindings']+ex.world_bindings['melody_bindings']:
        if row['kind'] not in (1,5):continue
        original=s['Binding'][row['id']-1]
        require(original['stable_id']==row['id'] and original['kind']==row['kind'],'Music stable binding prefix differs')
        original.update(lower_music_binding(row,ex))
    def actor(name):
        # Dialogue's source party identity resolves through the admitted player
        # instance, never a duplicate actor or an arbitrary runtime node path.
        require(name==d['skills'][0]['character'],'Unbound basement actor')
        return s['Scene'][0]['player_instance_index']
    def resource(path):
        matches=[i for i,r in enumerate(s['Resource']) if r['kind']==2 and ex.strings[r['path_string']]=='res://'+path]
        require(len(matches)<=1,'Duplicate source sound resource')
        return matches[0] if matches else ex.add_resource('res://'+path,kind=2)
    def binding(node,method):
        if node=='AnimationPlayer' and method=='play_anim':
            index=len(s['Binding'])
            s['Binding'].append(dict(stable_id=index+1,kind=10,flags=0,target_index=NONE,auxiliary_index=NONE,value=0,duration=0))
            return index
        # Resolve the source melody/music endpoints from their original typed
        # resource bindings. Missing or ambiguous mappings are unsupported.
        if node==d['music']['region']['source_path']:
            require(method in ('stop_music','play_music'),'Unreviewed explicit region method')
            index=len(s['Binding']);stopping=method=='stop_music'
            s['Binding'].append(dict(stable_id=index+1,kind=11 if stopping else 12,flags=0,target_index=ex.string(node),auxiliary_index=NONE,value=0,duration=d['music']['default_stop_seconds'] if stopping else 0))
            return index
        kinds={('melodyBG','appear'):6,('melodyBG','disappear'):7}
        require((node,method) in kinds,'Unbound basement deferred object method')
        matches=[i for i,r in enumerate(s['Binding']) if r['kind']==kinds[node,method]]
        require(len(matches)==1,'Ambiguous basement inherited object binding')
        return matches[0]
    from tools.dialogue_choice_assets import extract
    choice_recipe,_=extract()
    choices={g['id']:i for i,g in enumerate(choice_recipe['groups'])}
    def choice(identity):
        require(identity in choices,'Unbound basement source choice')
        return choices[identity]
    programmes(ex,actor,lambda name:None,binding,resource,choice)
    ex.file('tools/link_basement_content.py')

def append_house(ex,house,room):
    d=load(ex.root)
    from tools.story_input_bindings import load as input_bindings
    labels=input_bindings(ex.root)
    # lockopened is contextual source feedback for this key; derive the name
    # from its YAML translation key rather than a literal in executable code.
    import csv,io
    table={r['key']:r['en'] for r in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/items - sheet.csv')))}
    key=d['key_items'][0]
    names={t['id']:table[key['name_key']] for t in d['texts'] if t['source']=='Data/Dialogue/Reusable/lockopened.yaml'}
    result=texts(ex,house,room,labels,names)
    result['schema']=8
    for path in ('content/native-basement-progression.json','content/story-input-bindings.json','tools/story_input_bindings.py','tools/link_basement_content.py'):
        result['dependencies'][path]=hashlib.sha256((Path(ex.root)/path).read_bytes()).hexdigest()
    return result
