#!/usr/bin/env python3
"""Fail-closed, source-ordered Carol/house-phone content frontend.

This exports a staging IR. It does not change the accepted shipping packs, run
gameplay, or interpret arbitrary YAML/GDScript. Dad-normal is a checked graph for
the later Record integration, not an executable program in this milestone.
"""
from __future__ import annotations

import argparse
import csv
import io
import json
import math
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.doll_dialogue import PIN, decode, sha, require
from tools.doll_postwin import return_duration
from tools.extract_battle_entry import Extractor, node

DIRECT = 'Data/Dialogue/Podunk/carol_call.yaml'
AREA = 'Data/Dialogue/Podunk/cutscenes/carol_call.yaml'
REMINDER = 'Data/Dialogue/Podunk/cutscenes/carol_phone.yaml'
CAROL_PHONE = 'Data/Dialogue/Podunk/carol_phone.yaml'
FIRST_DAD = 'Data/Dialogue/Podunk/dad_poltergeist.yaml'
NO_ANSWER = 'Data/Dialogue/Reusable/phonenoanswer.yaml'
NORMAL = 'Data/Dialogue/Reusable/dad_normal.yaml'
YAMLS = [DIRECT, AREA, REMINDER, CAROL_PHONE, FIRST_DAD, NO_ANSWER, NORMAL]
TABLES = ['Translations/TranslatedText/' + x + ' - sheet.csv' for x in
          ['dialogue_Podunk', 'dialogue_Podunk_cutscenes', 'dialogue_Reusable']]
SOURCES = [*YAMLS, *TABLES, 'Scripts/global/yaml_parser.gd',
           'Scripts/UI/DialogueBox.gd', 'Scripts/UI/AbstractDialogueBox.gd',
           'Scripts/Main/actor.gd', 'Scripts/Main/npc.gd',
           'Scripts/Main/Interact Dialog.gd', 'Scripts/Main/party/Player.gd',
           'Scripts/Main/party/party_object.gd', 'Scripts/Main/CutsceneArea.gd',
           'Scripts/Main/Flag Landmarks.gd', 'Scripts/Main/Camera2D.gd',
           'Scripts/global/text_tools.gd', 'Scripts/global/globalData.gd',
           'Scripts/global/global.gd', 'Scripts/global/uiManager.gd',
           'Scripts/global/Inventory.gd', 'Scripts/UI/SaveSelect.gd',
           'Maps/Testing/phone.gd', 'Nodes/Reusables/phone.tscn',
           'Nodes/Reusables/CutsceneArea.tscn', 'Nodes/Reusables/actor.tscn',
           'Nodes/Reusables/npc.tscn', 'Nodes/Ui/DialogueBox.tscn',
           'Maps/podunk/Nintens House.tscn', 'Data/save_new_game.yaml',
           'Audio/Sound effects/text/Female.mp3', 'Audio/Sound effects/text/Adult.mp3']
OUT = ROOT / 'content/phone-stage'
RECEIPT = 'content/phone-stage/native-parser.json'
REVIEW = 'compatibility/reviews/phone-dialogue.json'


def write_json(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + '\n')


