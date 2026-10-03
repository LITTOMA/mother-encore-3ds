#!/usr/bin/env python3
"""Compile deterministic source encounter declarations; never execute or sample RNG.

This is an offline admission manifest, not an implementation of Godot combat.
Uncompiled source resources stay source resources. Only proven binary pack pairs
can be adapted to EncounterDependency, and only for their exact dialogue binding.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from extract_battle_entry import properties, variant
import native_battle
import native_round

PIN = '7d9246600fffe518408f5830d4848635019005a3'
SPAWNER = 'Nodes/Overworld/Enemies/Enemy Spawner.tscn'
# Reviewed semantics, not automatic permission from the inventory's current hash.
REVIEWED = {
    'Scripts/global/yaml_parser.gd': '435abe2f1fcfe94475379cc74fc767e97629eef4f584dbd29b8b53444f6165d7',
    'Scripts/Main/Enemy Spawner.gd': '6aa09b9df8d640846a6d5f0b0acc7f88e650a826f2be0a16b0a68f153537773b',
    'Scripts/Main/Basic Enemy.gd': '43aebda6e7412a937c23902fc022e640cfef4396135f5185cb248c418b8dd805',
    'Scripts/global/uiManager.gd': '67b1161f16799963d478e4473cc48d8ca1329bfa041d95b458c4cb7906a7060b',
    'Scripts/global/globalData.gd': '992b1f1a20ca5a5658d02b6ce16b94329a83836f9df5fe7df6b93453c3645c4c',
    'Scripts/UI/DialogueBox.gd': 'cc9f20daa8bdb4e6311625ac480485b72bccf74d35208b6426a66939123e7ed8',
    'Scripts/UI/Battle/BattleSystem.gd': 'd0af7731e0311855de1f0363f8712525269a9f50828033c88989796a657436ca',
    'Scripts/UI/Battle/BattleParticipant.gd': 'a6012e909f32931dd673b884bfc3564ae78741e64a18adb3a23cf5c25c9a1fdb',
    'Scripts/global/Enemy.gd': 'ba81a8f7144dc06cd1674413e5c9b6e670c05aed00c44de98161ad7dda8ebe1b',
    SPAWNER: '06fb8a180f431d8006886949b3b552f70002addf4a5e72e197c9a3fc600c4582',
    'addons/distortionator_integration/scene_importer.gd': '3b800ed49e18d07438e9b75abc98787b837ded3c9128922d634114ecaef798f9',
}
ENEMY_FIELDS = set('name description article nickname maxhp hp maxpp pp offense defense speed iq guts level exp cash items skills boss sprite music musicintro bg ov affinity_multipliers battlescript eats_stolen_items swan_song hide_affinities passive_skills'.split())
BG_FIELDS = set('shader texture texture_stretch opacity blending screen_size move ping_pong_speed oscillation_amplitude oscillation_frequency oscillation_speed osc_amp_ping_pong osc_trans_ping_pong compression_amplitude compression_frequency compression_speed comp_amp_ping_pong comp_trans_ping_pong interlaced_amplitude interlaced_frequency interlaced_speed inter_amp_ping_pong inter_trans_ping_pong palette_shifting_speed palette_shifting palette_anim_frame_count palette barrel effect effect_scale barrelxy'.split())

class DependencyError(ValueError):
    pass

def require(ok, message):
    if not ok:
        raise DependencyError(message)

def sha(data):
    return hashlib.sha256(data).hexdigest()

def safe_path(path):
    require(isinstance(path, str) and path and '\\' not in path and ':' not in path and
            all(p not in ('', '.', '..') for p in path.split('/')), 'Unsafe resource path: ' + str(path))
    return path

def canonical(data):
    return (json.dumps(data, sort_keys=True, indent=2, ensure_ascii=False) + '\n').encode()

class UniqueLoader(yaml.SafeLoader):
    pass

def unique_mapping(loader, node, deep=False):
    result = {}
    for key_node, value_node in node.value:
        key = loader.construct_object(key_node, deep=deep)
        require(key not in result or key in getattr(loader, 'allowed_duplicate_keys', set()), 'Duplicate YAML key: ' + str(key))
        result[key] = loader.construct_object(value_node, deep=deep)
    return result
UniqueLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, unique_mapping)

def appearance_gate(rate):
    """Exact modulo acceptance domain, not an asserted uniform probability."""
    require(type(rate) is int and 0 <= rate <= 100, 'Unreviewed appearance threshold')
    return {'kind': 'independent_modulo_gate', 'raw_appearance_rate': rate, 'modulus': 100,
            'accepted_residue_min': 0, 'accepted_residue_count': 0 if rate == 0 else min(rate + 1, 100),
            'normalized_choice_weight': None,
            'runtime_predicates': ['no_attached_enemy', 'not_in_cutscene', 'timer_expired', 'screen_entered']}

def parse_spawners(text, scene):
    refs = {int(i): p[6:] for p, i in re.findall(r'^\[ext_resource path="(res://[^"]+)"[^\n]* id=(\d+)\]$', text, re.M)}
    records = []
    for match in re.finditer(r'^\[node ([^\n]+)\]\n(.*?)(?=^\[|\Z)', text, re.M | re.S):
        attrs, body = match.groups()
        inst = re.search(r'instance=ExtResource\(\s*(\d+)\s*\)', attrs)
        fields = dict(re.findall(r'(\w+)="([^"]*)"', attrs))
        profile = refs.get(int(inst[1])) if inst else None
        if profile not in (SPAWNER, 'Nodes/Overworld/Enemies/Basic Enemy.tscn'):
            # A serialized enemy outside the audited spawner adapter must not disappear.
            require(not re.search(r'^enemy\s*=', body, re.M), 'Unreviewed direct enemy node in ' + scene + ': ' + attrs)
            continue
        prop = properties(body)
        if profile != SPAWNER and 'enemy' not in prop:
            continue
        allowed = {'position', 'enemy', '_appearance_rate', '_return', '_perma_death', '_inital_direction', 'visible', '__meta__'}
        if profile != SPAWNER:
            allowed = {'position','enemy'}
        require(set(prop) <= allowed, 'Unknown spawner override in ' + scene + ': ' + str(set(prop) - allowed))
        name = fields['name']; parent = fields.get('parent', '.')
        node = name if parent == '.' else parent + '/' + name
        enemy = prop.get('enemy')
        require(isinstance(enemy, str) and enemy, 'Spawner lacks enemy: ' + scene + ':' + node)
        require(type(prop.get('_return', True)) is bool and type(prop.get('_perma_death', False)) is bool, 'Invalid spawner lifetime')
        records.append({'key': scene + '#' + node, 'node': node, 'enemy': enemy.replace(' ', ''),
                        'source_kind': 'spawner' if profile == SPAWNER else 'direct_actor',
                        'source_enemy_text': enemy, 'source_line': text[:match.start()].count('\n') + 1,
                        'gate': appearance_gate(prop.get('_appearance_rate', 100)) if profile == SPAWNER else
                                {'kind': 'serialized_actor_presence', 'accepted_residue_count': 100, 'rng_calls': 0},
                        'serialized_overrides': prop,
                        'lifetime': {'return': prop.get('_return', True), 'perma_death': prop.get('_perma_death', False)},
                        'defaults_from': ['Scripts/Main/Enemy Spawner.gd', SPAWNER] if profile == SPAWNER else ['Scripts/Main/Basic Enemy.gd', profile]})
    return records

def dialogue_battles(text):
    # Construct only resource-bearing commands. Other dialogue fields are outside
    # this adapter (some pinned files repeat presentation keys such as showbox).
    loader = UniqueLoader(text)
    try:
        root = loader.get_single_node()
        require(isinstance(root, yaml.MappingNode), 'Invalid dialogue document')
        segments = set()
        for key, phrase in root.value:
            segment = loader.construct_object(key)
            require(segment not in segments, 'Duplicate dialogue segment')
            segments.add(segment)
            if not isinstance(phrase, yaml.MappingNode):
                continue
            commands = [value for field, value in phrase.value if loader.construct_object(field) == 'startbattle']
            require(len(commands) <= 1, 'Duplicate startbattle command')
            if commands:
                yield str(segment), loader.construct_object(commands[0], deep=True)
    finally:
        loader.dispose()

class Compiler:
    def __init__(self, root=ROOT):
        self.root = Path(root)
        self.upstream = self.root / 'upstream/MOTHER-Encore'
        self.source_root = self.upstream.resolve().parents[1]
        self.inventory = json.loads((self.source_root / 'compatibility/upstream-inventory.json').read_text())
        lock = json.loads((self.source_root / 'upstream.lock').read_text())
        require(lock['commit'] == self.inventory['commit'] == PIN, 'Unreviewed source pin')
        head = subprocess.check_output(['git', '-C', str(self.upstream), 'rev-parse', 'HEAD'], text=True).strip()
        require(head == PIN, 'Upstream HEAD mismatch')
        require(not subprocess.check_output(['git', '-C', str(self.upstream), 'status', '--porcelain'], text=True).strip(), 'Upstream must stay pristine')
        self.resources = {}; self.enemies = {}; self.tables = {}; self.scanned = []
        for path, digest in REVIEWED.items():
            require(sha(self.read(path)) == digest, 'Unreviewed semantic source: ' + path)

    def read(self, path):
        safe_path(path)
        require(path in self.inventory['files'], 'Uninventoried resource: ' + path)
        data = (self.upstream / path).read_bytes()
        digest = sha(data)
        require(digest == self.inventory['files'][path]['sha256'], 'Changed pinned resource: ' + path)
        self.resources[path] = {'sha256': digest, 'bytes': len(data)}
        return data

    def text(self, path):
        return self.read(path).decode('utf-8')

    def yaml(self, path):
        try:
            loader = UniqueLoader(self.text(path))
            # Pinned lynx repeats this non-resource field. The reviewed upstream
            # _parse_dict uses last assignment; no resource key gets this exception.
            loader.allowed_duplicate_keys = {'affinity_multipliers'} if path == 'Data/Battlers/lynx.yaml' else set()
            try:
                return loader.get_single_data()
            finally:
                loader.dispose()
        except DependencyError as error:
            raise DependencyError(path + ': ' + str(error)) from error

    def static_scene(self, path, seen=None):
        seen = set() if seen is None else seen
        if path in seen:
            return seen
        seen.add(path)
        text = self.text(path)
        if path.endswith(('.tscn', '.tres')):
            for child in re.findall(r'^\[ext_resource path="res://([^"]+)"', text, re.M):
                if child.endswith(('.tscn', '.tres')):
                    self.static_scene(child, seen)
                else:
                    self.read(child); seen.add(child)
        return seen

    def background(self, name):
        safe_path(name)
        path = 'Graphics/Battle BGS/' + name + '.bbg'
        text = self.text(path); resources = {path}
        sections = list(re.finditer(r'^\[([^\]]*)\]\s*\n(.*?)(?=^\[|\Z)', text, re.M | re.S))
        require(sections and sections[0][1] == '', 'Unreviewed BBG header: ' + path)
        header = properties(sections[0][2])
        require(header == {'shader_folder': 'res://', 'texture_folder': 'res://'}, 'Unknown BBG resource folders: ' + path)
        labels = [section[1] for section in sections[1:]]
        require(len(labels) == len(set(labels)) and all(re.fullmatch(r'Layer [0-9]+', label) for label in labels), 'Unknown/duplicate BBG sections: ' + path)
        for section in sections[1:]:
            fields = properties(section[2])
            require(set(fields) <= BG_FIELDS and fields.get('shader') == '[DEFAULT]' and
                    fields.get('texture_stretch') == 'STRETCH_TILE', 'Unknown background resource profile: ' + path)
            for key in ('texture', 'palette'):
                if key in fields:
                    resource = fields[key]
                    require(isinstance(resource, str), 'Invalid BBG resource')
                    resource = resource.removeprefix('[Resource]').removeprefix('/')
                    self.read(resource); resources.add(resource)
        require(len(sections) > 1, 'Empty BBG profile')
        return resources

    def enemy(self, enemy):
        safe_path(enemy)
        if enemy in self.enemies:
            return self.enemies[enemy]
        path = 'Data/Battlers/' + enemy + '.yaml'
        data = self.yaml(path)
        require(isinstance(data, dict) and set(data) <= ENEMY_FIELDS, 'Unknown enemy resource fields: ' + path)
        result = {'id': enemy, 'source': path, 'direct_resources': [path], 'reinforcement_enemies': [],
                  'skill_choices': data.get('skills', []), 'scripted_resource_gate': bool(data.get('battlescript')),
                  'compiled': None}
        self.enemies[enemy] = result  # Mark before recursive self/reinforcement references.
        resources = {path}
        require(isinstance(data.get('sprite'), str), 'Missing enemy sprite profile: ' + path)
        sprite = 'Graphics/Battle Sprites/' + data['sprite'] + '.png'
        self.read(sprite); resources.add(sprite)
        resources |= self.background(data.get('bg') or 'lamp')
        for key in ('music', 'musicintro'):
            if data.get(key):
                music = 'Audio/Music/Battle Themes/' + data[key]
                self.read(music); resources.add(music)
        ov = data.get('ov')
        if ov:
            require(isinstance(ov, dict) and set(ov) <= {'type','sprite','anim','spriteOffset','shadow','maxDistance','maxSpeed','acceleration','friction','walk_frequency','walkFrequency','returning','connections'}, 'Unknown overworld resource profile: ' + path)
            require(ov.get('type', 'Basic Enemy') == 'Basic Enemy', 'Unreviewed overworld enemy factory')
            resources |= self.static_scene('Nodes/Overworld/Enemies/Basic Enemy.tscn')
            for resource in ['Graphics/Character Sprites/Enemies/' + ov['sprite'] + '.png', 'Data/Animations/' + ov.get('anim', 'BasicEnemy') + '.yaml']:
                self.read(resource); resources.add(resource)
        choices = data.get('skills', [])
        require(isinstance(choices, list), 'Invalid skill choice table')
        skills = []
        for choice in choices:
            require(isinstance(choice, dict) and isinstance(choice.get('skill'), str) and
                    type(choice.get('weight')) is int and choice['weight'] >= 0, 'Unknown weighted skill entry: ' + path)
            skills.append(choice['skill'])
        # ECS is intentionally not executed or claimed supported. Fixed skill references
        # form a conservative data union; dynamic sprite/phase logic remains gated.
        if data.get('battlescript'):
            script = 'Data/BattleScripts/' + data['battlescript'] + '.ecs'
            script_text = self.text(script); resources.add(script)
            skills += re.findall(r'\.(?:set_scripted_skill|add_skill|do_skill)\("?([A-Za-z0-9_]+)"?\s*[,)]', script_text)
            result['scripted_skill_references'] = sorted(set(skills) - {c['skill'] for c in choices})
            result['source_profile_limit'] = 'ECS dynamic sprite, phase, special-action and condition semantics are not lowered; no readiness grant'
        for skill in sorted(set(skills)):
            skill_path = 'Data/BattleSkills/' + skill + '.yaml'
            spec = self.yaml(skill_path); resources.add(skill_path)
            if 'allies' in spec:
                table = spec['allies']
                require(isinstance(table, list) and table, 'Empty reinforcement choice table')
                for entry in table:
                    require(set(entry) == {'ally','weight'} and isinstance(entry['ally'], str) and
                            type(entry['weight']) is int and entry['weight'] > 0, 'Unknown reinforcement resource profile')
                    target = enemy if entry['ally'] == 'self' else entry['ally']
                    self.enemy(target)
                    result['reinforcement_enemies'].append(target)
                self.tables[skill_path] = {'source': skill_path, 'kind': 'weighted_reinforcement_choice',
                                         'entries': table, 'weights_normalized': False,
                                         'selector': 'allies', 'runtime_selector_source': 'Scripts/UI/Battle/BattleSystem.gd:_try_add_allies',
                                         'runtime_selection': 'rand_range(0.0, sum(weights)); first cumulative weight greater than draw',
                                         'rng_samples_consumed': 0}
        if enemy == 'ratking':
            resources |= self.static_scene('Nodes/Ui/Battle/RatKingSprite.tscn')
        result['direct_resources'] = sorted(resources)
        result['reinforcement_enemies'] = sorted(set(result['reinforcement_enemies']))
        return result

    def union(self, enemies):
        resources, seen = set(), set()
        pending = list(enemies)
        while pending:
            enemy = pending.pop()
            if enemy in seen:
                continue
            seen.add(enemy); profile = self.enemy(enemy)
            resources.update(profile['direct_resources']); pending += profile['reinforcement_enemies']
        return {'enemy_ids': sorted(seen), 'resources': sorted(resources)}

    def build(self):
        scenes, formations = [], []
        common = self.static_scene('Nodes/Ui/Battle/Battle.tscn') | self.static_scene('Nodes/Ui/Battle/Battle Transition.tscn')
        # The reviewed participant module selects from these literal party/NPC scenes.
        participant = self.text('Scripts/UI/Battle/BattleParticipant.gd')
        for scene in re.findall(r'preload\("res://([^"]+\.tscn)"\)', participant):
            common |= self.static_scene(scene)
        for path in sorted(self.inventory['files']):
            if path.startswith('Maps/') and path.endswith('.tscn'):
                text = self.text(path); self.scanned.append(path)
                spawners = parse_spawners(text, path)
                if spawners:
                    for candidate in spawners:
                        self.enemy(candidate['enemy'])
                    union = self.union([s['enemy'] for s in spawners if s['gate']['accepted_residue_count']])
                    scenes.append({'key': path, 'source': path, 'candidates': spawners,
                                   'formation_domain': {'kind': 'runtime_eligible_on_screen_subset',
                                                        'membership_is_runtime_data': True,
                                                        'source': 'Scripts/global/uiManager.gd', 'selector': 'start_battle',
                                                        'maximum_from_serialized_nodes': len(spawners),
                                                        'spatial_coexistence_proven': False,
                                                        'ordered_first_enemy_selects_music_and_background': True},
                                   'source_resource_union': union, 'compiled_dependencies': [],
                                   'combat_gate': 'blocked: on-screen formation executor and compiled encounter packs unavailable'})
            elif path.startswith('Data/Dialogue/') and path.endswith('.yaml'):
                text = self.text(path); self.scanned.append(path)
                # Files without this token cannot contain the exact reviewed command key.
                if not re.search(r'^\s*startbattle:', text, re.M):
                    continue
                for segment, command in dialogue_battles(text):
                    require(isinstance(command, dict) and set(command) <= {'battlers','actorskeep','winflag','wincutscene','fleecutscene','losecutscene'}, 'Unknown battle command fields')
                    members = []
                    for record in command.get('battlers', []):
                        require(isinstance(record, dict) and record, 'Invalid formation member')
                        for enemy, actor in record.items():
                            require(isinstance(enemy, str) and (actor is None or isinstance(actor, str)), 'Invalid actor binding')
                            members.append({'enemy': enemy, 'actor': actor})
                    require(members, 'Dynamic/empty dialogue formation is unreviewed: ' + path)
                    union = self.union([m['enemy'] for m in members])
                    formations.append({'key': path + '#' + str(segment), 'source': path, 'segment': str(segment),
                                       'ordered_members': members, 'source_command': command,
                                       'source_resource_union': union, 'compiled': None,
                                       'combat_gate': 'blocked: no exact checked compiled binding'})
        system = self.text('Scripts/UI/Battle/BattleSystem.gd')
        for resource in re.findall(r'(?:preload|load)\("res://([^"]+)"\)', system):
            if resource.endswith(('.tscn', '.tres')):
                common |= self.static_scene(resource)
            else:
                self.read(resource); common.add(resource)
        music_match = re.search(r'var _musical_effects = (\{.*?\n\})', system, re.S)
        require(music_match is not None, 'Missing reviewed musical effects table')
        for relative in json.loads(music_match[1]).values():
            resource = 'Audio/Music/' + relative
            self.read(resource); common.add(resource)
        for item in scenes + formations:
            item['source_resource_union']['resources'] = sorted(set(item['source_resource_union']['resources']) | common)
        bindings = json.loads((self.root / 'content/encounter-dependencies.bindings.json').read_text())
        require(bindings.get('schema') == 1 and set(bindings) == {'schema','bindings'}, 'Unknown binding schema')
        by_key = {f['key']: f for f in formations}
        for binding in bindings['bindings']:
            require(binding['formation'] in by_key, 'Compiled binding has no source formation')
            formation = by_key[binding['formation']]
            pair = checked_pair(self.root, binding, formation, self)
            require(formation['compiled'] is None, 'Duplicate compiled formation')
            formation['compiled'] = pair
            formation['combat_gate'] = 'checked_pair_available_for_existing_single_party_binding_only'
        result = {'schema': 1, 'kind': 'encore.encounter-dependencies.source-manifest', 'commit': PIN,
                  'coverage': 'all serialized direct Enemy Spawner and Basic Enemy map nodes and dialogue startbattle commands in the pinned inventory',
                  'runtime_limits': ['source declarations are not executable combat', 'arbitrary scripted scene factories and direct debug battles are not lowered',
                                     'on-screen union is conservative, not a claim that every combination is spatially reachable',
                                     'ECS sprite/phase/special-action dependencies remain gated',
                                     'source union covers static ext_resources and reviewed constructed paths; it is not a completeness certificate for arbitrary script-created assets',
                                     'party composition and compiled room/world bindings remain runtime admission conditions'],
                  'rng_samples_consumed': 0, 'semantic_sources': REVIEWED,
                  'shared_entry_resources': sorted(common),
                  'source_data_notes': ['lynx.yaml repeats affinity_multipliers; reviewed YAMLParser uses last assignment; no resource keys are exempted from duplicate rejection'],
                  'scanned_source_files': self.scanned, 'scenes': scenes, 'formations': formations,
                  'reinforcement_tables': [self.tables[k] for k in sorted(self.tables)],
                  'enemy_profiles': [self.enemies[k] for k in sorted(self.enemies)],
                  'source_resources': {k: self.resources[k] for k in sorted(self.resources)}}
        validate_manifest(result)
        return result


def checked_pair(root, binding, formation, source):
    """Prove exact source IR -> checked pack bytes -> same two-participant binding."""
    require(set(binding) == {'formation','entry_ir','round_ir','battle_pack','round_pack'}, 'Unknown compiled binding fields')
    for key in ('entry_ir','round_ir','battle_pack','round_pack'):
        safe_path(binding[key])
    require(binding['battle_pack'].endswith('.encbattle') and binding['round_pack'].endswith('.encround'), 'Wrong pack kinds')
    require(binding['formation'] == formation['key'], 'Compiled source formation identity mismatch')
    commands = dict(dialogue_battles(source.text(formation['source'])))
    require(commands.get(formation['segment']) == formation['source_command'], 'Source dialogue formation changed')
    expected_members = [{'enemy': enemy, 'actor': actor} for group in formation['source_command']['battlers'] for enemy, actor in group.items()]
    require(expected_members == formation['ordered_members'], 'Source formation ordering/actor binding mismatch')
    members = formation['ordered_members']
    require(len(members) == 1, 'Multi-enemy executor is not implemented')
    entry = json.loads((root / binding['entry_ir']).read_text())
    round_ir = json.loads((root / binding['round_ir']).read_text())
    require(entry['commit'] == round_ir['commit'] == PIN and entry['enemy']['id'] == members[0]['enemy'], 'Compiled enemy/source mismatch')
    require(entry['enemy']['data'] == source.yaml('Data/Battlers/' + members[0]['enemy'] + '.yaml'), 'Compiled enemy data mismatch')
    for ir in (entry, round_ir):
        for path, digest in ir['sources'].items():
            require(sha(source.read(path)) == digest, 'Compiled source fingerprint mismatch')
    for path, digest in round_ir['dependencies'].items():
        require(sha((root / safe_path(path)).read_bytes()) == digest, 'Compiled IR dependency fingerprint mismatch: ' + path)
    require(round_ir['dependencies'].get(binding['entry_ir']) == sha((root / binding['entry_ir']).read_bytes()), 'Round is not bound to this entry IR')
    # Existing lowerers prove bytes, rather than accepting any CRC-valid file.
    old_battle, old_round = native_battle.ROOT, native_round.ROOT
    try:
        native_battle.ROOT = native_round.ROOT = root
        assets = json.loads((root / entry['presentation']['asset_receipt_path']).read_text())
        expected_battle = native_battle.encode(native_battle.lower(entry, assets, (root / 'romfs/data/opening.encroom').read_bytes()), PIN)
        expected_round = native_round.encode(native_round.lower(round_ir), PIN)
    finally:
        native_battle.ROOT, native_round.ROOT = old_battle, old_round
    battle = (root / 'romfs' / binding['battle_pack']).read_bytes()
    round_bytes = (root / 'romfs' / binding['round_pack']).read_bytes()
    require(battle == expected_battle and round_bytes == expected_round, 'Compiled pair bytes do not match source IR')
    sections = native_battle.parse_sections(battle); tables = native_round.parse_pack(round_bytes)
    metadata = struct.unpack('<8I', sections[8])
    participants = list(struct.iter_unpack('<4I12i', sections[6]))
    require(len(participants) == 2 and [p[2] for p in participants] == [1, 2], 'Unsupported compiled participant count/types')
    require(metadata[0] == tables['Bindings'][0][0] == round_ir['binding']['battle_id'] and
            tables['Bindings'][0][1:3] == (0, 1), 'Battle/round metadata binding mismatch')
    pool = sections[0]
    require(pool[metadata[1]:pool.index(0, metadata[1])].decode() == members[0]['enemy'], 'Binary enemy identity mismatch')
    # Stage verifies every referenced asset hash, not just the two pack files.
    files = dict(native_battle.stage_files(root / 'romfs', Path(binding['battle_pack'])))
    for path, data in native_round.stage_files(root / 'romfs', Path(binding['round_pack'])).items():
        require(path not in files or files[path] == data, 'Conflicting compiled resource ownership')
        files[path] = data
    return {'key': formation['key'], 'battle_pack': binding['battle_pack'], 'round_pack': binding['round_pack'],
            'battle_id': metadata[0], 'enemy_id': members[0]['enemy'], 'participant_count': 2,
            'source_binding': binding, 'files': {str(k): sha(v) for k, v in sorted(files.items())}}


def validate_manifest(doc):
    require(doc.get('schema') == 1 and doc.get('kind') == 'encore.encounter-dependencies.source-manifest' and doc.get('commit') == PIN, 'Unknown encounter manifest version/profile')
    require(doc.get('rng_samples_consumed') == 0, 'Encounter declarations must not sample RNG')
    profiles = {p['id']: p for p in doc['enemy_profiles']}
    require(len(profiles) == len(doc['enemy_profiles']), 'Duplicate enemy profile')
    resources = doc['source_resources']
    for path, value in resources.items():
        safe_path(path)
        require(set(value) == {'sha256','bytes'} and re.fullmatch('[0-9a-f]{64}', value['sha256']) and type(value['bytes']) is int and value['bytes'] >= 0, 'Invalid source fingerprint')
    identities = set()
    for item in doc['scenes'] + doc['formations']:
        require(item['key'] not in identities, 'Duplicate encounter identity'); identities.add(item['key'])
        require(item['source'] in resources, 'Missing encounter source provenance')
        union = item['source_resource_union']
        require(union['enemy_ids'] == sorted(set(union['enemy_ids'])) and union['resources'] == sorted(set(union['resources'])), 'Noncanonical resource union')
        require(set(doc['shared_entry_resources']) <= set(union['resources']), 'Missing shared entry dependencies')
        require(set(union['enemy_ids']) <= set(profiles) and set(union['resources']) <= set(resources), 'Unresolved encounter resource profile')
        for enemy in union['enemy_ids']:
            require(set(profiles[enemy]['direct_resources']) <= set(union['resources']) and set(profiles[enemy]['reinforcement_enemies']) <= set(union['enemy_ids']), 'Incomplete resource/reinforcement union')
    for scene in doc['scenes']:
        require(scene['compiled_dependencies'] == [], 'Uncompiled source scene cannot fabricate pack pairs')
        for candidate in scene['candidates']:
            if candidate['source_kind'] == 'spawner':
                require(candidate['gate'] == appearance_gate(candidate['gate']['raw_appearance_rate']), 'Changed source appearance semantics')
            else:
                require(candidate['source_kind'] == 'direct_actor' and candidate['gate'] == {'kind': 'serialized_actor_presence', 'accepted_residue_count': 100, 'rng_calls': 0}, 'Unknown scene candidate profile')
            require(candidate['enemy'] in profiles, 'Unknown candidate profile')
    for formation in doc['formations']:
        require(formation['ordered_members'] and all(m['enemy'] in profiles for m in formation['ordered_members']), 'Unknown formation profile')
        pair = formation['compiled']
        if pair is not None:
            require(len(formation['ordered_members']) == 1 and pair['participant_count'] == 2 and pair['enemy_id'] == formation['ordered_members'][0]['enemy'], 'Unimplemented combat composition')


def adapt_compiled_dependencies(doc, formation_keys, scene_epoch, root=ROOT):
    """Strict Python bridge to SceneEncounterDependencies' key/pack-pair shape.

