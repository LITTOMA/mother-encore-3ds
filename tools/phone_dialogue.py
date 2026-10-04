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
from tools.programme_lowering_recipe import operation

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


def expected_documents(root=ROOT):
    from tools import programme_lowering_recipe as recipe
    return recipe.phone_documents(root)


def validate_document(path,doc,root=ROOT):
    from tools import programme_lowering_recipe as recipe
    value=recipe.load(root)
    require(path in value['phone_sources'],'Unreviewed phone dialogue: '+path)
    recipe.document(value,path,doc)


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


@operation
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
        validate_document(path, doc, root)
    return ex, dict(zip(YAMLS, parsed))


def compile_program(path,doc,end_duration,root=ROOT):
    from tools import programme_lowering_recipe as recipe
    return recipe.for_source(path,doc,end_duration,root)


def text_segments(raw, allow_money=False, allow_delay=False, player_token=None):
    if player_token is None:
        from tools import programme_lowering_recipe as recipe
        player_token = recipe.load(ROOT)['identities']['phone/player-token']['value']
    require(raw.startswith('[@]'), 'Phone text lacks source bullet')
    out = []
    color = False
    # Aliased W@ appears only in the retained alternate-leader graph metadata.
    for part in re.split(r'\[(?:WAIT@|W@)\]', raw[3:]):
        tokens = []
        for piece in re.split(r'(\[[^\]]*\])', part):
            if not piece:
                continue
            if piece == player_token:
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
    from tools import programme_lowering_recipe as recipe
    bindings = recipe.load(ex.root)
    voice_prefix = bindings['facts']['voice-root']['value'].removeprefix('res://')
    voice_extension = bindings['facts']['voice-extension']['value']
    player_token = bindings['identities']['phone/player-token']['value']
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
                voice=voice_prefix + phrase['sound'] + voice_extension if 'sound' in phrase else '',
                segments=text_segments(raw, allow_money=path == NORMAL, allow_delay=path == NORMAL, player_token=player_token)))
    return texts, translations


def compile_normal_graph(doc,translations,root=ROOT):
    from tools import programme_lowering_recipe as recipe
    return recipe.normal_metadata(doc,translations,root)


@operation
def build(root=ROOT):
    ex, documents = load_receipt(root)
    texts, translations = extract_texts(ex, documents)
    programs = [compile_program(path, documents[path], return_duration(ex.text('Scripts/UI/DialogueBox.gd')), root)
                for path in YAMLS if path != NORMAL]
    normal = compile_normal_graph(documents[NORMAL], translations, root)
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
                validate_document(path, doc, args.root)
            write_json(args.root / RECEIPT, data)
            write_json(args.root / REVIEW, dict(schema=1, commit=PIN, whole_handler_approved=False,
                sources=data['sources'], native_receipt_sha256=sha(args.root / RECEIPT),
                scope='Exact seven phone YAML documents; six bounded linear programs and retained Dad-normal graph',
                source_phase_labels={p: list(d) for p, d in expected_documents(args.root).items()},
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