def expected_documents():
    """Exact reviewed phrase shapes; values come from the pinned YAML files."""
    def line(prefix, label, speaker='Carol', voice='Female', **fields):
        return dict(name=prefix + '_SPEAKER_' + speaker, sound=voice,
                    text=prefix + '_' + label, **fields)
    def carol(area):
        prefix = 'DIALOGUE_PODUNK_' + ('CUTSCENES_' if area else '') + 'CAROL_CALL'
        first = line(prefix, '0', actors={'ninten': 'leader', 'carol': 'Objects/npc'} if area
                     else {'carol': 'Objects/npc'}, talker='carol', goto='1' if area else '3')
        if area:
            first.update(actorsdir={'carol': {'x': 0, 'y': 1}}, returncam=.1)
        doc = {'0': first}
        if area:
            doc['1'] = line(prefix, '1', actorsmove={'ninten': {
                'movement': [{'x': 0, 'y': -32}, {'wait': .1}], 'speed': 64,
                'animation': 'Walk', 'type': 'step'}},
                actorsturn={'ninten': {'speed': .1, 'actor': 'carol', 'queue': True}}, goto='3')
        doc.update({'3': line(prefix, '3', autoadvance=True, goto='4'),
                    '4': dict(name='Carol', wait=.3, autoadvance=True, caninput=False, goto='5'),
                    '5': dict(name='Carol', actorsturn={'carol': {'x': -1, 'y': 0, 'speed': .05}},
                              objectsfunction={'Objects/Phone': '_ring'}, autowait=1, goto='6'),
                    '6': line(prefix, '6', goto='7'),
                    '7': line(prefix, '7', setflags='phone_ring')})
        return doc
    reminder = {'0': line('DIALOGUE_PODUNK_CUTSCENES_CAROL_PHONE', '0',
                         actors={'ninten': 'leader', 'carol': 'Objects/npc'}, talker='carol',
                         actorsdir={'carol': {'x': 0, 'y': 1}},
                         actorsmove={'ninten': {'movement': [{'wait': .1}, {'x': 0, 'y': -16}],
                                               'speed': 64, 'animation': 'Walk', 'type': 'step'}},
                         returncam=.1)}
    first_dad = {'0': dict(text='DIALOGUE_PODUNK_DAD_POLTERGEIST_0', goto='1')}
    prefix = 'DIALOGUE_PODUNK_DAD_POLTERGEIST'
    first_dad['1'] = line(prefix, '1', 'Dad', 'Adult', goto='7')
    first_dad['7'] = line(prefix, '7', 'Dad', 'Adult', movecam={'x': 96, 'length': 1.0}, goto='9')
    first_dad['9'] = line(prefix, '9', 'Dad', 'Adult', returncam=1.0, goto='13')
    first_dad['13'] = dict(text=prefix + '_13', setflags='talked_to_dad')
    normal_prefix = 'DIALOGUE_REUSABLE_DAD_NORMAL'
    normal = {'0': line(normal_prefix, '0', 'Dad', 'Adult', goto='check_lloyd')}
    leaders = ['lloyd', 'ana', 'teddy', 'pippi']
    for index, leader in enumerate(leaders):
        normal['check_' + leader] = dict(name=normal_prefix + '_SPEAKER_Dad',
            **{'if': {'leader': leader, 'goto': leader + '_leader'}},
            goto='check_' + leaders[index + 1] if index < 3 else 'check_earned_cash')
    for leader in leaders:
        normal[leader + '_leader'] = dict(name=normal_prefix + '_SPEAKER_Dad', sound='Adult',
            text='DIALOGUE_REUSABLE_DAD_' + leader.upper() + 'LEADER_0', goto='check_earned_cash')
    normal['check_earned_cash'] = dict(name=normal_prefix + '_SPEAKER_Dad',
        **{'if': {'flags': {'earned_cash': True}, 'goto': '1'}}, goto='2')
    normal['1'] = line(normal_prefix, '1', 'Dad', 'Adult', goto='2')
    normal['2'] = line(normal_prefix, '2', 'Dad', 'Adult', options={
        normal_prefix + '_3-OPT_0': '4', normal_prefix + '_3-OPT_1': '6', 'cancel': '6'},
        unsetflags='money_earned')
    normal['4'] = dict(text='', showbox=False, unsetflags='saved', caninput=False,
        **{'if': {'flags': {'saved': True}, 'goto': '5'}}, goto='6', save=True)
    for label, target in [('5', '6'), ('6', '7'), ('7', None)]:
        normal[label] = line(normal_prefix, label, 'Dad', 'Adult', **({'goto': target} if target else {}))
    return {DIRECT: carol(False), AREA: carol(True), REMINDER: reminder,
            CAROL_PHONE: {'0': line('DIALOGUE_PODUNK_CAROL_PHONE', '0')},
            FIRST_DAD: first_dad, NO_ANSWER: {'0': {'text': 'DIALOGUE_REUSABLE_PHONENOANSWER_0'}},
            NORMAL: normal}


def validate_document(path, doc):
    expected = expected_documents()
    require(path in expected, 'Unreviewed phone dialogue: ' + path)
    require(isinstance(doc, dict) and list(doc) == list(expected[path]), 'Phone phrase labels/order: ' + path)
    for label, phrase in doc.items():
        require(typed_equal(phrase, expected[path][label]), 'Unreviewed phone phrase: ' + path + ':' + label)
        if 'actors' in phrase:
            require(list(phrase['actors']) == list(expected[path][label]['actors']), 'Phone actor binding order: ' + path)
        if 'options' in phrase:
            require(list(phrase['options']) == list(expected[path][label]['options']), 'Phone option order: ' + path)


