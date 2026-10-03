#!/usr/bin/env python3
"""Link the reviewed Carol/phone staging IR into the shipping source IR.

This is an offline, bounded source adapter, never a runtime interpreter. Existing
Room/House identity prefixes are preserved; the reviewed Dad-normal graph is
linked to typed, checked branch, option and save-continuation commands.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / 'tools'))
from tools.doll_dialogue import PIN, require
from tools.doll_postwin import receipt as postwin_receipt
from tools.phone_dialogue import build as dialogue_build, NORMAL
from tools.phone_assets import build as presentation_build, RING_SOUND, HANGUP_SOUND
from tools.world_geometry import f32

NONE = 0xffffffff
DIALOGUE_FIRST_ID = 12
ACTORS = {'None': 65535, 'Ninten': 0, 'Carol': 5}
STAGE_DIALOGUE = 'content/phone-stage/dialogue.json'
STAGE_PRESENTATION = 'content/phone-stage/presentation.json'
REPORT = 'reports/phone-integration/link-review.json'
DAD_CHOICES = 'content/native-dialogue-choices.json'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load_stage(root=ROOT):
    root = Path(root)
    dialogue = json.loads((root / STAGE_DIALOGUE).read_text())
    presentation = json.loads((root / STAGE_PRESENTATION).read_text())
    require(dialogue == dialogue_build(root), 'Stale/unreviewed phone dialogue stage')
    require(presentation == presentation_build(root), 'Stale/unreviewed phone presentation stage')
    require(dialogue['commit'] == presentation['commit'] == PIN, 'Phone linker pin')
    require(len(dialogue['programs']) == 6 and dialogue['dad_normal']['source_path'] == NORMAL
            and dialogue['dad_normal']['execution_status'] == 'not_integrated', 'Phone linear scope')
    require(len(presentation['objects']) == 1 and presentation['objects'][0]['source_path'] == 'Objects/Phone',
            'Phone object binding changed')
    return dialogue, presentation


def linear_texts(dialogue):
    paths = {p['source_path'] for p in dialogue['programs']}
    result = [t for t in dialogue['texts'] if t['source_path'] in paths]
    identities = [t['identity'] for t in result]
    require(len(set(identities)) == len(identities), 'Duplicate phone text identity')
    return result


def text_ids(dialogue):
    return {t['identity']: DIALOGUE_FIRST_ID + i for i, t in enumerate(linear_texts(dialogue))}


def load_dad(root=ROOT):
    from tools.dad_record_dialogue import build
    from tools.dialogue_choice_assets import verify_recipe
    dad = build(root)
    choices = json.loads((Path(root) / DAD_CHOICES).read_text())
    verify_recipe(choices)
    graph = dad['program']
    require(choices['groups'][:1] == graph['choice_groups'], 'Dad choice graph differs from program')
    require(choices['graph_sha256'] == hashlib.sha256(json.dumps(graph, sort_keys=True).encode()).hexdigest(),
            'Dad choice graph fingerprint differs')
    require(graph['identity'] == 'Reusable/dad_normal' and graph['source_path'] == NORMAL,
            'Dad program identity changed')
    return dad, choices


def dad_text_ids(dialogue, dad):
    first = DIALOGUE_FIRST_ID + len(linear_texts(dialogue))
    return {t['identity']: first + i for i, t in enumerate(dad['texts'])}


def append_dad_room(ex, dialogue):
    from tools.native_content import OPCODES
    dad, choices = load_dad(ex.root)
    for path in ['tools/dad_record_dialogue.py', 'tools/dialogue_choice_assets.py', 'tools/menu_audio_binding.py',
                 DAD_CHOICES, 'reports/dad-record/native-layout.json']:
        ex.file(path)
    for document in [dad, choices]:
        for path, digest in document['sources'].items():
            ex.source(path, digest)
    sections = ex.sections
    flags = {ex.strings[f['name_string']]: i for i, f in enumerate(sections['Flag'])}
    texts = dad_text_ids(dialogue, dad)
    groups = {g['id']: i for i, g in enumerate(choices['groups'])}
    require(len(groups) == len(choices['groups']), 'Duplicate Dad choice identity')
    graph = dad['program']
    first = len(sections['Command'])
    for source in graph['commands']:
        kind = source['kind']
        target, auxiliary = NONE, NONE
        require(source['actor'] == 'None' and kind in OPCODES, 'Unreviewed Dad actor/command')
        if kind == 'ShowDialogue': target = texts[source['dialogue_key']]
        elif kind == 'Jump': target = source['target_pc']
        elif kind in ('SetFlag', 'BranchFlag'):
            target = flags[source['flag']]
            if kind == 'BranchFlag': auxiliary = source['target_pc']
        elif kind == 'BranchLeader':
            target, auxiliary = ex.string(source['leader']), source['target_pc']
        elif kind == 'AwaitChoices': target = groups[source['choice_group']]
        sections['Command'].append(dict(opcode=OPCODES.index(kind), actor_index=ACTORS['None'],
            phrase=source['phrase'], target_index=target, flags=source.get('flags', 0),
            vector=[0, 0], value=source.get('value', 0), duration=source.get('duration', 0),
            auxiliary_index=auxiliary))
    sections['Program'].append(dict(stable_id=len(sections['Program']) + 1, first_command=first,
        command_count=len(graph['commands']), phrase_count=len(graph['source_labels']),
        source_path_string=ex.string(graph['identity'])))
    ex.record_map('Program ' + graph['identity'],
        STAGE_DIALOGUE + ';' + DAD_CHOICES + ';upstream/MOTHER-Encore/' + NORMAL,
        dict(source_labels=graph['source_labels'], label_to_pc=graph['label_to_pc'],
             choice_groups=graph['choice_groups'], reviewed_noops=graph['reviewed_noops']),
        'rules6; all source leader/earned/saved branches; program-relative label entries; '
        'text-completion choices; hidden Record and callback-only save gate; checked House value/delay/newline tokens')


def append_carol(ex):
    """Share only proven identical frame tracks; derive Carol's own geometry."""
    from tools.extract_native_content import node_block, prop, pair
    import yaml
    s = ex.sections
    require(len(s['ActorProfile']) == len(s['ActorInstance']) == 5, 'Carol actor prefix changed')
    presentation = ex.document('content/native-house-presentation.json')
    recipe = ex.document('content/house-assets.json')
    native = postwin_receipt(ex)
    source = ex.text('Maps/podunk/Nintens House.tscn')
    nodes = [node_block(source, p) for p in ['Objects/npc', 'Objects/npc3']]
    # Both instances inherit the original npc.gd default 4dir YAML and zero
    # authored sprite offset. Different image heights affect profile offsets,
    # not the shared frame-index/timing tracks.
    npc_script = ex.text('Scripts/Main/npc.gd')
    require('res://Data/Animations/4dir.yaml' in npc_script, 'NPC default animation changed')
    import re
    for block in nodes:
        require(not re.search(r'^(yaml|sprite_offset|no_shadow|idle_animation|talk_idle_animation) =', block, re.M),
                'Carol/Minnie animation override needs review')
    anim = yaml.safe_load(ex.text('Data/Animations/4dir.yaml'))
    require(anim == native['yaml'][2] and anim['size'] == [5, 4] and anim['offset'] == [0, 0],
            'Carol/Minnie source YAML/grid/offset differ')
    resources = {r['role']: r for r in presentation['resources']}
    profiles = {p['role']: p for p in presentation['profiles']}
    for role, block in zip(['carol', 'minnie'], nodes):
        r = resources[role]
        source_resource = next(a for a in recipe['resources'] if a['role'] == role)
        require(json.loads(prop(block, 'sprite')) == source_resource['source'].removeprefix('Graphics/Character Sprites/').removesuffix('.png'),
                'Carol/Minnie texture binding changed')
        ex.source(source_resource['source'], recipe['sources'][source_resource['source']])
        require([r['columns'], r['rows']] == anim['size'] and profiles[role]['directions'] == 4,
                'Carol/Minnie grid/directions differ')
    minnie = s['ActorProfile'][4]
    bindings = s['AnimationBinding'][minnie['animation_binding_first']:
                                    minnie['animation_binding_first'] + minnie['animation_binding_count']]
    require(len(bindings) == 12, 'Carol shared native clip count')
    names = ['Down', 'Left', 'Right', 'Up']
    for b in bindings:
        state = {0: 'Idle', 4: 'Walk', 5: 'Talk'}[b['motion_state']]
        source_clip = native['animations'][1][state + ' ' + names[b['direction']]]
        clip = s['Clip'][b['clip_index']]
        keys = s['Key'][clip['first_key']:clip['first_key'] + clip['key_count']]
        require(clip['length'] == f32(source_clip['length']) and clip['frame_count'] == 20
                and keys == [dict(time=f32(t), frame=int(f)) for t, f in source_clip['keys']],
                'Carol shared native clip differs: ' + state)
    r = resources['carol']
    resource = ex.add_resource(r['path'], r['width'], r['height'], r['columns'], r['rows'], expected=r['sha256'])
    profile = copy.deepcopy(minnie)
    sprite_position = pair(prop(node_block(ex.text('Nodes/Reusables/npc.tscn'), 'CharacterSprite'), 'position'))
    frame_height = r['height'] / r['rows']
    original = s['Resource'][minnie['primary_resource']]
    bubble_extra = -minnie['emote_offset'][1] - original['height'] / original['rows']
    offset = [f32(anim['offset'][0]), f32(-int(frame_height / 2) + anim['offset'][1])]
    require([f32(sprite_position[i] + offset[i]) for i in range(2)] == profiles['carol']['sprite_offset'],
            'Carol House/Room sprite origin disagreement')
    profile.update(stable_id=6, primary_resource=resource, sprite_position=sprite_position,
                   sprite_offset=offset, emote_offset=[0, f32(-(frame_height + bubble_extra))],
                   animation_binding_first=len(s['AnimationBinding']), direction_first=len(s['DirectionFrame']))
    s['ActorProfile'].append(profile)
    s['AnimationBinding'].extend(dict(b, actor_profile_index=5) for b in bindings)
    s['DirectionFrame'].extend(copy.deepcopy(s['DirectionFrame'][minnie['direction_first']:
                                                minnie['direction_first'] + minnie['direction_count']]))
    house = ex.document('reports/cloud-world/house-exact.json')
    hn = {n['path']: n for n in house['nodes']}
    s['ActorInstance'].append(dict(stable_id=6, profile_index=5, binding_kind=2,
        flags=int(all(hn[p]['properties'].get('visible', True) for p in ('.', 'Objects', 'Objects/npc'))),
        display_name_string=ex.string(hn['Objects/npc']['name']), position=pair(prop(nodes[0], 'position')),
        direction=[0, 1], initial_clip=profile['idle_clip']))
    ex.record_map('Carol ActorProfile/ActorInstance/AnimationBinding/Resource',
        'upstream/MOTHER-Encore/Maps/podunk/Nintens House.tscn;upstream/MOTHER-Encore/Data/Animations/4dir.yaml;reports/doll-postwin/native-parser-animation.json;content/native-house-presentation.json',
        'Objects/npc; exact inherited 4dir YAML/grid/zero authored offset; native Idle/Walk/Talk frame tracks',
        'Append actor index5; share verified frame tracks only; own Carol texture and 32px frame-height offsets')


