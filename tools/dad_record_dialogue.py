#!/usr/bin/env python3
"""Compile only the pinned Dad-normal graph to checked, program-relative commands.

No shipping Room/House pack is changed here. The caller binds flag/string/text
indices and verifies choice targets against this program's command extent.
"""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.phone_dialogue import NORMAL, build as phone_build, load_receipt, validate_document, write_json
from tools.doll_dialogue import PIN, require
from tools.doll_postwin import return_duration

REPORT = 'reports/dad-record/command-graph.json'
OPCODES = {'Jump': 36, 'BranchFlag': 37, 'BranchLeader': 38,
           'AwaitChoices': 39, 'OpenSave': 40, 'AwaitSubmenu': 41}
CHOICE_ID = 'Reusable/dad_normal::2'


def compile_graph(doc, end_duration):
    validate_document(NORMAL, doc)
    require(type(end_duration) in (int, float) and math.isfinite(end_duration) and end_duration > 0,
            'Dad-normal camera return duration')
    commands, labels, noops = [], {}, []
    phase_labels = list(doc)
    def emit(kind, label, **fields):
        commands.append(dict(kind=kind, source_label=label, phrase=phase_labels.index(label),
                             actor='None', **fields))
    emit('BeginCutscene', '0')
    for label, phrase in doc.items():
        labels[label] = len(commands)
        if phrase.get('text'):
            emit('ShowDialogue', label, dialogue_key=NORMAL + '::' + label, flags=1)
        if 'unsetflags' in phrase:
            flag = phrase['unsetflags']
            if flag == 'money_earned':
                # This one exact source spelling was reviewed against _init_flags
                # and set_flag. Never turn this into an unknown-flag ignore path.
                require(label == '2', 'Unknown Dad-normal no-op location')
                noops.append(dict(source_label=label, command='unsetflags', value=flag,
                                  before_pc=len(commands), mutation=False, signal=False))
            else:
                require(label == '4' and flag == 'saved', 'Unknown Dad-normal flag effect')
                # Empty text first shows the already visible box; showbox:false
                # immediately clears/hides it, disabling physics and input.
                emit('HideDialogue', label, flags=1)
                emit('SetFlag', label, flag=flag, value=0)
        if 'save' in phrase:
            require(label == '4' and phrase['save'] is True and phrase['text'] == ''
                    and not phrase['showbox'] and not phrase['caninput'], 'Unknown save suspension')
            emit('OpenSave', label)
            emit('AwaitSubmenu', label)
        elif 'options' in phrase:
            # This is a text-completion gate, not an ordinary accept gate. The
            # scheduler opens choices only after every source WAIT is consumed.
            emit('AwaitChoices', label, choice_group=CHOICE_ID)
        elif phrase.get('text'):
            emit('AwaitDialogue', label, flags=0)
        if 'if' in phrase:
            condition = phrase['if']
            if 'leader' in condition:
                emit('BranchLeader', label, leader=condition['leader'], target_label=condition['goto'])
            else:
                require(len(condition['flags']) == 1, 'Unknown Dad-normal flag branch')
                flag, value = next(iter(condition['flags'].items()))
                emit('BranchFlag', label, flag=flag, value=int(value), target_label=condition['goto'])
        if 'goto' in phrase:
            emit('Jump', label, target_label=phrase['goto'])
        elif 'options' not in phrase:
            emit('SetTalker', label)
            emit('CutsceneEnded', label)
            emit('DialogueDone', label, duration=end_duration)
    for command in commands:
        if 'target_label' in command:
            command['target_pc'] = labels[command['target_label']]
    options = doc['2']['options']
    choices = dict(id=CHOICE_ID, program_identity='Reusable/dad_normal', source_label='2',
                   program_command_count=len(commands), initial_selection=0,
                   options=[dict(translation_key=k, target_label=v, target_pc=labels[v])
                            for k, v in options.items() if k != 'cancel'],
                   cancel_target_label=options['cancel'], cancel_target_pc=labels[options['cancel']])
    result = dict(identity='Reusable/dad_normal', source_path=NORMAL, source_labels=phase_labels,
                  entry_pc=0, label_to_pc=labels, commands=commands, choice_groups=[choices],
                  reviewed_noops=noops, clears_phone_location_on_finish=True)
    validate_graph(result)
    return result


