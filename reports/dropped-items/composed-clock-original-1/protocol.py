"""Exact composed coffin/item CPU protocol; raw OS UID entropy stays retained."""
import copy
from tools import dropped_item_protocol as item
from tools.dungeon_keys import fields
from tools.extract_battle_entry import require
semantic_digest = item.semantic_digest


def legacy_projection(doc):
    projected = copy.deepcopy(doc)
    projected['schema'] = 3
    for case in projected['cases']:
        case.pop('initial_idle_deltas')
    return projected


def validate(doc, config):
    fields(doc, 'schema godot cases entropy_allocations', 'Dropped composition')
    require(type(doc) is dict and type(doc.get('schema')) is int and doc['schema'] in [3, 4],
            'Dropped composition protocol version')
    require(type(doc['cases']) is list and len(doc['cases']) == len(config['cases']),
            'Dropped composition case coverage')
    if doc['schema'] == 4:
        for case in doc['cases']:
            fields(case, 'case sparkle_rng_tail initial_idle_deltas rows', 'Dropped composition initial clock')
            deltas = case['initial_idle_deltas']
            require(type(deltas) is list and len(deltas) <= 32, 'Dropped composition initial clock budget')
            for delta in deltas:
                require(0 < item.real(delta) <= 1, 'Dropped composition initial clock delta')
        validate(legacy_projection(doc), config)
        return doc
    projected = copy.deepcopy(doc)
    projected['schema'] = 2
    for case, stripped, control in zip(doc['cases'], projected['cases'], config['cases']):
        fields(case, 'case sparkle_rng_tail rows', 'Dropped composition case')
        require(type(case['rows']) is list and len(case['rows']) == control['frames'] + 2,
                'Dropped composition clock coverage')
        owner = config['covers'][control['cover']]
        for row, flat in zip(case['rows'], stripped['rows']):
            require('cover' in row, 'Dropped composition missing original cover')
            c = flat.pop('cover')
            fields(c, 'parent position visible sprite_visible frame_index animation playing time body_disabled body_layer hit_layer hit_mask timer_left timer_stopped audio rumble',
                   'Dropped composition cover')
            require(c['parent'] in [owner['parent'], owner['new_parent']], 'Dropped composition cover parent')
            item.vector(c['position'])
            for key in ['visible', 'sprite_visible', 'playing', 'body_disabled', 'timer_stopped']:
                item.boolean(c[key])
            item.integer(c['frame_index'], 0, 7)
            require(c['animation'] in ['', 'Break', 'Breakable', 'RESET'], 'Dropped composition cover animation')
            require((c['time'] is None) == (c['animation'] == ''), 'Dropped composition cover clock binding')
            if c['time'] is not None:
                item.real(c['time'])
            item.real(c['timer_left'])
            for key in ['body_layer', 'hit_layer', 'hit_mask']:
                item.integer(c[key], 0, (1 << 20) - 1)
            require(type(c['audio']) is list and len(c['audio']) <= 16, 'Dropped composition voice requests')
            for call in c['audio']:
                require(type(call) is list and len(call) == 2, 'Dropped composition voice request')
                item.integer(call[0], 0, control['frames'])
                item.boolean(call[1])
            require(type(c['rumble']) is list and len(c['rumble']) <= 1, 'Dropped composition rumble requests')
            for call in c['rumble']:
                require(type(call) is list and len(call) == 5, 'Dropped composition rumble request')
                item.integer(call[0], 0, control['frames'])
                item.integer(call[1], 0, 0)
                require(call[2:] == [0.5, 0.8, 0.2], 'Dropped composition rumble source arguments')
            if row['alive']:
                state = flat['state']
                fields(state, 'position scale visible sprite_visible rotation sparkle timer_paused timer_left timer_stopped animation animation_speed animation_time queued prompt parent physics body_disabled area_disabled', 'Dropped composition item')
                require(state.pop('parent') == owner['drop_parent'], 'Dropped composition item parent')
                for key in ['physics', 'body_disabled', 'area_disabled']:
                    item.boolean(state.pop(key))
    item.validate(projected, config)
    return doc