def append_entrance_flag_listener(ex, presentation):
    """Lower the reviewed FlagLandmark's deferred subtree deletion to its body."""
    from tools.extract_battle_entry import node, one
    import re
    scene_path = 'Maps/podunk/Nintens House.tscn'
    reusable_path = 'Nodes/Reusables/flag landmarks.tscn'
    script_path = 'Scripts/Main/Flag Landmarks.gd'
    data_path = 'Scripts/global/globalData.gd'
    scene, reusable, script, global_data = [ex.text(p) for p in
                                          [scene_path, reusable_path, script_path, data_path]]
    path = presentation['entrance_guard']['source_path']
    guard = node(scene, path)
    inherited = node(reusable, '.')
    require(inherited == {'script': {'ExtResource': 1}}, 'Entrance FlagLandmark reusable changed')
    require('path="res://' + script_path + '" type="Script" id=1' in reusable,
            'Entrance FlagLandmark script binding')
    scene_id = one(r'^\[ext_resource path="res://' + re.escape(reusable_path)
        + r'" type="PackedScene" id=(\d+)\]$', scene, 'FlagLandmark scene resource')[1]
    one(r'^\[node name="' + re.escape(path) + r'" parent="\." instance=ExtResource\( '
        + scene_id + r' \)\]$', scene, 'Entrance FlagLandmark instance')
    defaults = {key: one(r'^export var ' + key + r' = "([^"]*)"$', script, key)[1]
                for key in ['appear_flag', 'disappear_flag']}
    delete_default = one(r'^export var delete_if_hidden = (true|false)$', script, 'delete_if_hidden')[1] == 'true'
    require(set(guard) <= {'appear_flag', 'disappear_flag', 'delete_if_hidden'},
            'Unreviewed Entrance FlagLandmark overrides')
    appear = guard.get('appear_flag', defaults['appear_flag'])
    disappear = guard.get('disappear_flag', defaults['disappear_flag'])
    require(not appear and disappear == presentation['entrance_guard']['disappear_flag']
            and guard.get('delete_if_hidden', delete_default) is True,
            'Entrance listener requires reviewed disappear-only subtree deletion')
    require('func _ready():\n\t_check_flags()\n\tglobal.connect("flags_updated", self, "_check_flags")' in script
            and 'var shown = globaldata.check_appear_disappear_flags(appear_flag, disappear_flag)' in script
            and 'if delete_if_hidden and !shown:\n\t\tqueue_free()' in script,
            'Entrance listener ready/signal/deferred deletion changed')
    require('if disappear_flag != "":\n\t\tflag_on = flag_on and !flags.get(disappear_flag, false)' in global_data
            and 'if emit_signal: global.emit_signal("flags_updated")' in global_data,
            'Entrance listener disappearance/signal semantics changed')
    bodies = [b for b in ex.sections['BodyRule']
              if ex.strings[b['source_path_string']].startswith(path + '/')]
    require(len(bodies) == 1 and bodies[0]['initially_enabled'] == 1,
            'Entrance FlagLandmark descendant collider scope changed')
    body = bodies[0]
    body_path = ex.strings[body['source_path_string']]
    native = ex.document('reports/cloud-world/house-exact.json')
    source_node = next(n for n in native['nodes'] if n['path'] == body_path)
    require(source_node['class'] == 'StaticBody2D' and node(scene, body_path).get('visible') is False,
            'Entrance collider identity/independent visibility changed')
    flag = next(i for i, f in enumerate(ex.sections['Flag']) if ex.strings[f['name_string']] == disappear)
    binding_id = len(ex.sections['Binding']) + 1
    ex.sections['Binding'].append(dict(stable_id=binding_id, kind=9, flags=0,
        target_index=body['body_id'], auxiliary_index=flag, value=1, duration=0))
    ex.record_map('Entrance DeferredFlagBodyDeletion Binding',
        ';'.join('upstream/MOTHER-Encore/' + p for p in [scene_path, reusable_path, script_path, data_path]),
        dict(ancestor=path, descendant_body=body_path, body_id=body['body_id'], disappear_flag=disappear,
             flag_index=flag, delete_if_hidden=True, source_ready_check=True, signal='flags_updated',
             source_removal='queue_free()', stable_binding_id=binding_id),
        'Disappearance queues ancestor subtree destruction after the scene frame; remove its checked collider permanently for that instance. Original BodyRule/Polygon geometry and initial visibility remain unchanged.')