def typed_equal(actual, expected):
    if isinstance(expected, bool):
        return type(actual) is bool and actual == expected
    if isinstance(expected, (int, float)):
        return type(actual) in (int, float) and math.isfinite(actual) and actual == expected
    if isinstance(expected, dict):
        return isinstance(actual, dict) and set(actual) == set(expected) and all(
            typed_equal(actual[k], v) for k, v in expected.items())
    if isinstance(expected, list):
        return isinstance(actual, list) and len(actual) == len(expected) and all(
            typed_equal(a, e) for a, e in zip(actual, expected))
    return type(actual) is type(expected) and actual == expected


def run_native(root, godot):
    ex = Extractor(root)
    for path in SOURCES:
        ex.data(path)
    with tempfile.TemporaryDirectory(prefix='encore-phone-native-') as td:
        work = Path(td)
        (work / 'project.godot').write_text('config_version=4\n[application]\nconfig/name="Phone source parser"\n[logging]\nfile_logging/enable_logging=false\n')
        (work / 'yaml_parser.gd').write_bytes(ex.data('Scripts/global/yaml_parser.gd'))
        for index, path in enumerate(YAMLS):
            (work / (str(index) + '.yaml')).write_bytes(ex.data(path))
        (work / 'probe.gd').write_text('''extends SceneTree
func precise(value):
    if typeof(value)==TYPE_REAL:
        var bits=StreamPeerBuffer.new()
        bits.put_double(value)
        return {"native_f64_decimal":"%.17f"%value,"native_f64_le_hex":bits.data_array.hex_encode()}
    if value is Array:
        var out=[]
        for x in value:out.append(precise(x))
        return out
    if value is Dictionary:
        var out={}
        for k in value:out[k]=precise(value[k])
        return out
    return value
func _init():
    var parser=load("res://yaml_parser.gd")
    var yaml=[]
    for i in range(7):yaml.append(parser.parse_file("res://%d.yaml"%i))
    var file=File.new()
    file.open("res://result.json",File.WRITE)
    file.store_string(JSON.print(precise({"godot":Engine.get_version_info(),"yaml":yaml})))
    file.close()
    quit()
''')
        env = dict(os.environ, XDG_DATA_HOME=str(work / 'data'), XDG_CONFIG_HOME=str(work / 'config'),
                   XDG_CACHE_HOME=str(work / 'cache'))
        result = subprocess.run([str(godot), '--path', str(work), '--script', 'probe.gd'],
                                capture_output=True, text=True, timeout=60, env=env)
        require(result.returncode == 0 and 'SCRIPT ERROR' not in result.stdout + result.stderr
                and 'ERROR:' not in result.stdout + result.stderr, 'Phone original parser failed: ' + result.stdout + result.stderr)
        data = json.loads((work / 'result.json').read_text())
        data.update(schema=1, commit=PIN, sources=ex.sources, log=result.stdout + result.stderr)
        return data


def load_receipt(root=ROOT):
    root = Path(root)
    ex = Extractor(root)
    review = json.loads((root / REVIEW).read_text())
    data = json.loads((root / RECEIPT).read_text())
    require(set(data) == {'schema', 'commit', 'sources', 'log', 'godot', 'yaml'} and
            set(review) == {'schema', 'commit', 'whole_handler_approved', 'sources', 'native_receipt_sha256',
                            'scope', 'source_phase_labels', 'handler_order', 'constraints', 'unverified'},
            'Unknown phone receipt/review fields')
    require(review['schema'] == data['schema'] == 1 and review['commit'] == data['commit'] == PIN
            and review['whole_handler_approved'] is False, 'Unreviewed phone source scope')
    require(sha(root / RECEIPT) == review['native_receipt_sha256'] and data['sources'] == review['sources']
            and set(data['sources']) == set(SOURCES), 'Changed phone receipt or dependencies')
    require(all(data['godot'].get(k) == v for k, v in {'major': 3, 'minor': 6, 'patch': 2,
        'status': 'stable', 'build': 'official', 'hash': '3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'}.items()),
        'Phone parser version')
    for path, digest in data['sources'].items():
        require(sha(ex.upstream / path) == digest, 'Changed phone source: ' + path)
        ex.data(path)
    parsed = decode(data)['yaml']
    require(len(parsed) == len(YAMLS), 'Phone parser document count')
    for path, doc in zip(YAMLS, parsed):
        require(ex.yaml(path) == doc, 'Phone native/Python parser mismatch: ' + path)
        validate_document(path, doc)
    return ex, dict(zip(YAMLS, parsed))


