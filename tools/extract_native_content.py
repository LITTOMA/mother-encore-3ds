#!/usr/bin/env python3
"""Extract the pinned, already reviewed opening slice into external typed IR.

This is a one-time/baseline source adapter, not the data-only compiler. It reads
original upstream bytes, native Godot exports, existing scoped reviews and asset
receipts. It never reads generated C++ or runtime C++ and never runs a C++ tool.
Changing an upstream fingerprint requires the existing semantic-review process;
this adapter does not approve new scripts, scenes, or general Godot behavior.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.world_geometry import geometry, transform, shape_points, vec, real, f32
from tools.house_layers import records as overlay_records, receipt as overlay_receipt
from tools.character_animation import build as animation_profile
from tools.lamp_dialogue import compile_receipt, verify_sources
from tools.reference_progression import fixture as validate_progression
from tools.upstream import git
from tools.map_asset import layout as map_layout
from tools.doll_dialogue import receipt as doll_receipt, compile_dialogue as compile_doll_dialogue
from tools.doll_postwin import receipt as postwin_receipt, compile_dialogue as compile_postwin_dialogue, return_duration as postwin_return_duration
from tools.melody_dialogue import receipt as melody_receipt, compile_melody, compile_guard
from tools.native_content import OPCODES as SCHEMA_OPCODES
from tools.world_program_bindings import load as load_world_bindings, actor_indices as world_actor_indices, program as world_program, actor as world_actor, object_indices as world_object_indices, clip_name as world_clip_name, motion as world_motion, encounter as world_encounter

NONE = 0xffffffff
# Schema/execution enums, never source names dispatched by the runtime.
OPCODES = {name:i for i,name in enumerate(SCHEMA_OPCODES)}
SECTIONS = ('Resource Vec2 Polygon BodyRule Overlay MapDraw Clip Key DirectionFrame '
            'ActorProfile ActorInstance CameraArea Flag InitialFlag Condition Trigger '
            'Program Command Binding Battle Rule Experience Scene AnimationBinding MovementPath MovementEntry').split()
NUMBER = r'(-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def one(pattern, text, label, flags=re.M):
    matches = list(re.finditer(pattern, text, flags))
    require(len(matches) == 1, 'Missing or ambiguous reviewed source selector: ' + label)
    return matches[0]


def node_block(text, path):
    """Select a serialized node block. This is deliberately not a TSCN parser."""
    found = []
    for match in re.finditer(r'^\[node ([^\n]+)\]\n(.*?)(?=^\[|\Z)', text, re.M | re.S):
        attrs = dict(re.findall(r'(\w+)="([^"]*)"', match[1]))
        name, parent = attrs.get('name'), attrs.get('parent')
        actual = '.' if parent is None else name if parent == '.' else parent + '/' + name
        if actual == path:
            found.append(match[2])
    require(len(found) == 1, 'Unknown/ambiguous serialized node: ' + path)
    return found[0]


def prop(block, name):
    return one(r'^' + re.escape(name) + r' = (.*)$', block, name)[1]


def pair(raw):
    m = one(r'^Vector2\(\s*' + NUMBER + r'\s*,\s*' + NUMBER + r'\s*\)$', raw, raw)
    return [f32(float(m[1])), f32(float(m[2]))]


def image_size(path):
    raw = path.read_bytes()
    require(raw[:8] == b'\x89PNG\r\n\x1a\n' and raw[12:16] == b'IHDR', 'Expected source PNG')
    return list(struct.unpack('>II', raw[16:24]))


class Extractor:
    def __init__(self, root=ROOT):
        self.root = Path(root)
        self.upstream = self.root / 'upstream/MOTHER-Encore'
        self.sources = {}
        self.projections = []
        self.mapping = {}
        self.strings = ['']
        self.sections = {name: [] for name in SECTIONS}
        self.lock = self.document('upstream.lock')
        self.inventory = self.document('compatibility/upstream-inventory.json')
        require(self.lock['commit'] == self.inventory['commit'], 'Inventory/source lock mismatch')
        require(git(self.upstream, 'rev-parse', 'HEAD') == self.lock['commit'], 'Upstream HEAD differs from reviewed pin')
        require(not git(self.upstream, 'status', '--porcelain'), 'Baseline extraction requires pristine read-only upstream')
        self.reviews = {}
        for name in ('opening-world', 'ninten-animation', 'ninten-sprite', 'house-background',
                     'actor-actions', 'cutscene-camera', 'movement', 'world-flags',
                     'lamp-world', 'lamp-dialogue', 'progression'):
            review = self.document('compatibility/reviews/' + name + '-v0410.json')
            require(review['commit'] == self.lock['commit'] and review['game_version'] == self.lock['game_version'],
                    'Review/source lock mismatch: ' + name)
            for path, expected in review.get('sources', {}).items():
                self.source(path, expected)
            for path, expected in review.get('inputs', {}).items():
                self.file(path, expected)
            self.reviews[name] = review

    def file(self, relative, expected=None):
        path = self.root / relative
        digest = sha(path)
        require(expected is None or digest == expected, 'Changed unreviewed input: ' + relative)
        self.sources[relative] = digest
        return path

    def document(self, relative, expected=None):
        return json.loads(self.file(relative, expected).read_text(encoding='utf-8'))

    def catalog_bindings(self, relative, identities):
        from tools.catalog_projection import binding_projection, verify_projection
        previous = [r for r in self.projections if r['path'] == relative]
        require(len(previous) <= 1, 'Duplicate catalog projection')
        if previous:
            verify_projection(self.root, previous[0])
            identities = sorted(set(identities) | set(previous[0]['identities']))
        catalog, record = binding_projection(self.root, relative, identities)
        self.file('tools/catalog_projection.py')
        if previous:
            self.projections[self.projections.index(previous[0])] = record
        else:
            self.projections.append(record)
        return catalog

    def source(self, relative, expected=None):
        require(relative in self.inventory['files'], 'Source missing from pinned inventory: ' + relative)
        pinned = self.inventory['files'][relative]['sha256']
        require(expected is None or expected == pinned, 'Review and inventory disagree: ' + relative)
        return self.file('upstream/MOTHER-Encore/' + relative, pinned)

    def text(self, relative):
        return self.source(relative).read_text(encoding='utf-8')

    def string(self, value):
        require(isinstance(value, str) and '\0' not in value, 'Invalid IR string')
        if value not in self.strings:
            self.strings.append(value)
        return self.strings.index(value)

    def record_map(self, target, source, selector, derivation=None):
        self.mapping[target] = dict(source=source, selector=selector)
        if derivation:
            self.mapping[target]['derivation'] = derivation

    def scalar(self, source, pattern, target, kind=2):
        text = self.text(source)
        m = one(pattern, text, target)
        value = float(m[1])
        require(math.isfinite(value), 'Non-finite source rule')
        if kind == 1:
            value = f32(value)
        self.record_map(target, 'upstream/MOTHER-Encore/' + source,
                        {'regex': pattern, 'line': text[:m.start()].count('\n') + 1},
                        'IEEE-754 binary32 rounding' if kind == 1 else 'GDScript numeric binary64')
        return value

    def add_resource(self, path, width=0, height=0, columns=0, rows=0, kind=1, expected=None):
        file = self.source(path[6:]) if kind == 2 else self.file('romfs/' + path, expected)
        result = len(self.sections['Resource'])
        self.sections['Resource'].append(dict(stable_id=result + 1, path_string=self.string(path),
            width=width, height=height, columns=columns, rows=rows, kind=kind, flags=0, sha256=sha(file)))
        return result

    def run(self):
        s = self.sections
        self.world_bindings=load_world_bindings(self)
        house_path = 'reports/cloud-world/house-exact.json'
        player_path = 'reports/cloud-world/player-exact.json'
        house = self.document(house_path)
        player = self.document(player_path)
        hn = {n['path']: n for n in house['nodes']}
        hr = {r['id']: r for r in house['resources']}
        house_source = 'Maps/podunk/Nintens House.tscn'
        house_text = self.text(house_source)
        actor_hull, polygons = geometry(house, player)
        s['Vec2'] = [dict(x=x, y=y) for x, y in actor_hull]
        bodies = [n for n in house['nodes'] if n['class'] in ('StaticBody2D', 'KinematicBody2D')]
        require(len(bodies) == 21, 'Unreviewed opening body namespace')
        body_ids = {n['path']: i + 1 for i, n in enumerate(bodies)}
        owners = list(dict.fromkeys(p['owner'] for p in polygons))
        owner_ids = {p: i + 1 for i, p in enumerate(owners)}
        # Existing reviewed fresh-world behavior: openable doors apply lock state,
        # NPC visibility rewrites collisions, invisible static bodies stay enabled.
        door_script = self.text('Scripts/Main/Openable Door.gd')
        door_defaults = {key: one(r'^export \([^\n]+\) var ' + key + r' = (""|false)\s*(?:#.*)?$',
                                  door_script, 'door default ' + key)[1]
                         for key in ('key', 'blocked', 'one_way', 'locked')}
        for n in bodies:
            path = n['path']
            enabled = True
            if re.fullmatch(r'Below/Openable Door\d*/StaticBody2D', path):
                block = node_block(house_text, path.rsplit('/', 1)[0])
                values = {}
                for key, default in door_defaults.items():
                    matches = re.findall(r'^' + key + r' = (.*)$', block, re.M)
                    require(len(matches) <= 1, 'Ambiguous door property')
                    raw = matches[0] if matches else default
                    values[key] = json.loads(raw)
                enabled = any(values.values())
            elif n['class'] == 'KinematicBody2D':
                enabled = all(hn[parent]['properties'].get('visible', True)
                              for parent in ['.'] + [ '/'.join(path.split('/')[:i]) for i in range(1, len(path.split('/'))+1)])
            s['BodyRule'].append(dict(body_id=body_ids[path], source_path_string=self.string(path),
                                      initially_enabled=int(enabled), flags=0))
        for i, polygon in enumerate(polygons):
            vs = polygon['vertices']; start = len(s['Vec2'])
            s['Vec2'].extend(dict(x=x, y=y) for x, y in vs)
            s['Polygon'].append(dict(body_id=body_ids[polygon['body']], owner_id=owner_ids[polygon['owner']],
                first_vertex=start, vertex_count=len(vs), flags=int(polygon['disabled']),
                minimum=[min(x for x, y in vs), min(y for x, y in vs)],
                maximum=[max(x for x, y in vs), max(y for x, y in vs)]))
        self.record_map('Vec2/Polygon', house_path + ';' + player_path,
                        'native physics_shape_owners + resource shapes + transforms',
                        'world_geometry.geometry; float32 multiply/add order; actor hull first')
        self.record_map('BodyRule', house_path + ';upstream/MOTHER-Encore/' + house_source,
                        '21 bodies; door serialized overrides; existing world-flags-v0410 fresh-world rules')

        # Source-backed texture layout and compiled-byte identities.
        animation_bindings=self.world_bindings['animation']
        sprite_recipe = self.document(animation_bindings['sprite_review'])
        actor_receipt = self.document(animation_bindings['sprite_manifest'])
        require(actor_receipt['recipe'] == sprite_recipe, 'Stale actor recipe')
        for key in ('scene', 'texture', 'lamp_texture', 'lamp_yaml', 'npc_script',
                    'character_sprite_script', 'emote_texture', 'emote_scene', 'shadow_texture', 'actor_scene'):
            self.source(sprite_recipe[key], sprite_recipe[key + '_sha256'])
        emote_text = self.text(sprite_recipe['emote_scene'])
        emote_node = node_block(emote_text, '.')
        emote_grid = [int(prop(emote_node, key)) for key in ('hframes', 'vframes')]
        resources = {}
        for binding in animation_bindings['assets']:
            size = image_size(self.source(sprite_recipe[binding['recipe_key']]))
            require(binding['id']==len(s['Resource'])+1,'Actor asset stable append order')
            resources[binding['role']] = self.add_resource(binding['path'], *size, *binding['grid'],
                expected=actor_receipt['outputs'][Path(binding['path']).name]['sha256'])
        layers_receipt = self.document('content/asset-receipts/graphics/world/house-layers/source.json')
        layer_data = self.document('reports/m2-scene-reference-reviewed/house-data.json', layers_receipt['scene_export_sha256'])
        self.document('reports/m2-scene-reference-reviewed/receipt.json', layers_receipt['reference_sha256'])
        self.document('reports/m2-scene-reference-reviewed/house-source.json', layers_receipt['source_receipt_sha256'])
        for path, expected in layers_receipt['sources'].items():
            self.source(path, expected)
        overlays = overlay_records(layer_data)
        require(layers_receipt == overlay_receipt(self.upstream, overlays, self.root / 'romfs/graphics/world/house-layers'),
                'Stale overlay bundle')
        # Preserve the established Resource indices/stable IDs used by the
        # separate audio bank. New resources are appended after the old slice.
        for role in ('objects',):
            resource = layers_receipt['resources'][role]
            size = image_size(self.source(resource['source'], resource['source_sha256']))
            resources[role] = self.add_resource('graphics/world/house-layers/' + resource['output'], *size, 1, 1,
                                                expected=resource['sha256'])
        for i, row in enumerate(overlays):
            if row['flags'] == 1:
                continue
            s['Overlay'].append(dict(stable_id=i + 1, resource_index=resources[row['resource']],
                **{k: row[k] for k in ('x', 'y', 'sort_y', 'u', 'v')},
                w=row['width'], h=row['height'], flags=row['flags']))
        self.record_map('Overlay', 'reports/m2-scene-reference-reviewed/house-data.json;'
                        'upstream/MOTHER-Encore/' + house_source + ';upstream/MOTHER-Encore/Tilesets/Interior.tres',
                        'Objects single-tile mode cells and Above atlas-mode cells; zero Z and later Above sibling order',
                        'Objects flags=0 uses source Y-sort origin; Above flags=1 is the fixed foreground pass; '
                        'atlas UV = region position + cell atlas coordinate * tile size; original RGBA pixels and offsets retained')
        map_receipt = self.document('content/asset-receipts/graphics/world/house-map/source.json')
        require(map_receipt['recipe'] == self.reviews['house-background'], 'Stale map recipe')
        for key in ('scene', 'texture'):
            self.source(map_receipt['recipe'][key], map_receipt['recipe'][key + '_sha256'])
        expected_tiles = map_layout(map_receipt['recipe'])
        require(len(map_receipt['tiles']) == len(expected_tiles), 'Incomplete map tile manifest')
        require(set(map_receipt['outputs']) == {'house-%02d.t3x' % t['index'] for t in expected_tiles},
                'Unknown/missing map resource outputs')
        for tile, expected in zip(map_receipt['tiles'], expected_tiles):
            require(all(tile.get(key) == value for key, value in expected.items()), 'Unreviewed map slice geometry/order')
            name = 'house-%02d.t3x' % tile['index']
            rid = self.add_resource('graphics/world/house-map/' + name, tile['width'], tile['height'], 1, 1,
                                    expected=map_receipt['outputs'][name]['sha256'])
            s['MapDraw'].append(dict(stable_id=tile['index'] + 1, resource_index=rid,
                x=tile['x'], y=tile['y'], w=tile['width'], h=tile['height'], flags=0))
        self.record_map('Resource/Overlay/MapDraw', 'content/asset-receipts/graphics/{actors,world/house-layers,world/house-map}/source.json',
                        'reviewed source layouts and asset-byte SHA256; PNG IHDR dimensions')

        # Build normal tracks directly from reviewed native-export inputs.
        ar = self.document('reports/m3-actor-reference-reviewed/receipt.json')
        anim_doc = self.document('reports/m3-actor-reference-reviewed/player-data.json', ar['reports_sha256']['player-data.json'])
        playback = self.document('reports/m3-actor-reference-reviewed/animation.json', ar['reports_sha256']['animation.json'])
        profile = animation_profile(anim_doc, self.reviews['ninten-animation'], playback)
        def add_clip(length, keys, flags=0, visibility=0, frame_count=0, channel=0):
            index = len(s['Clip']); first = len(s['Key'])
            s['Key'].extend(dict(time=f32(time), frame=int(frame)) for time, frame in keys)
            s['Clip'].append(dict(stable_id=index + 1, length=f32(length), first_key=first,
                key_count=len(keys), flags=flags, visibility=visibility, frame_count=frame_count, channel=channel))
            return index
        clip_names = {}
        visible = {None: 0, False: 1, True: 2}
        for i, clip in enumerate(profile['clips']):
            clip_names[clip['name']] = add_clip(clip['length'], clip['keys'], int(clip['loop']),
                visible[clip['main_visible']] | (visible[clip['special_visible']] << 2), profile['frame_count'])
            s['AnimationBinding'].append(dict(actor_profile_index=0, motion_state=i // 8, direction=i % 8, clip_index=i))
        self.record_map('Clip[0:32]/Key/AnimationBinding', 'reports/m3-actor-reference-reviewed/{player-data,animation}.json',
                        'character_animation.build; native decimal timelines; reviewed state/direction order',
                        'interval-discrete sampling; preserve out-of-duration Idle Right keys')
        # PyYAML is used only by this source extraction command, never the pack compiler.
        import yaml
        base_profiles=animation_bindings['base_profiles']
        lamp_profile=base_profiles[1];party_profile=base_profiles[0]
        lamp_yaml = yaml.safe_load(self.text(lamp_profile['animation_source']))
        party_yaml = yaml.safe_load(self.text(party_profile['animation_source']))
        for binding in lamp_profile['clips']:
            name,flags=binding['animation'],binding['flags']
            anim = lamp_yaml['animations'][name]
            require(anim['type'] == 1 and len(anim['directions']) == 1, 'Unreviewed lamp animation')
            direction = anim['directions'][0]; time = float(direction[0]); keys = []
            for frame, duration in direction[1:]:
                keys.append((time, frame - 1)); time += duration
            clip_names[world_clip_name(lamp_profile['prefix'],name)] = add_clip(time, keys, flags, frame_count=math.prod(lamp_yaml['size']))
        initial_emote=animation_bindings['emotes'][0]['animation']
        surprise = one(r'^\[sub_resource type="Animation" id=\d+\]\nresource_name = "'+re.escape(initial_emote)+r'"\n(.*?)(?=^\[|\Z)',
                       emote_text, 'surprise animation', re.M | re.S)[1]
        times = [float(n.strip()) for n in one(r'"times": PoolRealArray\( ([^)]*) \)', surprise, 'surprise times')[1].split(',')]
        frames = [int(n.strip()) for n in one(r'"values": \[ ([^]]*) \]', surprise, 'surprise frames')[1].split(',')]
        require(len(times) == len(frames), 'Emote timeline arity')
        clip_names[initial_emote] = add_clip(float(prop(surprise, 'length')), list(zip(times, frames)),
                                         8, frame_count=math.prod(emote_grid), channel=1)
        idle_directions = party_yaml['animations'][party_profile['idle_animation']]['directions']
        require(len(idle_directions) == 8 and all(len(d) == 2 for d in idle_directions), 'Unknown directional idle profile')
        s['DirectionFrame'] = [dict(frame=d[1][0] - 1) for d in idle_directions]
        self.record_map('Clip[32:35]/DirectionFrame', 'upstream/MOTHER-Encore/{Data/Animations/Lamp.yaml,Data/Animations/PartyMember.yaml,Nodes/Ui/emotes.tscn}',
                        'Lamp Idle/Open, PartyMember Idle, emotes surprise',
                        'YAML one-based frame to zero-based; durations accumulated in binary64 then stored binary32; Lamp Open absolute-latched, Idle synchronously finishes, surprise stops channel')

        actor_text = self.text(sprite_recipe['actor_scene'])
        sprite_position = pair(prop(node_block(actor_text, 'CharacterSprite'), 'position'))
        shadow_offset = pair(prop(node_block(actor_text, 'Shadow'), 'position'))
        emote_initial = int(prop(emote_node, 'frame'))
        bubble_extra = self.scalar(sprite_recipe['emote_scene'], r'object\.vframes \+ ' + NUMBER + r'\)', 'ActorProfile.emote_offset')
        sprite_code = self.text(sprite_recipe['character_sprite_script'])
        require('offset.y = -int(texture.get_height()/float(vframes*2))' in sprite_code,
                'Unreviewed sprite auto-offset formula')
        for binding in base_profiles:
            index=binding['actor_id']-1;data=yaml.safe_load(self.text(binding['animation_source']))
            rid = resources[self.world_bindings['actors'][index]['resource_role']]; resource = s['Resource'][rid]
            offset = [f32(data['offset'][0]), f32(-int(resource['height'] / float(resource['rows'] * 2)) + data['offset'][1])]
            emote_offset = [0, f32(-(resource['height'] / resource['rows'] + bubble_extra))]
            idle_clip = clip_names[world_clip_name(binding['prefix'],binding['idle_animation'],animation_bindings['directions'][0]if binding['execution_kind']==1 else None)]
            s['ActorProfile'].append(dict(stable_id=binding['actor_id'], execution_kind=binding['execution_kind'],
                flags=int(next(n for n in player['nodes'] if n['path'] == 'Shadow')['properties']['visible']) if index == 0 else int(sprite_recipe['lamp_layout']['shadow']),
                primary_resource=rid, shadow_resource=resources['shadow'], emote_resource=resources['emote'],
                animation_binding_first=0 if index == 0 else len(s['AnimationBinding']),
                animation_binding_count=len(s['AnimationBinding']) if index == 0 else 0,
                initial_frame=s['DirectionFrame'][0]['frame'] if index == 0 else lamp_yaml['animations'][lamp_profile['idle_animation']]['directions'][0][1][0] - 1,
                emote_initial_frame=emote_initial, sprite_position=sprite_position, sprite_offset=offset,
                emote_offset=emote_offset, shadow_offset=shadow_offset, direction_first=0,
                direction_count=len(s['DirectionFrame']) if index == 0 else 0,
                idle_clip=idle_clip, emote_clip=clip_names[initial_emote] if index == 0 else NONE))
        intro_source = 'Maps/Cutscenes/Mt Itoi Landscape.tscn'
        intro_door = node_block(self.text(intro_source), 'Objects/Door')
        door_y = self.scalar('Scripts/Main/Door.gd', r'Vector2\(targetX, targetY - ' + NUMBER + r'\)', 'Scene.spawn.y.adjustment', 1)
        spawn = [f32(float(prop(intro_door, 'targetX'))), f32(float(prop(intro_door, 'targetY')) - door_y)]
        direction = pair(prop(intro_door, 'dir'))
        npc_default = one(r'initial_dir = (Vector2\([^\n]+\))', self.text('Scripts/Main/npc.gd'), 'NPC initial direction')[1]
        lamp_direction = pair(npc_default)
        lamp_node=world_actor(self.world_bindings,2)['node']
        instance_visibility = [player['nodes'][0]['properties']['visible'],
                               all(hn[path]['properties'].get('visible', True)
                                   for path in ('.',lamp_node.rsplit('/',1)[0],lamp_node))]
        require(all(type(value) is bool for value in instance_visibility), 'Invalid native actor visibility')
        for binding in base_profiles:
            i=binding['actor_id']-1;name=Path(sprite_recipe['texture']).parent.name if binding['execution_kind']==1 else hn[world_actor(self.world_bindings,binding['actor_id'])['node']]['name']
            s['ActorInstance'].append(dict(stable_id=binding['actor_id'], profile_index=i, binding_kind=binding['execution_kind'], flags=int(instance_visibility[i]),
                display_name_string=self.string(name), position=spawn if i == 0 else list(vec(hn[lamp_node]['properties']['position'])),
                direction=direction if i == 0 else lamp_direction,
                initial_clip=s['ActorProfile'][i]['idle_clip']))
        self.record_map('ActorInstance[0].flags', player_path,
                        'nodes[path=.] properties.visible', 'bit0 initially_visible from native Player root visibility')
        self.record_map('ActorInstance[1].flags', house_path,
                        'nodes[path=.], nodes[path=Objects], nodes[path=Objects/lamp] properties.visible',
                        'bit0 initially_visible is the AND of native lamp and all ancestor CanvasItem visibility')
        self.record_map('ActorProfile', 'upstream/MOTHER-Encore/{Nodes/Reusables/actor.tscn,Scripts/Main/character_sprite.gd,Nodes/Ui/emotes.tscn,Data/Animations/PartyMember.yaml,Data/Animations/Lamp.yaml}',
                        'actor position/shadow; auto sprite offset; emote bubble offset; native sprite/frame defaults',
                        'execution_kind 1=directional proxy, 2=scripted animation proxy; stable identity lives separately')
        self.record_map('ActorInstance/Scene.spawn', 'upstream/MOTHER-Encore/' + intro_source + ';upstream/MOTHER-Encore/Scripts/Main/Door.gd;' + house_path,
                        'Objects/Door targetX,targetY,dir; change_scene targetY adjustment; Objects/lamp native position')

        for n in house['nodes']:
            if n['class'] == 'Area2D' and n['path'].startswith('Limits/camarea'):
                owners_ = n['physics_shape_owners']
                require(len(owners_) == 1 and len(owners_[0]['shapes']) == 1, 'Unreviewed camera shape owners')
                owner = owners_[0]; resource = hr[owner['shapes'][0]['id']]
                require(resource['class'] == 'RectangleShape2D', 'Unreviewed camera shape')
                center = transform(n['world_transform'], vec(owner['transform']['origin']))
                s['CameraArea'].append(dict(stable_id=len(s['CameraArea']) + 1, flags=0,
                                            center=list(center), extents=list(vec(resource['properties']['extents']))))
        require(len(s['CameraArea']) == 8, 'Unreviewed camera area count')
        self.record_map('CameraArea', house_path, 'Limits/camarea* rectangle shape centers/extents; preserve native node order')

        global_source = 'Scripts/global/globalData.gd'
        global_text = self.text(global_source)
        flag_block = one(r'func _init_flags\(\):\s+var flag_names := \[(.*?)\n\s*\]', global_text, 'flag registry', re.S)[1]
        flags = re.findall(r'^\s*"([^"\n]+)"', flag_block, re.M)
        require(len(flags) == 178 and len(set(flags)) == len(flags), 'Unreviewed flag registry')
        flag_receipt = self.document('reports/m4-world-flags-reference-final/receipt.json')
        flags_ref = self.document('reports/m4-world-flags-reference-final/world-flags.json', flag_receipt['outputs_sha256']['world-flags.json'])
        require(flags == flags_ref['flag_names'], 'Source flag registry differs from native reference')
        area_text = self.text('Scripts/Main/RoomTypes/AreaRoom.gd')
        region = json.loads(prop(node_block(house_text, '.'), '_region_name'))
        visit_block = one(r'const REGION_VISIT_FLAGS := \{(.*?)^\}', area_text, 'region visit flags', re.M | re.S)[1]
        visited = one(r'^\s*"' + re.escape(region) + r'": "([^"\n]+)"', visit_block, 'visit flag')[1]
        for i, name in enumerate(flags):
            s['Flag'].append(dict(stable_id=i + 1, name_string=self.string(name), default_value=0,
                                  flags=int(name == visited)))
        s['InitialFlag'] = [dict(flag_index=flags.index(visited), value=1)]
        trigger_path = 'Poltergeist/Cutscene Area'
        trigger = hn[trigger_path]; owner = trigger['physics_shape_owners'][0]
        trigger_points = [transform(trigger['world_transform'], transform(owner['transform'], p))
                          for p in shape_points(hr[owner['shapes'][0]['id']])]
        trigger_first = len(s['Vec2']); s['Vec2'].extend(dict(x=x, y=y) for x, y in trigger_points)
        for path in (trigger_path, trigger_path.rsplit('/', 1)[0]):
            flag = json.loads(prop(node_block(house_text, path), 'disappear_flag'))
            s['Condition'].append(dict(flag_index=flags.index(flag), expected_value=0, domain=0))
        s['Trigger'] = [dict(stable_id=1, first_vertex=trigger_first, vertex_count=len(trigger_points),
            flags=0, condition_first=0, condition_count=len(s['Condition']), program_index=0, actor_instance_index=0)]
        self.record_map('Flag/InitialFlag', 'upstream/MOTHER-Encore/' + global_source + ';upstream/MOTHER-Encore/Scripts/Main/RoomTypes/AreaRoom.gd',
                        '_init_flags false defaults; native-reference order; scene region visit flag')
        self.record_map('Trigger/Condition', house_path + ';upstream/MOTHER-Encore/' + house_source,
                        trigger_path + ' world shape; own and ancestor disappear flags')

        dialogue_review = self.reviews['lamp-dialogue']
        verify_sources(self.upstream, dialogue_review)
        receipt = self.document('reports/m5-lamp-dialogue-reference/dialogue.json')
        actions = compile_receipt(receipt, dialogue_review)
        music_node = node_block(house_text, self.world_bindings['initial_bindings'][0]['node'])
        music_uri = 'res://Audio/Music/' + json.loads(prop(music_node, 'loop'))
        resources['music'] = self.add_resource(music_uri, kind=2)
        sound_uris = list(dict.fromkeys(a['text'] for a in actions if a['kind'] == 'PlaySound'))
        sound_resources = {uri: self.add_resource(uri, kind=2) for uri in sound_uris}
        delay = self.scalar('Scripts/Main/roomshaker.gd', r'func delayed_start\(time = ' + NUMBER + r'\)', 'Binding[1].duration')
        shaker_source = 'Scripts/Main/roomshaker.gd'
        shaker = self.text(shaker_source)
        shaker_scene = self.text('Nodes/Reusables/roomshaker.tscn')
        shaker_override = node_block(house_text, 'Room Shaker')
        require(shaker_override.strip() == 'direction = ' + prop(shaker_override, 'direction'),
                'Unreviewed room shaker scene overrides')
        shake_direction = pair(prop(shaker_override, 'direction'))
        require('export (bool) var auto_start = false' in shaker and
                not node_block(shaker_scene, 'Timer').strip(), 'Unreviewed room shaker Timer defaults')
        for fragment in ['if timer.time_left == 0:', '_on_Timer_timeout()', 'timer.start()',
                         'global.currentCamera.shake_camera(magnitude, length, direction)',
                         'audioPlayer.play()', 'timer.wait_time = rand_range(wait_time - wait_margin, wait_time + wait_margin)',
                         'if !uiManager.is_in_battle() and !uiManager.is_game_over() and global.get_player().get_state() != global.get_player().CAMERA:']:
            require(fragment in shaker, 'Unreviewed periodic camera shake mechanism: ' + fragment)
        require('[connection signal="timeout" from="Timer" to="." method="_on_Timer_timeout"]' in shaker_scene,
                'Unreviewed room shaker timeout signal')
        magnitude = self.scalar(shaker_source, r'^export \(float\) var magnitude = ' + NUMBER + r'\s*$', 'Binding[1].value')
        sound = json.loads(one(r'^export \(String\) var sound = ("[^"\n]+")', shaker, 'room shaker sound')[1])
        shake_sound = self.add_resource('res://Audio/Sound effects/' + sound, kind=2)
        initial_targets={'music':resources['music'],'shake_sound':shake_sound}
        s['Binding']=[dict(stable_id=binding['id'],kind=binding['kind'],flags=0,target_index=initial_targets[binding['target']],auxiliary_index=NONE,value=magnitude if binding['kind']==3 else 0,duration=delay if binding['kind']==3 else 0)for binding in self.world_bindings['initial_bindings']]
        self.record_map('Binding[1]/Resource[roomshake]', 'upstream/MOTHER-Encore/' + shaker_source + ';upstream/MOTHER-Encore/Nodes/Reusables/roomshaker.tscn',
                        'delayed_start default; magnitude/sound exports; native repeating idle Timer defaults',
                        'PeriodicCameraShake binding with sound resource index; scheduler owns guarded timeout and source RNG sequence')
        battle_actions = [a for a in actions if a['kind'] == 'QueueBattle']
        require(len(battle_actions) == 1, 'Unreviewed battle request count')
        battle = battle_actions[0]
        dialogue_code = self.text('Scripts/UI/DialogueBox.gd')
        battle_args = one(r'uiManager\.start_battle\((-?\d+), (true|false), \[\], _post_battle_cutscenes, _battle_win_flag\)',
                          dialogue_code, 'battle request arguments')
        encounter_binding=world_encounter(self.world_bindings,'initial')
        s['Battle'] = [dict(stable_id=encounter_binding['id'], enemy_string=self.string(battle['text']), actor_instance_index=encounter_binding['actor_id']-1,
            win_flag_index=flags.index(battle['detail']), advantage=int(battle_args[1]),
            win_cutscene_string=0, battle_resource_index=self.add_resource(encounter_binding['resource_path'],kind=3),
            flags=int(battle_args[2] == 'true') | (2 if any(a['kind'] == 'OverworldBattleMusic' and a['value'] for a in actions) else 0))]
        actor_indexes = world_actor_indices(self.world_bindings)
        binding_indexes = world_object_indices(self.world_bindings,'initial')
        for a in actions:
            target = NONE
            if a['kind'] == 'CallObjectDeferred': target = binding_indexes[a['text'], a['detail']]
            elif a['kind'] == 'PlaySound': target = sound_resources[a['text']]
            elif a['kind'] == 'AnimateActor': target = clip_names[world_clip_name(a['actor'],a['text'])]
            elif a['kind'] == 'EmoteActor': target = clip_names[a['text']]
            elif a['kind'] in ('QueueBattle', 'RequestBattle'): target = encounter_binding['id']-1
            elif a['kind'] == 'MoveCamera': target = 1 # reviewed schema SineOut
            s['Command'].append(dict(opcode=OPCODES[a['kind']], actor_index=actor_indexes[a['actor']], phrase=a['phrase'],
                target_index=target, flags=0, vector=[f32(v) for v in a['vector']],
                value=a['value'] if a['kind'] != 'AnimateActor' else 0,
                duration=a['duration'], auxiliary_index=NONE))
        s['Program'] = [dict(stable_id=world_program(self.world_bindings,'initial')['id'], first_command=0, command_count=len(actions), phrase_count=len(receipt['dialogue']))]
        self.record_map('Program/Command/Binding/Battle', 'reports/m5-lamp-dialogue-reference/dialogue.json;compatibility/reviews/lamp-dialogue-v0410.json',
                        'lamp_dialogue.compile_receipt reviewed native YAML parser receipt and fixed handler order',
                        'source-only names resolved offline to explicit clip/resource/actor/binding/battle indices; no runtime string dispatch')
        self.record_map('Battle.flags/advantage', 'upstream/MOTHER-Encore/Scripts/UI/DialogueBox.gd',
                        'start_battle arguments + ovbattlemusic reviewed YAML field')
        self.extend_doll(hn,resources,clip_names,add_clip,sprite_position,shadow_offset,emote_initial,bubble_extra,flags,battle_args)
        foreground_resource = layers_receipt['resources']['above']
        size = image_size(self.source(foreground_resource['source'], foreground_resource['source_sha256']))
        resources['above'] = self.add_resource('graphics/world/house-layers/' + foreground_resource['output'], *size, 1, 1,
                                              expected=foreground_resource['sha256'])
        for i, row in enumerate(overlays):
            if row['flags'] == 1:
                s['Overlay'].append(dict(stable_id=i + 1, resource_index=resources['above'],
                    **{k: row[k] for k in ('x', 'y', 'sort_y', 'u', 'v')},
                    w=row['width'], h=row['height'], flags=row['flags']))
        self.extend_doll_postwin(hn,resources,clip_names,add_clip,shadow_offset,emote_initial,bubble_extra,flags)

        rules = [
            (1, 1, 'Scripts/Main/party/Player.gd', r'^const SPEED_WALKING := ' + NUMBER + r'$'),
            (2, 1, 'Scripts/Main/party/Player.gd', r'^const SPEED_RUNNING := ' + NUMBER + r'$'),
            (3, 1, 'Scripts/Main/party/Player.gd', r'\(_speed/' + NUMBER + r'\)'),
            (4, 1, 'Scripts/Main/actor.gd', r'var angle = ' + NUMBER + r' \* sign\(_direction.angle_to\(newDir\)\)'),
            (5, 1, 'Scripts/Main/actor.gd', r'^var speed: float = ' + NUMBER + r'$'),
            (6, 2, 'Scripts/Main/actor.gd', r'_sprite_position.y - height\), length \* ' + NUMBER + r'\)'),
            (7, 2, 'Scripts/Main/actor.gd', r'_sprite_position.y\), length \* ' + NUMBER + r'\)'),
            (8, 2, 'Scripts/Main/actor.gd', r'for i in int\(length \* ' + NUMBER + r'\)'),
            (9, 2, 'Scripts/Main/actor.gd', r'character_sprite.offset = oldOffset \+ offset\n\s*yield\(get_tree\(\).create_timer\(' + NUMBER + r'\)'),
            (10, 2, 'Scripts/UI/DialogueBox.gd', r'var turner = _actors\[i\]\n\s*var direction = Vector2.ZERO\n\s*var speed = ' + NUMBER),
            (11, 2, 'Scripts/Main/actor.gd', r'global.get_player\(\).camera.return_camera\(' + NUMBER + r'\)'),
            (12, 2, 'Scripts/UI/DialogueBox.gd', r'"small":\n\s*global.start_joy_vibration\([^\n]+\)\n\s*global.currentCamera.shake_camera\(' + NUMBER),
            (13, 2, 'Scripts/Main/Camera2D.gd', r'^const SHAKE_STEP_TIME := ' + NUMBER + r'$'),
            (14, 2, 'Scripts/Main/Camera2D.gd', r'lerp_weight := ' + NUMBER + r', diminish := true'),
            (15, 2, 'Scripts/misc/Shaker.gd', r'var new_magnitude = max\(_shake_magnitude, ' + NUMBER + r'\)'),
            (16, 2, 'Scripts/misc/Shaker.gd', r'if _shake_magnitude <= ' + NUMBER + r' and _shake_side == -1:'),
            (17, 2, 'Scripts/Main/Camera2D.gd', r'final_tween.tween_property\(self, "_shake_offset", Vector2.ZERO, ' + NUMBER + r'\)'),
            (18, 2, 'Scripts/UI/DialogueBox.gd', r'var cam_pos = global.currentCamera.global_position\n\s*var time = ' + NUMBER),
            (19, 2, 'Scripts/UI/DialogueBox.gd', r'var size = str\(_curr_phrase\["shakecam"\][^\n]*\n\s*var length = ' + NUMBER),
        ]
        for key, scalar_type, source, pattern in rules:
            value = self.scalar(source, pattern, 'Rule[' + str(key) + ']', scalar_type)
            s['Rule'].append(dict(key=key, scalar_type=scalar_type, value=value))
        s['Rule'].append(dict(key=20, scalar_type=1, value=real(player['nodes'][0]['properties']['collision/safe_margin'])))
        self.record_map('Rule[20]', player_path, 'nodes[0].properties[collision/safe_margin]', 'native real_t exact decimal to binary32')
        for key, name in [(21, 'wait_time'), (22, 'wait_margin'), (23, 'length')]:
            value = self.scalar(shaker_source, r'^export \(float\) var ' + name + r' = ' + NUMBER + r'\s*$', 'Rule[' + str(key) + ']')
            s['Rule'].append(dict(key=key, scalar_type=2, value=value))
        for key, value in zip([24, 25], shake_direction):
            s['Rule'].append(dict(key=key, scalar_type=2, value=value))
        self.record_map('Rule[24:25]', 'upstream/MOTHER-Encore/' + house_source,
                        'Room Shaker.direction scene override', 'preserve source Vector2 components; no runtime content constant')
        progression = self.document('reports/m1-progression-linux-reference.json')
        validate_progression(progression, self.reviews['progression'])
        self.source(self.reviews['progression']['source'], self.reviews['progression']['source_sha256'])
        cap = self.reviews['progression']['level_cap']
        s['Experience'] = [dict(required_total_exp=value) for level, value in progression['level_cases'][:cap]]
        self.record_map('Experience', 'reports/m1-progression-linux-reference.json', 'level_cases levels 1 through reviewed level_cap',
                        'measured original Godot functions; no new formula or changed goldens')
        s['Scene'] = [dict(stable_id=1, display_name_string=self.string(hn['.']['name']), version_string=self.string(self.lock['game_version']),
            source_scene_string=self.string('res://' + house_source), player_instance_index=0, initial_program_index=NONE,
            actor_hull_first=0, actor_hull_count=len(actor_hull), spawn=spawn, start_direction=direction,
            initial_motion_state=0, initial_frame=profile['clips'][0]['keys'][0][1], initial_flag_first=0,
            initial_flag_count=len(s['InitialFlag']), body_rule_first=0, body_rule_count=len(s['BodyRule']),
            default_camera_area=0, rule_profile_id=1, flags=0)]
        self.record_map('Scene', house_path + ';upstream.lock', 'native root name, pin game version; source-backed actor spawn; scoped initial animation bindings',
                        'all IDs explicitly persisted in native-opening.json; local indices are not save identities')
        prefix=[row for row in self.world_bindings['programs']if row['stage']not in('melody','guard')]
        require(len(s['Program'])==len(prefix),'World program prefix coverage mismatch')
        for record,binding in zip(s['Program'],prefix):
            require(record['stable_id']==binding['id'],'World program stable order mismatch')
            self.source('Data/Dialogue/'+binding['path']+'.yaml')
            record['source_path_string']=self.string(binding['path'])
        # Append after all established rows AND strings, preserving identities.
        self.extend_melody(flags)
        from tools.link_phone_content import link_room
        link_room(self)
        from tools.link_pillow_content import append_room
        append_room(self,clip_names,add_clip)
        from tools.link_family_followup import append_room as append_family_room
        append_family_room(self,clip_names,add_clip)
        from tools.storage_dialogue import append_room as append_storage_room
        append_storage_room(self)
        from tools.link_basement_content import append_room as append_basement_room
        append_basement_room(self)
        for module in ('world_geometry', 'house_layers', 'character_animation', 'lamp_dialogue', 'doll_dialogue', 'doll_postwin', 'melody_dialogue', 'reference_animation', 'scene_data', 'reference_progression', 'upstream', 'map_asset', 'world_program_bindings'):
            self.file('tools/' + module + '.py')
        self.file('tools/extract_native_content.py')
        self.file('tools/programme_lowering_recipe.py')
        self.file('content/programme-lowering-recipe.json')
        source_map = dict(schema=1, upstream_commit=self.lock['commit'], scope='Existing scoped reviews only; not whole game/scene/script approval',
                          fields=self.mapping, stable_namespaces=dict(body=body_ids, owner=owner_ids,
                              clips={name: index + 1 for name, index in clip_names.items()}))
        ir = dict(schema=1, family=0x454e0002, rules=8, capabilities=9, scene_id=1,
                  upstream_commit=self.lock['commit'], exporter_version=1, adapter_revision=9,
                  strings=self.strings, sections=s, provenance=dict(sources=self.sources, projections=self.projections,
                      notes=['Baseline extraction from original source and existing scoped native exports/reviews; no new broad source approval.',
                             'C++ sources and generated headers are never extraction dependencies.',
                             'Audio entries are request-only upstream URIs; no audio backend or battle implementation claimed.',
                             'Float32 values retain native rounding; detailed field selectors are in native-source-map.json.']))
        return ir, source_map

    def extend_doll(self,hn,resources,clip_names,add_clip,sprite_position,shadow_offset,emote_initial,bubble_extra,flags,battle_args):
        s=self.sections;native=doll_receipt(self);doc=native['yaml'][0]
        presentation=self.document('content/native-house-presentation.json')
        npc_bindings=[row for row in self.world_bindings['npc_profiles']if row['phase']=='attack']
        for binding in npc_bindings:
            actor_binding=world_actor(self.world_bindings,binding['actor_id']);role=actor_binding['resource_role']
            candidates=[r for r in presentation['resources']if r['role']==role]
            require(len(candidates)==1,'Doll source resource identity')
            r=candidates[0];resources[role]=self.add_resource(r['path'],r['width'],r['height'],r['columns'],r['rows'],expected=r['sha256'])
            source_node=node_block(self.text(self.world_bindings['scene']),actor_binding['node'])
            self.source('Graphics/Character Sprites/'+json.loads(prop(source_node,'sprite'))+'.png')
        def add_native(name,clip,frames,extra=0,channel=0):
            flags_=int(clip['loop'])|extra|(16 if clip['keys'][0][0]>0 else 0)
            clip_names[name]=add_clip(clip['length'],clip['keys'],flags_,frame_count=frames,channel=channel)
            return clip_names[name]
        # Player AnimationPlayer tracks remain unchanged. Actor replacement uses
        # a distinct motion selector for its native PartyMember YAML Walk tracks.
        directions=self.world_bindings['animation']['directions'];walk=self.world_bindings['animation']['actor_walk']
        for d,name in enumerate(directions):
            c=add_native(world_clip_name(walk['prefix'],walk['animation'],name),native['animations'][walk['animation_index']][world_clip_name('',walk['animation'],name)],math.prod(native['yaml'][walk['yaml_index']]['size']))
            s['AnimationBinding'].append(dict(actor_profile_index=walk['actor_id']-1,motion_state=walk['motion'],direction=d,clip_index=c))
        s['ActorProfile'][0]['animation_binding_count']=len(s['AnimationBinding'])
        s['ActorProfile'][1]['animation_binding_first']=len(s['AnimationBinding'])
        for binding in npc_bindings:
            actor_binding=world_actor(self.world_bindings,binding['actor_id']);role=actor_binding['resource_role'];profile_index=actor_binding['id']-1;nodepath=actor_binding['node'];alias=actor_binding['alias']
            yaml_index=binding['yaml_index'];animation_index=binding['animation_index']
            require(profile_index==len(s['ActorProfile']),'NPC profile stable append order mismatch')
            yaml=native['yaml'][yaml_index];frames=math.prod(yaml['size']);r=s['Resource'][resources[role]]
            native_clips=native['animations'][animation_index];first_binding=len(s['AnimationBinding']);first_direction=len(s['DirectionFrame'])
            if binding['execution_kind']==2:
                # Source Actor.play_anim emits finished_action immediately for
                # the idle animation. Initialization only selects the clip and
                # therefore does not emit this play-command event.
                for clip_binding in binding['clips']:
                    name=world_clip_name(alias,clip_binding['animation']);index=add_native(name,native_clips[clip_binding['animation']],frames,clip_binding['flags'])
                    if clip_binding['animation']==binding['idle_animation']:idle=index
                direction_count=0
            else:
                for clip_binding in binding['clips']:
                    animation=clip_binding['animation'];motion=world_motion(self.world_bindings,animation)
                    for d,name in enumerate(directions[:len(yaml['animations'][animation]['directions'])]):
                        c=add_native(world_clip_name(alias,animation,name),native_clips[world_clip_name('',animation,name)],frames,clip_binding['flags'])
                        s['AnimationBinding'].append(dict(actor_profile_index=profile_index,motion_state=motion,direction=d,clip_index=c))
                idle=clip_names[world_clip_name(alias,binding['idle_animation'],directions[0])];direction_count=len(yaml['animations'][binding['idle_animation']]['directions'])
                s['DirectionFrame'].extend(dict(frame=int(direction[1][0])-1)for direction in yaml['animations'][binding['idle_animation']]['directions'])
            node=hn[nodepath];properties=node['properties'];source_position=pair(prop(node_block(self.text(self.world_bindings['animation']['npc_scene']),'CharacterSprite'),'position'))
            offset=[f32(yaml['offset'][0]),f32(-int(r['height']/float(r['rows']*2))+yaml['offset'][1])]
            source_node=node_block(self.text(self.world_bindings['scene']),nodepath)
            no_shadow=bool(re.search(r'^no_shadow = true$',source_node,re.M))
            s['ActorProfile'].append(dict(stable_id=profile_index+1,execution_kind=binding['execution_kind'],flags=int(not no_shadow),
                primary_resource=resources[role],shadow_resource=resources['shadow'],emote_resource=resources['emote'],
                animation_binding_first=first_binding,animation_binding_count=len(s['AnimationBinding'])-first_binding,
                initial_frame=s['Key'][s['Clip'][idle]['first_key']]['frame'],emote_initial_frame=emote_initial,sprite_position=source_position,
                sprite_offset=offset,emote_offset=[0,f32(-(r['height']/r['rows']+bubble_extra))],shadow_offset=shadow_offset,
                direction_first=first_direction,direction_count=direction_count,idle_clip=idle,emote_clip=NONE))
            s['ActorInstance'].append(dict(stable_id=profile_index+1,profile_index=profile_index,binding_kind=2,
                flags=int(all(hn[p]['properties'].get('visible',True)for p in('.', 'Objects',nodepath))),display_name_string=self.string(node['name']),
                position=list(vec(properties['position'])),direction=[0,1],initial_clip=idle))
        emote_binding=next(row for row in self.world_bindings['animation']['emotes']if row['phase']=='attack')
        add_native(emote_binding['animation'],native[emote_binding['receipt_key']],s['Resource'][resources['emote']]['columns']*s['Resource'][resources['emote']]['rows'],8,1)
        s['ActorProfile'][emote_binding['actor_id']-1]['emote_clip']=clip_names[emote_binding['animation']]
        stop_binding=self.world_bindings['stop_binding']
        s['Binding'].append(dict(stable_id=stop_binding['id'],kind=stop_binding['kind'],flags=0,target_index=stop_binding['target_binding_id']-1,auxiliary_index=NONE,value=0,duration=0))
        encounter_binding=world_encounter(self.world_bindings,'attack')
        battle=doc[encounter_binding['phrase']]['startbattle']
        require(all('ovbattlemusic'not in p for p in doc.values()),'Doll must inherit original overworld battle music state')
        s['Battle'].append(dict(stable_id=encounter_binding['id'],enemy_string=self.string(next(iter(battle['battlers'][0]))),actor_instance_index=encounter_binding['actor_id']-1,
            win_flag_index=NONE,advantage=int(battle_args[1]),flags=int(battle_args[2]=='true')|4|8,
            win_cutscene_string=self.string(battle['wincutscene']),battle_resource_index=self.add_resource(encounter_binding['resource_path'],kind=3)))
        actor_indices=world_actor_indices(self.world_bindings);first_command=len(s['Command'])
        commands=compile_doll_dialogue(doc)
        for a in commands:
            kind=a['kind'];target=NONE
            if kind=='MoveActorPath':
                p=a['path'];target=len(s['MovementPath']);first=len(s['MovementEntry'])
                for e in p['movement']:
                    s['MovementEntry'].append(dict(kind=int('wait'in e),vector=[0,0]if'wait'in e else[f32(e['x']),f32(e['y'])],duration=e.get('wait',0)))
                s['MovementPath'].append(dict(stable_id=target+1,first_entry=first,entry_count=len(p['movement']),flags=int(p['type']=='step')|int(p.get('moonwalk',False))*2,animation_motion=world_motion(self.world_bindings,p.get('animation')),speed=p['speed']))
            elif kind in('AnimateActor','EmoteActor'):target=clip_names[a['clip']]
            elif kind in('QueueBattle','RequestBattle'):target=encounter_binding['id']-1
            elif kind=='SetFlag':target=flags.index(a['flag'])
            elif kind=='ShowDialogue':target=a['dialogue_id']
            elif kind=='CallObjectDeferred':target=a['binding']
            s['Command'].append(dict(opcode=OPCODES[kind],actor_index=actor_indices[a['actor']],phrase=a['phrase'],target_index=target,flags=0,
                vector=[f32(v)for v in a.get('vector',[0,0])],value=a.get('value',0),duration=a.get('duration',0),auxiliary_index=NONE))
        s['Program'].append(dict(stable_id=world_program(self.world_bindings,'attack')['id'],first_command=first_command,command_count=len(commands),phrase_count=len(doc)))
        self.record_map('Doll Program/MovementPath/MovementEntry/Battle','upstream/MOTHER-Encore/Data/Dialogue/Podunk/cutscenes/doll_attack.yaml;'+ 'reports/doll-sequence/native-parser-animation.json',
            'all seven native-parser phrases in DialogueBox handler order; actors dictionary order retained',
            'source asynchronous waits are path entries; step target resolved when entry starts; flag set before queued battle and final wait; no win flag invented; keepAfterBattle and win cutscene preserved; absent ovbattlemusic inherits current source global state through typed Battle.flags bit8')
        self.record_map('Doll/Mimmie ActorProfile/AnimationBinding/Clip/Key','reports/doll-sequence/native-parser-animation.json',
            'original CharacterSprite._create_animations native Animation tracks; native exclamation resource keys',
            'exact native real decimals, original texture dimensions and sprite offsets; Actor Walk motion4 distinct from Player Walk; Talk motion5 and delayed-first-key flag16')

    def extend_doll_postwin(self,hn,resources,clip_names,add_clip,shadow_offset,emote_initial,bubble_extra,flags):
        s=self.sections;native=postwin_receipt(self);doc=native['yaml'][0]
        require(len(s['Resource'])==28 and len(s['ActorInstance'])==4 and len(s['Program'])==2,'Post-win append-only namespace')
        presentation=self.document('content/native-house-presentation.json')
        npc_bindings=[row for row in self.world_bindings['npc_profiles']if row['phase']=='postwin']
        require(len(npc_bindings)==1,'Post-win NPC profile coverage')
        binding=npc_bindings[0];actor_binding=world_actor(self.world_bindings,binding['actor_id']);alias=actor_binding['alias'];role=actor_binding['resource_role']
        candidates=[r for r in presentation['resources']if r['role']==role]
        require(len(candidates)==1,'Minnie source resource identity')
        r=candidates[0];resources[role]=self.add_resource(r['path'],r['width'],r['height'],r['columns'],r['rows'],expected=r['sha256'])
        sound='res://Audio/Sound effects/'+doc['0']['soundeffect'];resources['postwin_sound']=self.add_resource(sound,kind=2)
        nodepath=actor_binding['node'];require(doc['0']['actors'][actor_binding['source_alias']]==nodepath,'Post-win actor node mismatch');source_node=node_block(self.text(self.world_bindings['scene']),nodepath)
        require('Graphics/Character Sprites/'+json.loads(prop(source_node,'sprite'))+'.png'==binding['texture_source'],'Post-win source texture binding')
        self.source(binding['texture_source'])
        require(not re.search(r'^yaml = ',source_node,re.M),'Minnie inherited animation changed')
        yaml=native['yaml'][binding['yaml_index']];frames=math.prod(yaml['size']);profile_index=actor_binding['id']-1
        require(profile_index==len(s['ActorProfile']),'Post-win profile stable append order mismatch')
        def add_native(name,clip,channel=0):
            clip_names[name]=add_clip(clip['length'],clip['keys'],int(clip['loop'])|(8 if channel else 0)|(16 if clip['keys'][0][0]>0 else 0),frame_count=frames if not channel else s['Resource'][resources['emote']]['columns']*s['Resource'][resources['emote']]['rows'],channel=channel)
            return clip_names[name]
        first_binding=len(s['AnimationBinding']);first_direction=len(s['DirectionFrame'])
        directions=self.world_bindings['animation']['directions']
        for clip_binding in binding['clips']:
            animation=clip_binding['animation'];motion=world_motion(self.world_bindings,animation)
            for d,name in enumerate(directions[:len(yaml['animations'][animation]['directions'])]):
                clip=add_native(world_clip_name(alias,animation,name),native['animations'][binding['animation_index']][world_clip_name('',animation,name)])
                s['AnimationBinding'].append(dict(actor_profile_index=profile_index,motion_state=motion,direction=d,clip_index=clip))
        idle=clip_names[world_clip_name(alias,binding['idle_animation'],directions[0])]
        s['DirectionFrame'].extend(dict(frame=int(direction[1][0])-1)for direction in yaml['animations'][binding['idle_animation']]['directions'])
        source_position=pair(prop(node_block(self.text(self.world_bindings['animation']['npc_scene']),'CharacterSprite'),'position'))
        node=hn[nodepath];offset=[f32(yaml['offset'][0]),f32(-int(r['height']/float(r['rows']*2))+yaml['offset'][1])]
        require(not re.search(r'^no_shadow = true$',source_node,re.M),'Minnie shadow binding changed')
        s['ActorProfile'].append(dict(stable_id=profile_index+1,execution_kind=binding['execution_kind'],flags=1,primary_resource=resources[role],
            shadow_resource=resources['shadow'],emote_resource=resources['emote'],animation_binding_first=first_binding,
            animation_binding_count=len(s['AnimationBinding'])-first_binding,initial_frame=s['Key'][s['Clip'][idle]['first_key']]['frame'],
            emote_initial_frame=emote_initial,sprite_position=source_position,sprite_offset=offset,
            emote_offset=[0,f32(-(r['height']/r['rows']+bubble_extra))],shadow_offset=shadow_offset,
            direction_first=first_direction,direction_count=4,idle_clip=idle,emote_clip=NONE))
        s['ActorInstance'].append(dict(stable_id=profile_index+1,profile_index=profile_index,binding_kind=2,
            flags=int(all(hn[p]['properties'].get('visible',True)for p in('.', 'Objects',nodepath))),display_name_string=self.string(node['name']),
            position=list(vec(node['properties']['position'])),direction=[0,1],initial_clip=idle))
        emote_binding=next(row for row in self.world_bindings['animation']['emotes']if row['phase']=='postwin')
        add_native(emote_binding['animation'],native[emote_binding['receipt_key']],1)
        end_duration=postwin_return_duration(self.text('Scripts/UI/DialogueBox.gd'))
        commands=compile_postwin_dialogue(doc,end_duration);first_command=len(s['Command'])
        actor_indices=world_actor_indices(self.world_bindings)
        for a in commands:
            kind=a['kind'];target=NONE
            if kind=='MoveActorPath':
                p=a['path'];target=len(s['MovementPath']);first=len(s['MovementEntry'])
                for e in p['movement']:s['MovementEntry'].append(dict(kind=0,vector=[f32(e['x']),f32(e['y'])],duration=0))
                s['MovementPath'].append(dict(stable_id=target+1,first_entry=first,entry_count=len(p['movement']),flags=int(p['type']=='step'),animation_motion=world_motion(self.world_bindings,p.get('animation')),speed=p['speed']))
            elif kind in('AnimateActor','EmoteActor'):target=clip_names[a['clip']]
            elif kind=='PlaySound':target=resources['postwin_sound']
            elif kind=='SetFlag':target=flags.index(a['flag'])
            elif kind=='ShowDialogue':target=a['dialogue_id']
            s['Command'].append(dict(opcode=OPCODES[kind],actor_index=actor_indices[a['actor']],phrase=a['phrase'],target_index=target,flags=0,
                vector=[f32(v)for v in a.get('vector',[0,0])],value=a.get('value',0),duration=a.get('duration',0),auxiliary_index=NONE))
        s['Program'].append(dict(stable_id=world_program(self.world_bindings,'postwin')['id'],first_command=first_command,command_count=len(commands),phrase_count=len(doc)))
        self.record_map('Doll post-win Program/Command/MovementPath/MovementEntry','upstream/MOTHER-Encore/Data/Dialogue/Podunk/cutscenes/doll_defeated.yaml;reports/doll-postwin/native-parser-animation.json',
            'all eight native-parser phrases in original DialogueBox handler order; final nonbattle done carries source camera return duration',
            'source jump speed aliases duration; false poltergeist flag retained; no melody, tutorial or initial event-position fabrication')
        self.record_map('Minnie ActorInstance/ActorProfile/Resource','reports/cloud-world/house-exact.json;reports/doll-postwin/native-parser-animation.json;content/native-house-presentation.json',
            'Objects/npc3 original texture, 4dir profile, initial position and visibility; House binds original body11',
            'append after existing resource28, actor4, program2 namespaces; original geometry/body IDs untouched')

    def extend_melody(self,flags):
        s=self.sections;native=melody_receipt(self)
        require(len(s['Resource'])==30 and len(s['ActorInstance'])==5 and len(s['Program'])==3 and len(s['Binding'])==3,'Melody append-only namespace')
        # PartyLead aliases the configured Ninten name only in this source-backed
        # initial singleton party. New party-changing scripts require a new scope.
        global_code=self.text('Scripts/global/global.gd')
        init=global_code[global_code.index('func _init_player():'):global_code.index('\nfunc ',global_code.index('func _init_player():')+5)]
        require(re.findall(r'party\.append\(([^\n]+)\)',init)==['globaldata.characters.ninten']and 'partyObjects = [player]'in init,'Melody requires reviewed singleton Ninten party')
        import yaml
        save=yaml.safe_load(self.text('Data/save_new_game.yaml'))
        require(save['party']==['ninten'],'Melody singleton save-party contract')
        effect_binding=self.world_bindings['effect'];self.document(effect_binding['source_ir'])
        effect=self.add_resource(effect_binding['path'],kind=4)
        require(s['Resource'][effect]['stable_id']==effect_binding['id'],'Melody effect stable order mismatch')
        audio={}
        for binding in self.world_bindings['audio']:
            path=binding['path'];index=self.add_resource('res://'+path,kind=2)
            require(s['Resource'][index]['stable_id']==binding['id'],'Melody audio stable order mismatch');audio['res://'+path]=index
        root_audio=[row for row in self.world_bindings['audio']if row['selector']['kind']=='scene_loop']
        require(len(root_audio)==1,'Melody root music coverage');house=audio['res://'+root_audio[0]['path']]
        bindings={}
        targets={'house_music':house,'effect':effect}
        for binding in self.world_bindings['melody_bindings']:
            index=len(s['Binding']);require(index+1==binding['id'],'Melody object stable order mismatch');bindings[binding['name']]=index
            s['Binding'].append(dict(stable_id=binding['id'],kind=binding['kind'],flags=0,target_index=targets[binding['target']],auxiliary_index=NONE,value=0,duration=0))
        end=postwin_return_duration(self.text('Scripts/UI/DialogueBox.gd'))
        compiled={'melody':compile_melody(native['yaml'][0],end),'guard':compile_guard(native['yaml'][1],end)}
        for binding in self.world_bindings['programs']:
            if binding['stage']not in compiled:continue
            path=binding['path'];commands=compiled[binding['stage']];labels=binding['labels']
            require(binding['id']==len(s['Program'])+1,'Melody program stable order mismatch')
            first=len(s['Command'])
            for a in commands:
                kind=a['kind'];target=NONE
                if kind=='MoveActorPath':
                    p=a['path'];target=len(s['MovementPath']);entry=len(s['MovementEntry'])
                    for e in p['movement']:s['MovementEntry'].append(dict(kind=0,vector=[f32(e['x']),f32(e['y'])],duration=0))
                    s['MovementPath'].append(dict(stable_id=target+1,first_entry=entry,entry_count=len(p['movement']),flags=int(p['type']=='step'),animation_motion=world_motion(self.world_bindings,p.get('animation')),speed=p['speed']))
                elif kind=='CallObjectDeferred':target=bindings[a['binding']]
                elif kind in('PlaySound','PlayMusicImmediate'):target=audio[a['resource']]
                elif kind=='SetFlag':target=flags.index(a['flag'])
                elif kind=='ShowDialogue':target=a['dialogue_id']
                s['Command'].append(dict(opcode=OPCODES[kind],actor_index=world_actor_indices(self.world_bindings)[a['actor']],phrase=a['phrase'],target_index=target,
                    flags=a.get('flags',0),vector=[f32(v)for v in a.get('vector',[0,0])],value=a.get('value',0),duration=a.get('duration',0),auxiliary_index=NONE))
            s['Program'].append(dict(stable_id=len(s['Program'])+1,first_command=first,command_count=len(commands),phrase_count=len(labels),source_path_string=self.string(path)))
            self.record_map('Program '+path,'reports/doll-melody/native-parser.json;upstream/MOTHER-Encore/Data/Dialogue/'+path+'.yaml',
                dict(dense_phase_source_labels=labels,command_order='original DialogueBox handler order'),
                'rules4: original NPC inherited talker, deferred object calls, immediate dialogue music, concurrent text input delay, dialogue-owned camera; only source-bound actors restored')
        self.record_map('Melody Resource/Binding','upstream/MOTHER-Encore/Maps/podunk/Nintens House.tscn;upstream/MOTHER-Encore/Nodes/Overworld/MusicChanger.tscn;content/native-world-effect.json',
            'Root MusicArea House ownership; source music and heal requests; checked independent melody effect pack',
            'Targeted stop is a no-op without attached House track; it cannot stop newly started melody. Source music properties execute before deferred object calls.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=ROOT / 'content/native-opening.json')
    parser.add_argument('--source-map', type=Path, default=ROOT / 'content/native-source-map.json')
    parser.add_argument('--check', action='store_true', help='Compare deterministic baseline extraction without writing')
    args = parser.parse_args()
    try:
        ir, source_map = Extractor().run()
        for path, document in ((args.out, ir), (args.source_map, source_map)):
            content = json.dumps(document, ensure_ascii=False, indent=2, allow_nan=False) + '\n'
            if args.check:
                require(path.read_text(encoding='utf-8') == content, 'Baseline differs from ' + str(path))
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content, encoding='utf-8')
        print(('Verified' if args.check else 'Extracted') + ' reviewed opening IR: ' + ', '.join(name + '=' + str(len(rows)) for name, rows in ir['sections'].items()))
        return 0
    except (OSError, ValueError, KeyError, TypeError, ImportError, subprocess.CalledProcessError) as error:
        print('NATIVE EXTRACTION ERROR: ' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