def link_room(ex):
    from tools.native_content import OPCODES
    dialogue, presentation = load_stage(ex.root)
    s = ex.sections
    require(len(s['Resource']) == 34 and len(s['Program']) == 5 and len(s['Binding']) == 7,
            'Phone Room append-only prefix changed')
    for file in [STAGE_DIALOGUE, STAGE_PRESENTATION, 'tools/link_phone_content.py',
                 'tools/phone_dialogue.py', 'tools/phone_assets.py',
                 'compatibility/reviews/phone-dialogue.json', 'compatibility/reviews/phone-presentation.json',
                 'content/phone-stage/native-parser.json']:
        ex.file(file)
    for doc in [dialogue, presentation]:
        for path, digest in doc['sources'].items():
            ex.source(path, digest)
    append_carol(ex)
    for path in [RING_SOUND, HANGUP_SOUND]:
        ex.add_resource('res://' + path, kind=2)
    binding = len(s['Binding'])
    s['Binding'].append(dict(stable_id=binding + 1, kind=8, flags=0, target_index=0,
                             auxiliary_index=NONE, value=0, duration=0))
    append_entrance_flag_listener(ex, presentation)
    flags = {ex.strings[f['name_string']]: i for i, f in enumerate(s['Flag'])}
    texts = text_ids(dialogue)
    for program in dialogue['programs']:
        first = len(s['Command'])
        for a in program['commands']:
            kind = a['kind']
            require(kind in OPCODES and a['actor'] in ACTORS, 'Unreviewed phone command/actor')
            target = NONE
            if kind == 'MoveActorPath':
                p = a['path']
                require(set(p) == {'movement', 'speed', 'animation', 'type'}
                        and p['animation'] == 'Walk' and p['type'] == 'step', 'Phone movement mode')
                target = len(s['MovementPath'])
                entry = len(s['MovementEntry'])
                for e in p['movement']:
                    if set(e) == {'wait'}:
                        s['MovementEntry'].append(dict(kind=1, vector=[0, 0], duration=e['wait']))
                    else:
                        require(set(e) == {'x', 'y'}, 'Unknown phone movement entry')
                        s['MovementEntry'].append(dict(kind=0, vector=[f32(e['x']), f32(e['y'])], duration=0))
                s['MovementPath'].append(dict(stable_id=target + 1, first_entry=entry,
                    entry_count=len(p['movement']), flags=1, animation_motion=4, speed=p['speed']))
            elif kind == 'CallObjectDeferred':
                require((a['binding'], a['source_object'], a['source_method']) ==
                        ('phone_ring', 'Objects/Phone', '_ring'), 'Phone object call scope')
                target = binding
            elif kind == 'SetFlag':
                target = flags[a['flag']]
            elif kind == 'ShowDialogue':
                target = texts[a['dialogue_key']]
            elif kind == 'TurnActor' and 'target_actor' in a:
                target = ACTORS[a['target_actor']]
            elif kind == 'MoveCamera':
                # Reviewed DialogueBox default TRANS_SINE/EASE_OUT mode.
                target = 1
            s['Command'].append(dict(opcode=OPCODES.index(kind), actor_index=ACTORS[a['actor']],
                phrase=a['phrase'], target_index=target, flags=a.get('flags', 0),
                vector=[f32(v) for v in a.get('vector', [0, 0])], value=a.get('value', 0),
                duration=a.get('duration', 0), auxiliary_index=NONE))
        s['Program'].append(dict(stable_id=len(s['Program']) + 1, first_command=first,
            command_count=len(program['commands']), phrase_count=len(program['source_labels']),
            source_path_string=ex.string(program['identity'])))
        ex.record_map('Program ' + program['identity'], STAGE_DIALOGUE + ';upstream/MOTHER-Encore/' + program['source_path'],
            dict(dense_phase_source_labels=program['source_labels'], command_order='reviewed source handler order'),
            'rules5; typed text/hide/automatic gate, queued actor target, axis-preserving camera, original deferred Phone object call')
    ex.record_map('Phone Resource/Binding', STAGE_PRESENTATION,
        'Objects/Phone index0; phonering and phonehangup; StartPhoneRing binding8',
        'Original InteractDialog remains separate from actor/NPC and does not mark NPC seen')
    append_dad_room(ex, dialogue)