def compile_program(path, doc, end_duration):
    validate_document(path, doc)
    require(path != NORMAL, 'Dad-normal graph is staged, not a linear program')
    require(type(end_duration) in (int, float) and math.isfinite(end_duration) and end_duration > 0,
            'Phone camera return duration')
    commands = []
    labels = list(doc)
    bound = list(doc['0'].get('actors', {}))
    talker = 'Carol' if path in (DIRECT, AREA, REMINDER) else 'None'
    def emit(kind, phase=0, actor='None', **fields):
        commands.append(dict(kind=kind, phrase=phase, source_label=labels[phase], actor=actor, **fields))
    emit('BeginCutscene')
    for phase, (label, p) in enumerate(doc.items()):
        if 'text' in p:
            emit('ShowDialogue', phase, talker, dialogue_key=path + '::' + label,
                 flags=1 if talker == 'None' else 0)
        elif 'wait' in p or 'autowait' in p:
            emit('HideDialogue', phase, flags=1)
        if 'actors' in p:
            for actor in p['actors']:
                emit('BindActor', phase, actor.title())
                emit('ActorPersistent', phase, actor.title())
            emit('YieldIdle', phase)
        if 'wait' in p or 'autowait' in p:
            emit('StartWait', phase, duration=p.get('wait', p.get('autowait')))
        if 'objectsfunction' in p:
            emit('CallObjectDeferred', phase, binding='phone_ring', source_object='Objects/Phone', source_method='_ring')
        if 'talker' in p:
            emit('SetTalker', phase, 'Carol')
        for actor, direction in p.get('actorsdir', {}).items():
            emit('SetActorDirection', phase, actor.title(), vector=[direction['x'], direction['y']])
        for actor, movement in p.get('actorsmove', {}).items():
            emit('MoveActorPath', phase, actor.title(), path=movement)
        for actor, turn in p.get('actorsturn', {}).items():
            if 'actor' in turn:
                emit('TurnActor', phase, actor.title(), target_actor=turn['actor'].title(),
                     vector=[0, 0], duration=turn['speed'], flags=3)
            else:
                emit('TurnActor', phase, actor.title(), vector=[turn['x'], turn['y']], duration=turn['speed'], flags=0)
        if 'movecam' in p:
            emit('MoveCamera', phase, vector=[p['movecam']['x'], 0], flags=1,
                 duration=p['movecam']['length'])
        if 'returncam' in p:
            emit('ReturnCamera', phase, duration=p['returncam'])
        if 'setflags' in p:
            emit('SetFlag', phase, flag=p['setflags'], value=1)
        if 'text' in p:
            emit('AwaitDialogue', phase, flags=(2 if p.get('autoadvance', False) else 0)
                 | (4 if not p.get('caninput', True) else 0))
        else:
            emit('AwaitTimer', phase)
    last = len(labels) - 1
    # Dad and no-answer have no talker; no fabricated Phone Actor is stopped or restored.
    if talker != 'None':
        emit('StopInteraction', last, talker)
    elif path == CAROL_PHONE:
        emit('StopInteraction', last, flags=1)
    emit('SetTalker', last)
    for actor in bound:
        emit('RestoreActor', last, actor.title())
    emit('CutsceneEnded', last)
    emit('DialogueDone', last, duration=end_duration)
    return dict(identity=path.removeprefix('Data/Dialogue/').removesuffix('.yaml'), source_path=path,
                source_labels=labels, actor_bindings=[dict(actor=k.title(), source=v) for k, v in doc['0'].get('actors', {}).items()],
                commands=commands, clears_phone_location_on_finish=True)


