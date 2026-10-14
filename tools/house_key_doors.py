"""Source-bound automatic key-door investigation; fixed item text, no new VM."""
import copy
import re
from pathlib import Path
from tools.extract_battle_entry import Extractor, PIN, ROOT, node, require
from tools.house_presents import bake, table, LOCALES
from tools.link_house_inspections import text_segments
from tools.house_source_bindings import read
from tools.house_source_bindings import read

BINDING = 'content/house-key-door-bindings.json'


def exact(obj, names, label):
    require(isinstance(obj, dict) and set(obj) == set(names.split()), 'Unknown/missing ' + label)


def texts(ex, house, binding=None):
    b = binding if binding is not None else read(Path(ex.root) / BINDING)
    exact(b, 'schema kind commit sources scene door_scene door_script inventory_script text_script dialogue_table item_table prefix_text_id doors', 'key-door bindings')
    require(type(b['schema']) is int and b['schema'] == 1 and b['kind'] == 'encore.house-key-door-bindings' and b['commit'] == PIN, 'Unreviewed key-door version/pin')
    require(isinstance(b['sources'], dict) and b['sources'], 'Missing key-door sources')
    for path, sha in b['sources'].items():
        ex.data(path)
        require(ex.sources[path] == sha, 'Changed key-door source: ' + path)
    for key in ('scene', 'door_scene', 'door_script', 'inventory_script', 'text_script', 'dialogue_table', 'item_table'):
        require(b[key] in b['sources'], 'Unbound key-door source role: ' + key)
    require(type(b['prefix_text_id']) is int and b['prefix_text_id'] > 0 and isinstance(b['doors'], list) and b['doors'], 'Invalid key-door scope')
    scene, inherited = ex.text(b['scene']), ex.text(b['door_scene'])
    script = ex.text(b['door_script'])
    # Reviewed native scope retains the source default. Consuming keys, flag
    # notifications and item-menu invocation remain distinct unsupported paths.
    require('export (bool) var remove_key = false' in script, 'Changed key retention default')
    dialogue, items = table(ex, b['dialogue_table']), table(ex, b['item_table'])
    rows, ids, paths = [], set(), set()
    for d in b['doors']:
        exact(d, 'id source_path key flag locked_text_id opened_text_id', 'key-door row')
        require(type(d['id']) is int and d['id'] > 0 and d['id'] not in ids, 'Duplicate/invalid key-door identity')
        ids.add(d['id'])
        actual = [r for r in house['openable_doors'] if r['id'] == d['id'] and r['source_path'] == d['source_path']]
        require(len(actual) == 1, 'Missing/ambiguous House key-door binding')
        door = actual[0]; source = node(scene, d['source_path'])
        require(d['key'] and d['flag'] and source.get('key') == door['key'] == d['key'] and source.get('flag') == door['flag'] == d['flag'], 'Key-door source key/flag mismatch')
        require(door['policy'] == 0 and not door['activates_flag'] and not door['deactivates_flag'] and not source.get('remove_key', False), 'Unsupported key-door side effects')
        item_path = 'Data/Items/' + d['key'] + '.yaml'
        require(item_path in b['sources'], 'Unbound key-door item source')
        item = ex.yaml(item_path)
        require(item.get('keyitem') is True, 'Key-door item is not a source key item')
        defaults = {prop: re.search(r'^export .* var ' + prop + r' = "([^"]*)"', script, re.M)[1] for prop in ('interact_locklocked', 'interact_lockopened')}
        for role, prop in (('locked', 'interact_locklocked'), ('opened', 'interact_lockopened')):
            identity, source_path = d[role + '_text_id'], door[role + '_dialogue']
            require(type(identity) is int and identity == b['prefix_text_id'] + len(rows) + 1 and source_path not in paths, 'Key-door text identity/path changed')
            paths.add(source_path)
            require(source_path in b['sources'] and source_path == 'Data/Dialogue/' + source.get(prop, defaults.get(prop)) + '.yaml', 'Key-door dialogue source mismatch')
            doc = ex.yaml(source_path)
            require(set(doc) == {'0'} and set(doc['0']) == {'text'}, 'Unsupported key-door dialogue graph')
            key = doc['0']['text']
            localized = {locale: bake(dialogue[key][column], items[item['name']][column], items[item['article']][column]) for locale, column in LOCALES}
            # No extra tags or commands may be lost while baking fixed item tags.
            for value in localized.values():
                text_segments(value)
            rows.append(dict(id=identity, source_path=source_path, label='0', translation_key=key, item=d['key'], localized=localized))
    return rows


def append_house(ex, house):
    rows = texts(ex, house)
    require(max(d['id'] for d in house['dialogues']) + 1 == rows[0]['id'], 'Key-door stable text prefix changed')
    require(not ({r['source_path'] for r in rows} & {d['source_path'] for d in house['dialogues']}), 'Key-door text already linked')
    out = copy.deepcopy(house)
    for row in rows:
        parts = text_segments(row['localized']['en']); first = len(out['segments'])
        for i, tokens in enumerate(parts):
            out['segments'].append(dict(id=len(out['segments']) + 1, speaker='', voice='', tokens=tokens, flags=3 if i + 1 < len(parts) else 5))
        out['dialogues'].append(dict(id=row['id'], source_path=row['source_path'], first_segment=first, segment_count=len(parts)))
    out['sources'] = dict(sorted(ex.sources.items()))
    out['scope'] += '; source automatic basement-key door investigation and fixed-item localized lock text'
    return out