def link_house(ex, ir, room):
    dialogue, presentation = load_stage(ex.root)
    require(ir['schema'] == 4 and [d['id'] for d in ir['dialogues']] == list(range(1, 12)),
            'Phone House append-only text prefix changed')
    require(len(ir['npcs']) == 4 and ir['npcs'][0]['source_path'] == 'Objects/npc', 'Carol House binding')
    paths = {room['strings'][p['source_path_string']]: i for i, p in enumerate(room['sections']['Program'])}
    require(all(p['identity'] in paths for p in dialogue['programs']), 'House requires linked phone Room')
    for doc in [dialogue, presentation]:
        for path, digest in doc['sources'].items():
            ex.data(path)
            require(ex.sources[path] == digest, 'Phone House source mismatch')
    color = dialogue['text_contracts']['hint_color_hex']
    token_kinds = {'Literal': 1, 'PlayerName': 2, 'HintStart': 3, 'HintEnd': 4}
    for text in linear_texts(dialogue):
        first = len(ir['segments'])
        for i, segment in enumerate(text['segments']):
            tokens = []
            for token in segment['tokens']:
                require(token['kind'] in token_kinds, 'Unimplemented phone linear text token')
                value = color if token['kind'] == 'HintStart' else token.get('text', '')
                tokens.append(dict(kind=token_kinds[token['kind']], text=value))
            ir['segments'].append(dict(id=len(ir['segments']) + 1, speaker=text['speaker_en'],
                voice=text['voice'], tokens=tokens, flags=3 if i < len(text['segments']) - 1 else 5))
        ir['dialogues'].append(dict(id=text_ids(dialogue)[text['identity']], source_path=text['source_path'],
            first_segment=first, segment_count=len(text['segments'])))
    dad, choices = load_dad(ex.root)
    require(dad['program']['identity'] in paths, 'House requires linked Dad Room program')
    for document in [dad, choices]:
        for path, digest in document['sources'].items():
            ex.data(path)
            require(ex.sources[path] == digest, 'Dad House source mismatch')
    for text in dad['texts']:
        first = len(ir['segments'])
        for i, segment in enumerate(text['segments']):
            ir['segments'].append(dict(id=len(ir['segments']) + 1, speaker=text['speaker_en'],
                voice=text['voice'], tokens=copy.deepcopy(segment['house_tokens']),
                flags=3 if i < len(text['segments']) - 1 else 5))
        ir['dialogues'].append(dict(id=dad_text_ids(dialogue, dad)[text['identity']],
            source_path=text['source_path'], first_segment=first, segment_count=len(text['segments'])))
    ir['npcs'][0]['room_actor_index'] = 5
    for trigger in presentation['story_triggers']:
        existing = [t for t in ir['story_triggers'] if t['source_path'] == trigger['source_path']]
        require(len(existing) == 1, 'Phone area source identity')
        row = existing[0]
        require(row['center'] == trigger['center'] and row['extents'] == trigger['extents']
                and row['dialogue'] == 'Data/Dialogue/' + trigger['dialogue'] + '.yaml', 'Phone area geometry/source')
        conditions = ir['story_conditions'][row['first_condition']:row['first_condition'] + row['condition_count']]
        require([dict(flag=c['flag'], value=bool(c['value'])) for c in conditions] == trigger['conditions'],
                'Phone area flag conditions')
        row.update(disposition=2, program_index=paths[trigger['dialogue']])
    ir.update(schema=6, scope='Six same-scene house warps; original NPCs and openable doors; Lamp/Doll/melody plus six reviewed Carol/Phone programs; original Area3/4 contact gates; complete reviewed Dad-normal branch/choice/save-continuation graph and all leader texts')
    # The baseline extractor discovers door sounds through a set. Canonicalize
    # its provenance map so separate Python hash seeds produce identical files.
    ir['sources'] = dict(sorted(ex.sources.items()))
    return ir


