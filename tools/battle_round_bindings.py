#!/usr/bin/env python3
"""Checked source bindings for the already supported battle round adapters."""
import argparse, hashlib, json, math, re
from pathlib import Path
from extract_battle_entry import ROOT, PIN, one, require

RECIPE = ROOT / 'content/battle-round-bindings.json'

def fields(value, names, label):
    require(type(value) is dict and set(value) == set(names), 'Unknown/missing ' + label)

def unique_pairs(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'Duplicate binding field ' + key)
        result[key] = value
    return result

def safe(path):
    return isinstance(path, str) and bool(path) and ':' not in path and '\\' not in path and all(p not in ('', '.', '..') for p in path.split('/'))

def load(root=ROOT, recipe=None):
    root = Path(root)
    if recipe is None:
        recipe = json.loads((root / 'content/battle-round-bindings.json').read_text(encoding='utf-8'), object_pairs_hook=unique_pairs)
    fields(recipe, ['schema', 'commit', 'sources', 'skills', 'menus', 'constants', 'source_facts', 'boss_shake'], 'round bindings')
    require(type(recipe['schema']) is int and recipe['schema'] == 1 and recipe['commit'] == PIN, 'Round binding schema/pin')
    inventory = json.loads((root / 'compatibility/upstream-inventory.json').read_text())
    require(inventory['commit'] == PIN and type(recipe['sources']) is dict and 4 <= len(recipe['sources']) <= 32, 'Round binding inventory/sources')
    source = {}
    for path, sha in recipe['sources'].items():
        require(safe(path) and path in inventory['files'] and sha == inventory['files'][path]['sha256'], 'Unreviewed binding source ' + str(path))
        data = (root / 'upstream/MOTHER-Encore' / path).read_bytes()
        require(hashlib.sha256(data).hexdigest() == sha, 'Changed binding source ' + path)
        source[path] = data.decode('utf-8')
    skills = recipe['skills']
    require(type(skills) is list and 2 <= len(skills) <= 16, 'Round binding skills')
    names = set()
    for index, skill in enumerate(skills):
        fields(skill, ['id', 'name', 'source', 'actor'], 'skill binding')
        require(type(skill['id']) is int and skill['id'] == index + 1 and isinstance(skill['name'], str) and re.fullmatch(r'[a-z][a-z0-9_]*', skill['name']) and skill['name'] not in names, 'Duplicate/unstable skill binding')
        require(skill['source'] == 'Data/BattleSkills/' + skill['name'] + '.yaml' and skill['source'] in source and skill['actor'] in ('party', 'enemy'), 'Skill source/actor binding')
        names.add(skill['name'])
    fields(recipe['menus'], ['basic', 'items', 'guard'], 'menu bindings')
    identities = set()
    for role, menu in recipe['menus'].items():
        fields(menu, ['identity', 'source'], 'menu source binding')
        require(isinstance(menu['identity'], str) and re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*', menu['identity']) and menu['identity'] not in identities and isinstance(menu['source'], str) and menu['source'] in source, 'Menu binding identity/source')
        identities.add(menu['identity'])
    fields(recipe['constants'], ['basic', 'guard'], 'skill constants')
    for role, binding in recipe['constants'].items():
        fields(binding, ['source', 'constant', 'skill'], 'constant binding')
        require(binding['source'] in source and isinstance(binding['constant'], str) and re.fullmatch(r'[A-Z][A-Z0-9_]*', binding['constant']) and binding['skill'] in names, 'Constant source/reference')
        actual = one(r'^const ' + re.escape(binding['constant']) + r'\s*:=\s*"([a-z0-9_]+)"', source[binding['source']], role + ' skill constant')[1]
        require(actual == binding['skill'], 'Changed source skill constant')
        require(next(s for s in skills if s['name'] == actual)['actor'] == 'party', 'Party skill actor')
    require(recipe['constants']['basic']['skill'] != recipe['constants']['guard']['skill'], 'Duplicate action skill')
    for role, menu in recipe['menus'].items():
        body = one(r'^\t\t"' + re.escape(menu['identity']) + r'":\n(.*?)(?=^\t\t"|^func |\Z)', source[menu['source']], role + ' menu branch', re.M | re.S)[1]
        if role == 'basic': require('globaldata.get_battle_skill(bp.character.get_basic_skill())' in body, 'Changed basic menu mechanism')
        elif role == 'items': require('ItemAction.new(bp)' in body and '_goto_menu($ItemsBox, item_action)' in body, 'Changed item menu mechanism')
        else: require('globaldata.get_battle_skill(globaldata.' + recipe['constants']['guard']['constant'] + ')' in body and '_do_guard(skill_action)' in body, 'Changed guard menu mechanism')
    require(type(recipe['source_facts']) is dict and 2 <= len(recipe['source_facts']) <= 16, 'Missing binding semantic facts')
    for path, facts in recipe['source_facts'].items():
        require(path in source and type(facts) is list and 1 <= len(facts) <= 16 and all(isinstance(f, str) and f and f in source[path] for f in facts) and len(set(facts)) == len(facts), 'Changed binding semantic fact')
    shake = recipe['boss_shake']
    fields(shake, ['source', 'parameter', 'value', 'facts'], 'boss shake binding')
    require(shake['source'] in source and isinstance(shake['parameter'], str) and re.fullmatch(r'[a-z_]+', shake['parameter']), 'Shake source/parameter')
    require(type(shake['value']) in (int, float) and math.isfinite(shake['value']) and 0 < shake['value'] <= 1, 'Shake weight')
    actual = float(one(r'^func _init\([^\n]*\b' + re.escape(shake['parameter']) + r' := ([0-9.]+)', source[shake['source']], 'shake default')[1])
    require(actual == shake['value'], 'Changed source shake default')
    require(type(shake['facts']) is list and 2 <= len(shake['facts']) <= 16 and all(isinstance(f, str) and f and f in source[shake['source']] for f in shake['facts']) and len(set(shake['facts'])) == len(shake['facts']), 'Changed shake mechanism fact')
    entry = json.loads((root / 'content/native-battle.json').read_text(encoding='utf-8'))
    require(entry['commit'] == PIN and entry['party']['basic_skill_id'] == recipe['constants']['basic']['skill'], 'Battle entry basic binding')
    require({m['id'] for m in entry['menu']['actions']} == identities, 'Battle entry menu bindings')
    require({s['skill'] for s in entry['enemy']['data']['skills']} == {s['name'] for s in skills if s['actor'] == 'enemy'}, 'Battle entry enemy bindings')
    return recipe

def check_round(ir, recipe):
    require([(s['id'], s['source']) for s in ir['skills']] == [(s['id'], s['source']) for s in recipe['skills']], 'Round IR skill identity/order mismatch')
    for role in ('basic', 'guard'):
        index = ir['binding'][role + '_skill']
        require(type(index) is int and 0 <= index < len(ir['skills']), 'Round IR action binding index')
        require(ir['skills'][index]['source'] == next(s['source'] for s in recipe['skills'] if s['name'] == recipe['constants'][role]['skill']), 'Round IR action binding mismatch')
    if 'boss_shakes' in ir:
        require(all(s['weight'] == recipe['boss_shake']['value'] for s in ir['boss_shakes']), 'Round IR boss shake source default mismatch')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['verify'], nargs='?', default='verify')
    parser.parse_args()
    try:
        recipe = load()
        for path in ['native-round.json', 'doll-round.json', 'pillow-round.json']:
            check_round(json.loads((ROOT / 'content' / path).read_text()), recipe)
        print('Verified independent battle round source bindings')
        return 0
    except (ValueError, OSError, KeyError, TypeError, IndexError) as error:
        print('ROUND BINDING ERROR:', error)
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
