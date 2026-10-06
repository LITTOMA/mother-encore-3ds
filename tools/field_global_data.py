#!/usr/bin/env python3
"""Actual globaldata declaration objects and KEY/STORAGE/Ninten LOAD prefix.

This is deliberately not admission of all globaldata._init/_ready/global.LOAD.
The six YAML caches, GodStorage and remaining character initialization remain
explicit pending cursors; real Object IDs and RNG never come from this IR.
"""
from __future__ import annotations
import argparse, json, math, re, struct, sys, zlib
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.podunk_scene import PIN, read, sha, require, write, stable
from tools.extract_battle_entry import Extractor
IR = ROOT / 'content/native-global-data.json'
REVIEW = ROOT / 'reports/global-data/source-review.json'
PACK = ROOT / 'romfs/data/global.encdata'
FAMILY = 0x454e004d
OWNER = 'Scripts/global/globalData.gd'


def defaults(text):
    rows = []
    for name, typed, expr in re.findall(r'^var (_\w+)(?:: ([A-Za-z]+))?(?:\s*:?=\s*([^\n]+))?$', text, re.M):
        if not expr:
            require(typed in ('String', 'int', 'Inventory'), 'Unknown source native default')
            kind, value = dict(String=(1, ''), int=(2, 0), Inventory=(5, ''))[typed]
        elif expr.strip() in ('[]', '{}', '{ }', 'false'):
            kind, value = {'[]': (3, ''), '{}': (4, ''), '{ }': (4, ''), 'false': (6, 0)}[expr.strip()]
        else:
            raise ValueError('Unknown declaration default ' + name)
        rows.append(dict(name=name, kind=kind, value=value))
    return rows