# Existing AudioIR identities are an explicit accepted prefix. Hashes are always
# re-derived through the pinned source inventory, never copied from generated C++.
AUDIO_PREFIX = [
    (21, 'Audio/Music/Poltergeist.ogg', 'poltergeist'),
    (22, 'Audio/Sound effects/bash.mp3', 'bash'),
    (1001, 'Audio/Music/Battle Encounter/Encounter Enemy.mp3', 'encounter-enemy'),
    (1002, 'Audio/Music/Battle Encounter/Encounter Boss.mp3', 'encounter-boss'),
    (1101, 'Audio/Sound effects/Cursor 1.mp3', 'cursor-move'),
    (1102, 'Audio/Sound effects/Cursor 2.mp3', 'cursor-select'),
    (1103, 'Audio/Sound effects/M3/curshoriz.wav', 'cursor-back'),
    (1104, 'Audio/Sound effects/M3/bump.wav', 'cursor-restricted'),
    (30, 'Audio/Sound effects/M3/SMAAAASH.wav', 'doll-postwin-smash'),
    (32, 'Audio/Music/Melodies/melody1.mp3', 'doll-melody'),
    (33, 'Audio/Sound effects/M3/heal_se.wav', 'melody-remembered'),
    (34, 'Audio/Music/House.mp3', 'house-music'),
]