The caller must provide exact reviewed dialogue bindings for the active scene.
It cannot use this function to omit an uncompiled candidate from a scene union.
All selected declarations are validated before producing any dependency output.
"""
    validate_manifest(doc)
    require(type(scene_epoch) is int and 0 < scene_epoch < 2**64, 'Invalid scene epoch')
    require(isinstance(formation_keys, list) and formation_keys and len(formation_keys) <= 1024 and len(set(formation_keys)) == len(formation_keys), 'Invalid/duplicate scene formation declarations')
    all_items = {v['key']: v for v in doc['scenes'] + doc['formations']}
    result = []
    source = Compiler(root)
    for key in formation_keys:
        require(key in all_items, 'Unknown encounter declaration: ' + str(key))
        item = all_items[key]
        require(item.get('compiled') is not None, 'Encounter readiness rejected: ' + item['combat_gate'])
        pair = checked_pair(Path(root), item['compiled']['source_binding'], item, source)
        require(pair == item['compiled'], 'Manifest compiled fingerprints/binding are stale')
        result.append({k: pair[k] for k in ('key','battle_pack','round_pack')})
    require(len({e['battle_pack'] for e in result}) == len(result), 'Compiled pack identity must be unioned')
    return {'scene_epoch': scene_epoch, 'encounters': result}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['compile','verify'], nargs='?', default='compile')
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    out = args.out or args.root / 'content/encounter-dependencies.json'
    try:
        doc = Compiler(args.root).build(); blob = canonical(doc)
        if args.action == 'verify':
            require(out.read_bytes() == blob, 'Encounter dependency manifest stale or changed')
        else:
            out.parent.mkdir(parents=True, exist_ok=True); out.write_bytes(blob)
        print(json.dumps({'manifest': str(out), 'sha256': sha(blob), 'bytes': len(blob),
                          'scenes': len(doc['scenes']), 'source_candidates': sum(len(s['candidates']) for s in doc['scenes']),
                          'spawners': sum(c['source_kind'] == 'spawner' for s in doc['scenes'] for c in s['candidates']),
                          'formations': len(doc['formations']), 'multi_enemy_formations': sum(len(f['ordered_members']) > 1 for f in doc['formations']),
                          'enemy_profiles': len(doc['enemy_profiles']), 'reinforcement_tables': len(doc['reinforcement_tables']),
                          'checked_pairs': sum(f['compiled'] is not None for f in doc['formations']), 'rng_samples_consumed': 0}, sort_keys=True))
        return 0
    except (ValueError, KeyError, TypeError, OSError, struct.error) as error:
        print('ENCOUNTER DEPENDENCY ERROR:', error, file=sys.stderr)
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