def extract():
    ex = Extractor(ROOT)
    paths = [OWNER, 'Scripts/global/global.gd', 'Scripts/global/Character.gd',
             'Scripts/global/PartyMember.gd', 'Scripts/global/PartyNPC.gd',
             'Scripts/global/Inventory.gd', 'Scripts/global/Item.gd']
    src = {p: ex.text(p) for p in paths}
    gd, pm, npc, inv = (src[p] for p in (OWNER, paths[3], paths[4], paths[5]))
    require(src[paths[2]].startswith('extends Object\n') and pm.startswith('extends Character\n') and npc.startswith('extends Character\n'), 'Character native Object inheritance changed')
    require(not re.search(r'^extends ', inv, re.M) and not re.search(r'^extends ', src[paths[6]], re.M), 'Inventory/Item implicit Reference inheritance changed')
    require('func _init(data := {}):\n\tif data:\n\t\tinit_from_dict(data)' in pm, 'Empty PartyMember constructor changed')
    require('func _init(data := {}, constant_data := {}):\n\tinit_from_dict(data)\n\tinit_from_dict(constant_data)' in npc and 'func init_from_dict(dict: Dictionary):\n\tif dict:' in npc, 'Empty PartyNPC constructor changed')
    require('func _init(type:= InvType.NORMAL, load_data:= []):\n\t_type = type\n\tif _type == InvType.STORAGE_GOD:\n\t\t_init_god_storage()\n\tif load_data:\n\t\tinit_from_serialized(load_data)' in inv, 'Inventory constructor changed')
    require('Item.new(item["item_name"], item.get("equipped", false),\\\n\t\t\t\titem.get("doses", 1), int(item.get("uid", Item.get_uid(Item.used_uids_tab))))' in inv, 'Eager fallback UID source changed')
    require('_inventory = Inventory.new(Inventory.InvType.NORMAL, dict.get("inventory", []))' in pm, 'Party inventory construction cursor changed')
    block = gd.split('var characters := {\n', 1)[1].split('\n}', 1)[0]
    matches = re.findall(r'\t(PartyMember|PartyNPC)\.(\w+): (PartyMember|PartyNPC)\.new\(\),?', block)
    require(len(matches) == 8 and not re.sub(r'\t(?:PartyMember|PartyNPC)\.\w+: (?:PartyMember|PartyNPC)\.new\(\),?|\s', '', block), 'Unknown character declaration grammar')
    enum = re.search(r'enum InvType \{([^}]+)\}', inv)[1].replace(' ', '').split(',')
    rows = []
    for cls, key, constructor in matches:
        require(cls == constructor, 'Character constructor class mismatch')
        script = 'Scripts/global/' + cls + '.gd'
        name = re.search(r'^const ' + key + r' := "([^"]+)"$', src[script], re.M)[1]
        fields = defaults(src[paths[2]].split('func ', 1)[0]) + defaults(src[script].split('func ', 1)[0])
        rows.append(dict(id=stable(OWNER + '#characters.' + name), kind=1,
                         role=0 if cls == 'PartyMember' else 1, name=name,
                         native='Object', script=script, fields=fields))
    for name, etype in re.findall(r'^var (\w+) := Inventory.new\(Inventory.InvType.(\w+)\)$', gd, re.M):
        require(etype in ('KEY', 'STORAGE'), 'Unknown inventory declaration')
        rows.append(dict(id=stable(OWNER + '#' + name), kind=2, role=enum.index(etype),
                         name=name, native='Reference', script=paths[5],
                         fields=[dict(name='_type', kind=2, value=enum.index(etype)), dict(name='_items', kind=3, value='')]))
    global_script = src[paths[1]]
    order = [global_script.index('globaldata.key_items.init_from_serialized'),
             global_script.index('globaldata.storage.init_from_serialized'),
             global_script.index('for char_id in globaldata.characters:')]
    require(order == sorted(order), 'global.LOAD inventory/character order changed')
    require('god_storage = Inventory.new(Inventory.InvType.STORAGE_GOD)' in gd, 'GodStorage pending Ready cursor changed')
    cache = re.search(r'var to_load := \{(.*?)\n\t\}', gd, re.S)[1]
    caches = re.findall(r'"(\w+)": (_\w+)', cache)
    require(len(caches) == 6, 'globaldata cache constructor changed')
    save = ex.yaml('Data/save_new_game.yaml'); overrides = ex.yaml('Data/save_overrides.yaml')
    require(save['party'] == [rows[0]['name']], 'Source prefix first member changed')
    inventory = read(ROOT / 'content/native-field-inventory.json')
    definitions = read(ROOT / 'content/native-field-item-definitions.json')
    for p, h in inventory['sources'].items():
        ex.data(p); require(ex.sources[p] == h, 'Inventory source identity differs')
    owners = inventory['owners']
    require([r['name'] for r in rows[-2:]] == [o['name'] for o in owners[1:]], 'Inventory source owner correspondence')
    pending = [dict(cursor='_init/_load_data', source=OWNER, argument='Data/' + n + '/') for n, _ in caches]
    pending += [dict(cursor='_ready/_init_god_storage', source=OWNER, argument='god_storage')]
    pending += [dict(cursor='global.LOAD/characters', source=paths[1], argument=r['name']) for r in rows[1:-2]]
    ninten = dict(save[rows[0]['name']]); override = overrides[rows[0]['name']]
    require(set(override) == {'name', 'learnedSkills', 'affinity_multipliers'}, 'Unknown first-character override')
    ninten.update(name=override['name'], affinity_multipliers=override['affinity_multipliers'])
    skills = list(ninten['learnedSkills'])
    for skill in override['learnedSkills']:
        if skill not in skills: skills.append(skill)
    skill_text = re.search(r'const SKILLS_ORDER := \[(.*?)\n\]', pm, re.S)[1]
    clean = '\n'.join(line.split('#', 1)[0] for line in skill_text.splitlines())
    skills_order = re.findall(r'"([A-Za-z0-9]+)"', clean)
    require(len(skills_order) == len(set(skills_order)) and not re.sub(r'"[A-Za-z0-9]+"|[\s,]', '', clean), 'Unknown source skills ordering')
    require(all(s in skills_order for s in skills) and not ninten['status'] and not ninten.get('permanent_boosts', {}), 'Unsupported initial character status/permanent values')
    require('return 0 if level == 1 else _level_to_exp(LEVEL_CAP) if level > LEVEL_CAP else int(level * level * (level + 1) * .75)' in pm and '_set_exp(max(dict["exp"], _level_to_exp(dict["level"])) as int, false)' in pm, 'Experience source formula changed')
    cap = int(re.search(r'const LEVEL_CAP := (\d+)', pm)[1])
    character = dict(name=ninten['name'], nickname=ninten.get('nickname', ninten['name']), exp=ninten['exp'], skills=skills, skills_order=skills_order, affinities=list(ninten['affinity_multipliers'].items()), exp_levels=[0 if level == 1 else int(level*level*(level+1)*.75) for level in range(1, cap+1)], slots=['_name','_nickname','_exp','_level','_hp','_pp'], stat_slots=['_'+s for s in inventory['stats']])
    for slot in character['slots'] + character['stat_slots']:
        require(any(f['name'] == slot for f in rows[0]['fields']), 'Unknown source field binding ' + slot)
    require('emit_signal("stat_changed", stat, new_value, max_value)' in src[paths[2]] and 'set_hp(_hp + diff)' in src[paths[2]] and 'set_pp(_pp + diff)' in src[paths[2]], 'Nested stat signal order changed')
    d = dict(schema=1, kind='encore.global-data-owner.source-ir', commit=PIN, family=FAMILY,
             owner=OWNER, declarations=rows, source_types=enum,
             inventory_owners=owners, first_character=rows[0]['id'],
             normal_inventory_defaults=defaults(inv.split('func ', 1)[0]),
             character=character,
             load_roles=[enum.index('KEY'), enum.index('STORAGE'), enum.index('NORMAL')],
             next_character=rows[1]['id'], pending=pending,
             dependencies={p: sha(ROOT / p) for p in ['content/native-field-inventory.json', 'content/native-field-item-definitions.json']},
             sources=dict(sorted(ex.sources.items())), policy=dict(uid='eager-clock-randomize-unique-before-item-object', object='same-process-registry-sequential', saved_uid_zero=True, character_native='Object', inventory_native='Reference', entire_constructor=False, entire_ready=False, entire_load=False))
    validate(d); write(IR, d); write(REVIEW, dict(schema=1, commit=PIN, ir_sha256=sha(IR), sources=d['sources'], dependencies=d['dependencies'], policy=d['policy'], scope='Complete declaration objects and checked inventory LOAD prefix only; pending source caches/Ready/remaining characters must not be skipped.'))
    return d