def text_segments(raw, allow_money=False, allow_delay=False):
    require(raw.startswith('[@]'), 'Phone text lacks source bullet')
    out = []
    color = False
    # Aliased W@ appears only in the retained alternate-leader graph metadata.
    for part in re.split(r'\[(?:WAIT@|W@)\]', raw[3:]):
        tokens = []
        for piece in re.split(r'(\[[^\]]*\])', part):
            if not piece:
                continue
            if piece == '[Ninten]':
                tokens.append(dict(kind='PlayerName'))
            elif piece == '[color]':
                require(not color, 'Nested phone hint color')
                color = True
                tokens.append(dict(kind='HintStart'))
            elif piece == '[/color]':
                require(color, 'Unmatched phone hint color end')
                color = False
                tokens.append(dict(kind='HintEnd'))
            elif allow_money and piece in ('[EarnedCash]', '[BankCash]'):
                tokens.append(dict(kind=piece[1:-1]))
            elif allow_delay and re.fullmatch(r'\[D:[45]\]', piece):
                tokens.append(dict(kind='SourceDelay', parameter=int(piece[3:-1])))
            else:
                require('[' not in piece and ']' not in piece, 'Unknown phone text token: ' + piece)
                tokens.append(dict(kind='Literal', text=piece))
        require(not color, 'Phone hint spans cross a source segment')
        out.append(dict(bullet=True, tokens=tokens))
    for index, segment in enumerate(out):
        segment['wait_for_input'] = index < len(out) - 1
    return out


def extract_texts(ex, documents):
    translations = {}
    for path in TABLES:
        for row in csv.DictReader(io.StringIO(ex.text(path))):
            if row['key'] in translations:
                require(translations[row['key']] == row['en'], 'Conflicting phone translation key')
            translations[row['key']] = row['en']
    texts = []
    for path, doc in documents.items():
        for label, phrase in doc.items():
            if not phrase.get('text'):
                continue
            key = phrase['text']
            raw = translations[key]
            texts.append(dict(identity=path + '::' + label, source_path=path, source_label=label,
                translation_key=key, text_en=raw, speaker_key=phrase.get('name', ''),
                speaker_en=translations[phrase['name']] if 'name' in phrase else '',
                voice='Audio/Sound effects/text/' + phrase['sound'] + '.mp3' if 'sound' in phrase else '',
                segments=text_segments(raw, allow_money=path == NORMAL, allow_delay=path == NORMAL)))
    return texts, translations


def compile_normal_graph(doc, translations):
    validate_document(NORMAL, doc)
    nodes = []
    for label, p in doc.items():
        n = dict(label=label, text_key=NORMAL + '::' + label if p.get('text') else None,
                 fallback=p.get('goto'), terminal=not any(k in p for k in ('goto', 'options', 'if')),
                 textless_immediate='text' not in p)
        if 'if' in p:
            condition = p['if']
            if 'leader' in condition:
                n['branch'] = dict(kind='LeaderEquals', value=condition['leader'], target=condition['goto'])
            else:
                flag, value = next(iter(condition['flags'].items()))
                n['branch'] = dict(kind='FlagEquals', flag=flag, value=value, target=condition['goto'])
            n['evaluate_branch'] = 'submenu_callback' if label == '4' else 'phrase_advance'
        if 'options' in p:
            n['choices'] = [dict(translation_key=k, text_en=translations[k], target=v)
                            for k, v in p['options'].items() if k != 'cancel']
            n.update(cancel_target=p['options']['cancel'], initial_selection=0, show_choices='after_text_complete')
        if label == '1':
            n['text_replacement_effects'] = [dict(token='EarnedCash', capture='earned_cash', reset_amount=0,
                clear_flag='earned_cash', flags_updated=False, phase='before_print', frequency='once_per_phrase_entry')]
        if label == '2':
            n['text_replacement_effects'] = [dict(token='BankCash', read='bank', mutate=False, phase='before_print')]
            n['reviewed_noop'] = dict(source_command='unsetflags', flag='money_earned',
                reason='Unregistered exact upstream spelling; globalData.set_flag returns without mutation or signal',
                flags_updated=False, source='Scripts/global/globalData.gd:409')
        if label == '4':
            n['entry'] = [dict(kind='ShowEmptyText'), dict(kind='HideDialogue'),
                          dict(kind='SetFlag', flag='saved', value=False),
                          dict(kind='OpenSave', mode='SAVE', callback='_try_resume_dialogue')]
            n.update(can_input=False, suspend_until='submenu_callback', hidden_text_does_not_finish=True)
        nodes.append(n)
    return dict(identity='Reusable/dad_normal', source_path=NORMAL, execution_status='not_integrated',
                entry='0', nodes=nodes, clears_phone_location_on_finish=True,
                singleton_specialization=dict(required_party=['ninten'], skipped_conditions=[],
                    policy='Retain typed leader checks; any future specialization must assert singleton Ninten'))