def validate_graph(graph):
    commands = graph['commands']
    labels = graph['label_to_pc']
    require(len(commands) > 0 and graph['entry_pc'] == 0, 'Dad-normal entry')
    require(len(set(labels.values())) == len(labels) and list(labels) == graph['source_labels'],
            'Dad-normal labels must map uniquely in source order')
    for label, pc in labels.items():
        require(type(pc) is int and 0 <= pc < len(commands) and commands[pc]['source_label'] == label,
                'Dad-normal label PC outside program: ' + label)
    legal = {'BeginCutscene', 'ShowDialogue', 'AwaitDialogue', 'HideDialogue', 'SetFlag',
             'SetTalker', 'CutsceneEnded', 'DialogueDone'} | set(OPCODES)
    for c in commands:
        require(c['kind'] in legal and c['source_label'] in labels, 'Unknown Dad-normal command')
        if c['kind'] in ('Jump', 'BranchFlag', 'BranchLeader'):
            require(c.get('target_label') in labels and c.get('target_pc') == labels[c['target_label']],
                    'Dad-normal branch target must be a source-label entry')
        if c['kind'] == 'BranchFlag':
            require(c.get('flag') in ('earned_cash', 'saved') and type(c.get('value')) is int
                    and c['value'] in (0, 1), 'Unknown Dad-normal flag test')
        if c['kind'] == 'BranchLeader':
            require(c.get('leader') in ('lloyd', 'ana', 'teddy', 'pippi'), 'Unknown Dad-normal leader')
    require(len(graph['choice_groups']) == 1, 'Dad-normal choice group count')
    group = graph['choice_groups'][0]
    require(group['program_command_count'] == len(commands) and len(group['options']) == 2
            and group['initial_selection'] == 0, 'Dad-normal choice extent/default')
    for row in group['options']:
        require(row['target_label'] in labels and row['target_pc'] == labels[row['target_label']],
                'Dad-normal choice target outside program')
    require(group['cancel_target_pc'] == labels[group['cancel_target_label']], 'Dad-normal cancel target')
    # All branches are forward. This prevents accidental source cycles or a
    # malformed content graph from turning resume() into an unbounded loop.
    for pc, c in enumerate(commands):
        if 'target_pc' in c:
            require(c['target_pc'] > pc, 'Dad-normal branch must progress')


def build(root=ROOT):
    root = Path(root)
    stage = json.loads((root / 'content/phone-stage/dialogue.json').read_text())
    require(stage == phone_build(root), 'Stale/unreviewed phone source stage')
    ex, docs = load_receipt(root)
    graph = compile_graph(docs[NORMAL], return_duration(ex.text('Scripts/UI/DialogueBox.gd')))
    texts = [copy.deepcopy(t) for t in stage['texts'] if t['source_path'] == NORMAL]
    kinds = {'Literal': 1, 'PlayerName': 2, 'HintStart': 3, 'HintEnd': 4,
             'EarnedCash': 5, 'BankCash': 6, 'CurrentCash': 7, 'SourceDelay': 8}
    for text in texts:
        for segment in text['segments']:
            segment['house_tokens'] = []
            for token in segment['tokens']:
                value = (stage['text_contracts']['hint_color_hex'] if token['kind'] == 'HintStart'
                         else str(token['parameter']) if token['kind'] == 'SourceDelay' else token.get('text', ''))
                if token['kind'] == 'Literal':
                    for i, part in enumerate(value.split('\n')):
                        if i: segment['house_tokens'].append(dict(kind=9, text=''))
                        if part: segment['house_tokens'].append(dict(kind=1, text=part))
                else:
                    segment['house_tokens'].append(dict(kind=kinds[token['kind']], text=value))
    source_graph = stage['dad_normal']
    for group in graph['choice_groups']:
        source = next(n for n in source_graph['nodes'] if n['label'] == group['source_label'])
        for row, original in zip(group['options'], source['choices']):
            require(row['translation_key'] == original['translation_key'] and row['target_label'] == original['target'],
                    'Choice source staging mismatch')
            row['text'] = original['text_en']
    return dict(schema=1, kind='encore.dad-record.graph-ir', commit=PIN, sources=ex.sources,
                program=graph, texts=texts, command_schema=OPCODES,
                contracts=dict(branch_pc='Program-relative command index; bind/validate before execution',
                    await_choices='Wait for final text completion, then menu result supplies the next PC',
                    await_submenu='Only SaveSelect close callback resumes; hidden empty text never finishes',
                    money='EarnedCash captures/resets once at phrase entry without flag broadcast; BankCash reads only',
                    source_delay='Token8 carries source numeric amount; CHAR_DELAY count=int(amount/(text_seconds*16)); suppress voice/RNG for delay chars',
                    source_newline='Token9 is an empty-payload forced newline with no printable cell; W@ wait then new bullet is a separate segment',
                    choice_clear='Clear text and hide choices before handling selected phrase; play InputSound after handling it'))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=['extract', 'verify'])
    p.add_argument('--root', type=Path, default=ROOT)
    args = p.parse_args()
    data = build(args.root)
    target = args.root / REPORT
    if args.action == 'extract':
        write_json(target, data)
    else:
        require(json.loads(target.read_text()) == data, 'Stale Dad-normal command graph')
    print('Dad-normal graph ' + args.action + ': ' + str(len(data['program']['commands'])) + ' commands')


if __name__ == '__main__':
    main()