def build_audio(root, room):
    from tools.extract_battle_entry import Extractor
    ex = Extractor(root)
    ids = {room['strings'][r['path_string']]: r['stable_id'] for r in room['sections']['Resource']}
    rows = AUDIO_PREFIX + [(ids['res://' + RING_SOUND], RING_SOUND, 'phone-ring'),
        (ids['res://' + HANGUP_SOUND], HANGUP_SOUND, 'phone-hangup'),
        (1201, 'Audio/Sound effects/text/Adult.mp3', 'text-adult'),
        (1202, 'Audio/Music/Mother Earth.mp3', 'title-mother-earth')]
    assets = []
    for stable_id, path, name in rows:
        ex.data(path)
        ex.data(path + '.import')
        row = dict(stable_id=stable_id, source_path='res://' + path,
            source_sha256=ex.sources[path], import_sha256=ex.sources[path + '.import'],
            pcm_path='audio/' + name + '.pcm', gain_db=0)
        if stable_id == 1104:
            row['output_sample_rate'] = 48000
        assets.append(row)
    bus = 'Audio/Audio Buses/default_bus_layout.tres'
    manager = 'Scripts/global/audioManager.gd'
    ex.data(bus)
    ex.data(manager)
    return dict(schema=1, upstream_commit=PIN, bus_source=bus, bus_sha256=ex.sources[bus],
                manager_source=manager, manager_sha256=ex.sources[manager], assets=assets)