def build(root=ROOT):
    ex, documents = load_receipt(root)
    texts, translations = extract_texts(ex, documents)
    programs = [compile_program(path, documents[path], return_duration(ex.text('Scripts/UI/DialogueBox.gd')))
                for path in YAMLS if path != NORMAL]
    normal = compile_normal_graph(documents[NORMAL], translations)
    hint_match = re.search(r'^const DIALOG_HINT_COLOR := "([0-9a-f]{6})"',
                           ex.text('Scripts/global/text_tools.gd'), re.M)
    require(hint_match is not None, 'Phone source hint color missing')
    data_script = ex.text('Scripts/global/globalData.gd')
    flag_body = data_script[data_script.index('func _init_flags():'):data_script.index('func ', data_script.index('func _init_flags():') + 5)]
    flags = re.findall(r'"([a-z0-9_]+)"', flag_body)
    require('earned_cash' in flags and 'saved' in flags and 'money_earned' not in flags,
            'Dad-normal reviewed unknown-flag no-op changed')
    return dict(schema=1, kind='encore.phone-stage.dialogue-ir', commit=PIN,
                shipping_integrated=False, sources=ex.sources, programs=programs, texts=texts, dad_normal=normal,
                text_contracts=dict(hint_color_hex=hint_match[1],
                    money_replacement='Once before phrase printing/reflow; retain substituted result',
                    player_name='Source Ninten nickname with existing source name-length policy'),
                contracts=dict(phone_is_actor=False, phone_marks_npc_seen=False,
                    phone_card_required=False, money_required=False, basement_required=False,
                    pillow_required=False, grants_rewards=False, first_call_opens_save=False,
                    flag_timing='source command phase before final line finishes',
                    restoration='Only bound original actors; update_npcs writes position/direction then restores asynchronously',
                    queued_turn_target_resolution='Compute direction at command dispatch, before queued wait',
                    camera_axis_flags='1 replaces absolute X retaining current camera global Y; sine/out source defaults'))


def verify(root=ROOT):
    root = Path(root)
    current = json.loads((root / 'content/phone-stage/dialogue.json').read_text())
    require(current == build(root), 'Stale phone dialogue staging IR')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('action', choices=['record', 'extract', 'verify'])
    ap.add_argument('--godot', type=Path)
    ap.add_argument('--root', type=Path, default=ROOT)
    args = ap.parse_args()
    try:
        if args.action == 'record':
            require(args.godot is not None, 'Recording requires official Godot')
            data = run_native(args.root, args.godot)
            for path, doc in zip(YAMLS, decode(data)['yaml']):
                validate_document(path, doc)
            write_json(args.root / RECEIPT, data)
            write_json(args.root / REVIEW, dict(schema=1, commit=PIN, whole_handler_approved=False,
                sources=data['sources'], native_receipt_sha256=sha(args.root / RECEIPT),
                scope='Exact seven phone YAML documents; six bounded linear programs and retained Dad-normal graph',
                source_phase_labels={p: list(d) for p, d in expected_documents().items()},
                handler_order=['text or hide box', 'actor-ready/persistent/idle', 'timer', 'deferred object calls',
                    'talker', 'directions', 'move queues', 'enqueue-time actor turn', 'camera', 'flags',
                    'dialogue/timer gate', 'stop talker', 'ordered actor restoration', 'cutscene ended/done/camera return'],
                constraints=['Phone remains InteractDialog and never becomes NPC/Actor',
                    'No item, cash, basement, Pillow, or Minnie prerequisite',
                    'No first-call save/reward; retain phone_ring after first Dad',
                    'All source locations/labels/text/flags/timings remain content data',
                    'Dad-normal alternate-leader text is metadata, not evidence of alternate-party runtime support'],
                unverified=['Native runtime integration', 'Full Godot game', 'GUI', '3DS gameplay', 'Audio audibility']))
        if args.action in ('record', 'extract'):
            write_json(args.root / 'content/phone-stage/dialogue.json', build(args.root))
        else:
            verify(args.root)
        print('Phone dialogue ' + args.action + ' complete')
        return 0
    except (ValueError, OSError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print('PHONE DIALOGUE ERROR:', error, file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