def validate(d):
    require(set(d) == set('schema kind commit family owner declarations source_types inventory_owners normal_inventory_defaults character first_character load_roles next_character pending dependencies sources policy'.split()), 'Unknown globalData IR fields')
    require(d['schema'] == 1 and d['kind'] == 'encore.global-data-owner.source-ir' and d['commit'] == PIN and d['family'] == FAMILY and d['owner'] == OWNER, 'globalData identity')
    require(d['source_types'] == ['NORMAL', 'KEY', 'STORAGE', 'STORAGE_GOD'] and d['load_roles'] == [1, 2, 0], 'Unsupported inventory source type/order')
    require(d['policy'] == dict(uid='eager-clock-randomize-unique-before-item-object', object='same-process-registry-sequential', saved_uid_zero=True, character_native='Object', inventory_native='Reference', entire_constructor=False, entire_ready=False, entire_load=False), 'Unknown constructor capability')
    rows = d['declarations']; require(len(rows) == 10 and len({r['id'] for r in rows}) == len(rows), 'Incomplete declaration slice')
    require(d['first_character'] == rows[0]['id'] and d['next_character'] == rows[1]['id'] and len(d['pending']) == 14, 'Unknown source continuation')
    for i, r in enumerate(rows):
        require(set(r) == set('id kind role name native script fields'.split()) and r['kind'] == (1 if i < 8 else 2) and r['native'] == ('Object' if i < 8 else 'Reference') and r['id'] == stable(OWNER + ('#characters.' if i < 8 else '#') + r['name']) and r['script'] in d['sources'], 'Unknown source declaration object')
        require(len({f['name'] for f in r['fields']}) == len(r['fields']), 'Duplicate declaration field')
        for f in r['fields']:
            require(set(f) == {'name', 'kind', 'value'} and re.fullmatch('_\w+', f['name']) and 1 <= f['kind'] <= 6 and (type(f['value']) is int if f['kind'] in (2, 6) else f['value'] == ''), 'Unsupported native declaration value')
    require(len(d['inventory_owners']) == 3 and [o['role'] for o in d['inventory_owners']] == [0, 1, 2], 'Unknown inventory owner projection')
    require(d['normal_inventory_defaults'] == [dict(name='_type', kind=2, value=0), dict(name='_items', kind=3, value='')], 'Unknown NORMAL inventory defaults')
    c=d['character'];require(set(c)==set('name nickname exp skills skills_order affinities exp_levels slots stat_slots'.split()) and c['name'] and type(c['exp']) is int and c['exp']>=0 and len(c['skills_order'])==len(set(c['skills_order'])) and all(s in c['skills_order'] for s in c['skills']) and len(c['exp_levels'])>=1 and c['exp_levels'][0]==0 and c['exp_levels']==sorted(set(c['exp_levels'])) and len(c['slots'])==6 and len(c['stat_slots'])==7, 'Unknown character initialization policy')
    require(len({k for k,v in c['affinities']})==len(c['affinities']) and all(type(k)is str and k and type(v)in(int,float)and math.isfinite(v)and v>=0 for k,v in c['affinities']), 'Unknown character affinity value')
    for p in d['pending']: require(set(p) == {'cursor', 'source', 'argument'} and p['source'] in d['sources'] and all(type(v) is str and v for v in p.values()), 'Unknown pending source cursor')
    return d