def report(root, room, house, audio):
    dialogue, presentation = load_stage(root)
    dad, choices = load_dad(root)
    audio_sources = {}
    for asset in audio['assets'][len(AUDIO_PREFIX):]:
        path = asset['source_path'].removeprefix('res://')
        audio_sources[path] = asset['source_sha256']
        audio_sources[path + '.import'] = asset['import_sha256']
    return dict(schema=1, commit=PIN, whole_handler_approved=False,
        inputs={p: sha(Path(root) / p) for p in [STAGE_DIALOGUE, STAGE_PRESENTATION,
            'content/phone-stage/native-parser.json', 'reports/doll-postwin/native-parser-animation.json',
            'tools/link_phone_content.py', 'tools/extract_native_content.py', 'tools/extract_house.py',
            'tools/dad_record_dialogue.py', 'tools/dialogue_choice_assets.py', 'tools/menu_audio_binding.py', DAD_CHOICES]},
        sources={**dialogue['sources'], **presentation['sources'], **dad['sources'], **choices['sources'], **audio_sources,
            **{p: room['provenance']['sources']['upstream/MOTHER-Encore/' + p] for p in
               ['Nodes/Reusables/flag landmarks.tscn', 'Scripts/Main/Flag Landmarks.gd', 'Scripts/global/globalData.gd']}},
        outputs={p: sha(Path(root) / p) for p in ['content/native-opening.json', 'content/native-house.json', 'content/native-audio.json']},
        stable_namespaces=dict(preserved_program_ids=list(range(1, 6)), preserved_resource_ids=list(range(1, 35)),
            preserved_actor_ids=list(range(1, 6)), preserved_dialogue_ids=list(range(1, 12)),
            new_programs={**{p['identity']: 6 + i for i, p in enumerate(dialogue['programs'])},
                          dad['program']['identity']: next(p['stable_id'] for p in room['sections']['Program'] if room['strings'][p['source_path_string']] == dad['program']['identity'])},
            new_texts={**text_ids(dialogue), **dad_text_ids(dialogue, dad)}, carol_actor_index=5, phone_object_index=0,
            entrance_flag_listener=next(b for b in room['sections']['Binding'] if b['kind'] == 9)),
        mechanisms=['Exact six reviewed source programs, source labels and command phase order',
            'Shared native 4dir frame tracks after exact YAML/grid/authored-offset comparison; Carol owns texture and height offsets',
            'Area3/4 retain exact inherited geometry and source contact/flag conditions',
            'Hint tags become typed color/reset tokens, no raw BBCode in displayed literals',
            'Original Phone object audio resources and deferred StartPhoneRing; no fabricated Actor/NPC',
            'DoorBlock inherits FlagLandmark delete_if_hidden=true: _ready and flags_updated check talked_to_dad; queue_free defers permanent descendant collider removal to the scene frame end',
            'Existing AudioIR IDs preserved; appended ring/hangup and Adult voice source',
            'Dad-normal all leader/earned/saved branches, source text and option targets; hidden Record save gate resumes only on callback'],
        dad_normal=dict(source=NORMAL, status='linked_source_graph', identity=dad['program']['identity'],
            label_to_pc=dad['program']['label_to_pc'], command_count=len(dad['program']['commands']),
            reviewed_noops=dad['program']['reviewed_noops'], choice_groups=dad['program']['choice_groups']),
        unverified=['Runtime gameplay execution', 'Integrated save persistence and restore', '3DS rendering and audio audibility'])


def write(path, document, check=False):
    path = Path(path)
    text = json.dumps(document, ensure_ascii=False, indent=2, allow_nan=False) + '\n'
    if check:
        require(path.read_text() == text, 'Phone extraction differs: ' + str(path))
    else:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    try:
        from tools.extract_native_content import Extractor
        from tools.extract_house import build as house_build, source_review
        room, source_map = Extractor(args.root).run()
        write(args.root / 'content/native-opening.json', room, args.check)
        write(args.root / 'content/native-source-map.json', source_map, args.check)
        house = house_build(args.root)
        write(args.root / 'content/native-house.json', house, args.check)
        write(args.root / 'reports/house-data/source-review.json', source_review(house, args.root / 'content/native-house.json'), args.check)
        audio = build_audio(args.root, room)
        write(args.root / 'content/native-audio.json', audio, args.check)
        write(args.root / REPORT, report(args.root, room, house, audio), args.check)
        print(('Verified' if args.check else 'Linked') + ' six phone programs, Carol actor, Area3/4/audio and Dad-normal branch/choice/save-continuation graph')
        return 0
    except (OSError, ValueError, KeyError, TypeError, ImportError) as error:
        print('PHONE LINK ERROR:', error, file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
