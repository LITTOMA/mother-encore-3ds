"""Strict CPU-state/opaque-UID protocol for the pinned dropped-item oracle."""
import copy
import hashlib
import json
import math
import re
import struct
from tools.extract_battle_entry import require
from tools.dungeon_keys import fields


def integer(value, low=0, high=0xffffffff):
    require(type(value) is int and low <= value <= high, 'Dropped reference integer')


def boolean(value):
    require(type(value) is bool, 'Dropped reference boolean')


def real(value):
    integer(value)
    result = struct.unpack('<f', struct.pack('<I', value))[0]
    require(math.isfinite(result) and abs(result) <= 1e8, 'Dropped reference binary32')
    return result


def vector(value):
    require(type(value) is list and len(value) == 2, 'Dropped reference vector')
    for number in value:
        real(number)


def validate(doc, config):
    fields(doc, 'schema godot cases entropy_allocations', 'Dropped reference')
    integer(doc['schema'], 2, 2)
    engine = doc['godot']
    fields(engine, 'major minor patch hex status build year hash string', 'Dropped reference engine')
    require(type(engine) is dict and engine.get('hash') == '3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'
            and engine.get('major') == 3 and engine.get('minor') == 6 and engine.get('patch') == 2,
            'Dropped reference original engine')
    require(type(doc['cases']) is list and type(doc['entropy_allocations']) is list
            and len(doc['cases']) == len(doc['entropy_allocations']) == len(config['cases']),
            'Dropped reference case coverage')
    names = {v['source'] for v in config['templates'].values()} | {'DogTreats'}
    total = 0
    for case, entropy, test in zip(doc['cases'], doc['entropy_allocations'], config['cases']):
        fields(case, 'case sparkle_rng_tail rows', 'Dropped reference case')
        fields(entropy, 'case uids', 'Dropped reference entropy')
        require(case['case'] == entropy['case'] == test['name'], 'Dropped reference case identity')
        integer(case['sparkle_rng_tail'])
        require(type(entropy['uids']) is list and len(entropy['uids']) <= 16,
                'Dropped reference generated UID count')
        for uid in entropy['uids']:
            integer(uid)
        require(len(set(entropy['uids'])) == len(entropy['uids']), 'Dropped reference duplicate generated UID')
        require(type(case['rows']) is list and len(case['rows']) == test['frames'] + 2,
                'Dropped reference clock coverage')
        previous_ledger = previous_idle = previous_physics = 0
        flag = config['bindings'][test['binding']]['flag']
        provided = {'provided:' + str(1000 + j * 100 + i)
                    for j, count in enumerate(test['fill']) for i in range(count)}

        def item(value, ledger):
            fields(value, 'name doses equipped uid', 'Dropped reference item')
            require(value['name'] in names, 'Dropped reference unknown source item')
            integer(value['doses'], 1, 0xffff)
            boolean(value['equipped'])
            uid = value['uid']
            require(type(uid) is str, 'Dropped reference opaque UID type')
            if uid not in provided:
                require(re.fullmatch(r'allocated:(0|[1-9][0-9]*)', uid) is not None
                        and int(uid.split(':')[1]) < ledger, 'Dropped reference UID ledger ownership')

        for index, row in enumerate(case['rows']):
            common = 'frame idle physics delta alive flags dialogues ledger_size party keys global_item prompt'
            require(type(row) is dict and type(row.get('alive')) is bool, 'Dropped reference lifetime')
            fields(row, common + (' state' if row['alive'] else ''), 'Dropped reference row')
            integer(row['frame'], -1, test['frames'])
            require(row['frame'] == index - 1, 'Dropped reference frame order')
            integer(row['idle'], previous_idle, 10000)
            integer(row['physics'], previous_physics, 10000)
            previous_idle, previous_physics = row['idle'], row['physics']
            require(0 <= real(row['delta']) <= 1, 'Dropped reference process delta')
            ledger = row['ledger_size']
            integer(ledger, previous_ledger, len(entropy['uids']))
            previous_ledger = ledger
            require(type(row['flags']) is dict and set(row['flags']) <= {flag},
                    'Dropped reference foreign flag')
            for value in row['flags'].values():
                boolean(value)
            require(type(row['dialogues']) is list and len(row['dialogues']) <= 16
                    and all(v in ['ItemDialogue/itemcheck', 'ItemDialogue/itemfull',
                                  'ItemDialogue/presentempty'] for v in row['dialogues']),
                    'Dropped reference unknown dialogue')
            require(type(row['party']) is list and len(row['party']) == len(test['fill'])
                    and type(row['keys']) is list and len(row['keys']) <= 16,
                    'Dropped reference inventory holders')
            for inventory in [*row['party'], row['keys']]:
                require(type(inventory) is list and len(inventory) <= 16, 'Dropped reference inventory capacity')
                uids = set()
                for value in inventory:
                    item(value, ledger)
                    require(value['uid'] not in uids, 'Dropped reference duplicate owned UID')
                    uids.add(value['uid'])
            if row['global_item'] is not None:
                item(row['global_item'], ledger)
            if row['prompt'] is not None:
                p = row['prompt']
                fields(p, 'parent visible hidden pressing position scale enabled force_show force_hide animation time queued',
                       'Dropped reference detached prompt')
                require(p['parent'] in ['Objects', config['bindings'][test['binding']]['path'] + '/interact'],
                        'Dropped reference prompt owner')
                for k in ['visible', 'hidden', 'pressing', 'enabled', 'force_show', 'force_hide', 'queued']:
                    boolean(p[k])
                for k in ['position', 'scale']:
                    vector(p[k])
                require(p['animation'] in ['', 'Show', 'Hide', 'Press', 'Float'], 'Dropped reference prompt animation')
                if p['time'] is not None:
                    real(p['time'])
            if row['alive']:
                state = row['state']
                fields(state, 'position scale visible sprite_visible rotation sparkle timer_paused timer_left timer_stopped animation animation_speed animation_time queued prompt',
                       'Dropped reference item pose')
                for k in ['position', 'scale']:
                    vector(state[k])
                for k in ['visible', 'sprite_visible', 'timer_paused', 'timer_stopped', 'queued']:
                    boolean(state[k])
                for k in ['rotation', 'timer_left', 'animation_speed']:
                    real(state[k])
                integer(state['sparkle'], 0, 39)
                require(state['animation'] in ['', 'blink'], 'Dropped reference blink animation')
                require((state['animation_time'] is None) == (state['animation'] == ''),
                        'Dropped reference absent animation time')
                if state['animation_time'] is not None:
                    real(state['animation_time'])
                if state['prompt'] is not None:
                    p = state['prompt']
                    fields(p, 'visible hidden pressing position scale', 'Dropped reference attached prompt')
                    for k in ['visible', 'hidden', 'pressing']:
                        boolean(p[k])
                    vector(p['position'])
                    vector(p['scale'])
            total += 1
        require(previous_ledger == len(entropy['uids']), 'Dropped reference unused raw UID allocation')
    require(0 < total <= 100000, 'Dropped reference observation budget')
    return doc


def semantic_digest(doc):
    # This is the sole normalization: the retained raw OS-random UIDs are opaque
    # identities. Every state, clock, ordered ledger token and outcome is kept.
    canonical = copy.deepcopy(doc)
    canonical['entropy_allocations'] = [dict(case=v['case'], count=len(v['uids']))
                                      for v in doc['entropy_allocations']]
    return hashlib.sha256(json.dumps(canonical, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