def load():
    d = validate(read(IR)); review = read(REVIEW)
    require(review['schema'] == 1 and review['commit'] == PIN and review['ir_sha256'] == sha(IR) and review['sources'] == d['sources'] and review['dependencies'] == d['dependencies'] and review['policy'] == d['policy'], 'globalData semantic review differs')
    ex = Extractor(ROOT)
    for p, h in d['sources'].items(): ex.data(p); require(ex.sources[p] == h, 'Changed globalData source ' + p)
    for p, h in d['dependencies'].items(): require(sha(ROOT / p) == h, 'Changed owner dependency ' + p)
    return d


def encode(d):
    validate(d); b = bytearray(bytes.fromhex(PIN) + bytes.fromhex(sha(IR)))
    def u(n): b.extend(struct.pack('<I', n))
    def s(v): raw = v.encode(); u(len(raw)); b.extend(raw)
    s(d['owner']); b.extend(bytes.fromhex(d['dependencies']['content/native-field-inventory.json']))
    u(len(d['declarations'])); u(len(d['pending'])); u(len(d['sources'])); u(d['first_character']); u(d['next_character'])
    for r in d['declarations']:
        for k in ('id', 'kind', 'role'): u(r[k])
        for k in ('name', 'native', 'script'): s(r[k])
        u(len(r['fields']))
        for f in r['fields']:
            s(f['name']); u(f['kind']); b.extend(struct.pack('<q', f['value'] if type(f['value']) is int else 0)); s(f['value'] if type(f['value']) is str else '')
    for o in d['inventory_owners']: u(o['id']); u(o['role']); s(o['name'])
    for r in d['load_roles']: u(r)
    u(len(d['normal_inventory_defaults']))
    for f in d['normal_inventory_defaults']:
        s(f['name']); u(f['kind']); b.extend(struct.pack('<q', f['value'] if type(f['value']) is int else 0)); s(f['value'] if type(f['value']) is str else '')
    c=d['character'];s(c['name']);s(c['nickname']);b.extend(struct.pack('<q',c['exp']))
    for field in ('skills','skills_order','slots','stat_slots'):
        u(len(c[field]))
        for value in c[field]:s(value)
    u(len(c['exp_levels']))
    for value in c['exp_levels']:b.extend(struct.pack('<q',value))
    u(len(c['affinities']))
    for key,value in c['affinities']:s(key);b.extend(struct.pack('<d',value))
    for p in d['pending']:
        for k in ('cursor', 'source', 'argument'): s(p[k])
    for p, h in d['sources'].items(): s(p); b.extend(bytes.fromhex(h))
    return struct.pack('<8s6I', b'ENCGDAT1', 1, 32 + len(b), zlib.crc32(b), FAMILY, 1, 1) + b


def stage_files(source):
    raw = encode(load()); path = Path('data/global.encdata')
    require((Path(source) / path).read_bytes() == raw, 'Stale globalData owner binary')
    return {path: raw}


if __name__ == '__main__':
    p = argparse.ArgumentParser(); p.add_argument('action', choices=['extract', 'compile']); a = p.parse_args()
    try:
        if a.action == 'extract': print('Source declaration objects:', len(extract()['declarations']))
        else: PACK.parent.mkdir(parents=True, exist_ok=True); PACK.write_bytes(encode(load())); print('globalData owner binary:', PACK.stat().st_size)
    except (ValueError, KeyError, TypeError, OSError, struct.error) as e: sys.exit('GLOBAL DATA ERROR: ' + str(e))
