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
        sprite_recipe = self.reviews['ninten-sprite']
        actor_receipt = self.document('romfs/actor-preview/source.json')
        require(actor_receipt['recipe'] == sprite_recipe, 'Stale actor recipe')
        for key in ('scene', 'texture', 'lamp_texture', 'lamp_yaml', 'npc_script',
                    'character_sprite_script', 'emote_texture', 'emote_scene', 'shadow_texture', 'actor_scene'):
            self.source(sprite_recipe[key], sprite_recipe[key + '_sha256'])
        emote_text = self.text(sprite_recipe['emote_scene'])
        emote_node = node_block(emote_text, '.')
        emote_grid = [int(prop(emote_node, key)) for key in ('hframes', 'vframes')]
        asset_descriptors = [
            ('primary', 'ninten-main.t3x', sprite_recipe['texture'], sprite_recipe['grid']),
            ('lamp', 'lamp.t3x', sprite_recipe['lamp_texture'], sprite_recipe['lamp_layout']['grid']),
            ('emote', 'emotes.t3x', sprite_recipe['emote_texture'], emote_grid),
            ('shadow', 'shadow.t3x', sprite_recipe['shadow_texture'], [1, 1])]
        resources = {}
        for role, name, source, grid in asset_descriptors:
            size = image_size(self.source(source))
            resources[role] = self.add_resource('actor-preview/' + name, *size, *grid,
                                               expected=actor_receipt['outputs'][name]['sha256'])
        layers_receipt = self.document('romfs/house-layers/source.json')
        layer_data = self.document('reports/m2-scene-reference-reviewed/house-data.json', layers_receipt['scene_export_sha256'])
        self.document('reports/m2-scene-reference-reviewed/receipt.json', layers_receipt['reference_sha256'])
        self.document('reports/m2-scene-reference-reviewed/house-source.json', layers_receipt['source_receipt_sha256'])
        for path, expected in layers_receipt['sources'].items():
            self.source(path, expected)
        overlays = overlay_records(layer_data)
        require(layers_receipt == overlay_receipt(self.upstream, overlays, self.root / 'romfs/house-layers'),
                'Stale overlay bundle')
        # Preserve the established Resource indices/stable IDs used by the
        # separate audio bank. New resources are appended after the old slice.
        for role in ('objects',):
            resource = layers_receipt['resources'][role]
            size = image_size(self.source(resource['source'], resource['source_sha256']))
            resources[role] = self.add_resource('house-layers/' + resource['output'], *size, 1, 1,
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
        map_receipt = self.document('romfs/map-preview/source.json')
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
            rid = self.add_resource('map-preview/' + name, tile['width'], tile['height'], 1, 1,
                                    expected=map_receipt['outputs'][name]['sha256'])
            s['MapDraw'].append(dict(stable_id=tile['index'] + 1, resource_index=rid,
                x=tile['x'], y=tile['y'], w=tile['width'], h=tile['height'], flags=0))
        self.record_map('Resource/Overlay/MapDraw', 'romfs/{actor-preview,house-layers,map-preview}/source.json',
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
        lamp_yaml = yaml.safe_load(self.text(sprite_recipe['lamp_yaml']))
        party_yaml = yaml.safe_load(self.text('Data/Animations/PartyMember.yaml'))
        require(set(lamp_yaml['animations']) == {'Idle', 'Open'}, 'Unknown lamp animation source')
        for name, flags in (('Idle', 4), ('Open', 2)):
            anim = lamp_yaml['animations'][name]
            require(anim['type'] == 1 and len(anim['directions']) == 1, 'Unreviewed lamp animation')
            direction = anim['directions'][0]; time = float(direction[0]); keys = []
            for frame, duration in direction[1:]:
                keys.append((time, frame - 1)); time += duration
            clip_names['Lamp ' + name] = add_clip(time, keys, flags, frame_count=math.prod(lamp_yaml['size']))
        surprise = one(r'^\[sub_resource type="Animation" id=\d+\]\nresource_name = "surprise"\n(.*?)(?=^\[|\Z)',
                       emote_text, 'surprise animation', re.M | re.S)[1]
        times = [float(n.strip()) for n in one(r'"times": PoolRealArray\( ([^)]*) \)', surprise, 'surprise times')[1].split(',')]
        frames = [int(n.strip()) for n in one(r'"values": \[ ([^]]*) \]', surprise, 'surprise frames')[1].split(',')]
        require(len(times) == len(frames), 'Emote timeline arity')
        clip_names['surprise'] = add_clip(float(prop(surprise, 'length')), list(zip(times, frames)),
                                         8, frame_count=math.prod(emote_grid), channel=1)
        idle_directions = party_yaml['animations']['Idle']['directions']
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
        for index, data in enumerate((party_yaml, lamp_yaml)):
            rid = resources['primary' if index == 0 else 'lamp']; resource = s['Resource'][rid]
            offset = [f32(data['offset'][0]), f32(-int(resource['height'] / float(resource['rows'] * 2)) + data['offset'][1])]
            emote_offset = [0, f32(-(resource['height'] / resource['rows'] + bubble_extra))]
            idle_clip = clip_names['Idle Down' if index == 0 else 'Lamp Idle']
            s['ActorProfile'].append(dict(stable_id=index + 1, execution_kind=index + 1,
                flags=int(next(n for n in player['nodes'] if n['path'] == 'Shadow')['properties']['visible']) if index == 0 else int(sprite_recipe['lamp_layout']['shadow']),
                primary_resource=rid, shadow_resource=resources['shadow'], emote_resource=resources['emote'],
                animation_binding_first=0 if index == 0 else len(s['AnimationBinding']),
                animation_binding_count=len(s['AnimationBinding']) if index == 0 else 0,
                initial_frame=s['DirectionFrame'][0]['frame'] if index == 0 else lamp_yaml['animations']['Idle']['directions'][0][1][0] - 1,
                emote_initial_frame=emote_initial, sprite_position=sprite_position, sprite_offset=offset,
                emote_offset=emote_offset, shadow_offset=shadow_offset, direction_first=0,
                direction_count=len(s['DirectionFrame']) if index == 0 else 0,
                idle_clip=idle_clip, emote_clip=clip_names['surprise'] if index == 0 else NONE))
        intro_source = 'Maps/Cutscenes/Mt Itoi Landscape.tscn'
        intro_door = node_block(self.text(intro_source), 'Objects/Door')
        door_y = self.scalar('Scripts/Main/Door.gd', r'Vector2\(targetX, targetY - ' + NUMBER + r'\)', 'Scene.spawn.y.adjustment', 1)
        spawn = [f32(float(prop(intro_door, 'targetX'))), f32(float(prop(intro_door, 'targetY')) - door_y)]
        direction = pair(prop(intro_door, 'dir'))
        npc_default = one(r'initial_dir = (Vector2\([^\n]+\))', self.text('Scripts/Main/npc.gd'), 'NPC initial direction')[1]
        lamp_direction = pair(npc_default)
        instance_visibility = [player['nodes'][0]['properties']['visible'],
                               all(hn[path]['properties'].get('visible', True)
                                   for path in ('.', 'Objects', 'Objects/lamp'))]
        require(all(type(value) is bool for value in instance_visibility), 'Invalid native actor visibility')
        for i, name in enumerate((Path(sprite_recipe['texture']).parent.name, hn['Objects/lamp']['name'])):
            s['ActorInstance'].append(dict(stable_id=i + 1, profile_index=i, binding_kind=i + 1, flags=int(instance_visibility[i]),
                display_name_string=self.string(name), position=spawn if i == 0 else list(vec(hn['Objects/lamp']['properties']['position'])),
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
        music_node = node_block(house_text, 'Poltergeist/MusicArea')
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
        s['Binding'] = [dict(stable_id=1, kind=1, flags=0, target_index=resources['music'], auxiliary_index=NONE, value=0, duration=0),
                        dict(stable_id=2, kind=3, flags=0, target_index=shake_sound, auxiliary_index=NONE, value=magnitude, duration=delay)]
        self.record_map('Binding[1]/Resource[roomshake]', 'upstream/MOTHER-Encore/' + shaker_source + ';upstream/MOTHER-Encore/Nodes/Reusables/roomshaker.tscn',
                        'delayed_start default; magnitude/sound exports; native repeating idle Timer defaults',
                        'PeriodicCameraShake binding with sound resource index; scheduler owns guarded timeout and source RNG sequence')
        battle_actions = [a for a in actions if a['kind'] == 'QueueBattle']
        require(len(battle_actions) == 1, 'Unreviewed battle request count')
        battle = battle_actions[0]
        dialogue_code = self.text('Scripts/UI/DialogueBox.gd')
        battle_args = one(r'uiManager\.start_battle\((-?\d+), (true|false), \[\], _post_battle_cutscenes, _battle_win_flag\)',
                          dialogue_code, 'battle request arguments')
        s['Battle'] = [dict(stable_id=1, enemy_string=self.string(battle['text']), actor_instance_index=1,
            win_flag_index=flags.index(battle['detail']), advantage=int(battle_args[1]),
            win_cutscene_string=0, battle_resource_index=self.add_resource('data/opening.encbattle',kind=3),
            flags=int(battle_args[2] == 'true') | (2 if any(a['kind'] == 'OverworldBattleMusic' and a['value'] for a in actions) else 0))]
        actor_indexes = {'None': 65535, 'Ninten': 0, 'Lamp': 1}
        binding_indexes = {('Poltergeist/MusicArea', 'play_music'): 0, ('Room Shaker', 'delayed_start'): 1}
        for a in actions:
            target = NONE
            if a['kind'] == 'CallObjectDeferred': target = binding_indexes[a['text'], a['detail']]
            elif a['kind'] == 'PlaySound': target = sound_resources[a['text']]
            elif a['kind'] == 'AnimateActor': target = clip_names['Lamp ' + a['text']]
            elif a['kind'] == 'EmoteActor': target = clip_names[a['text']]
            elif a['kind'] in ('QueueBattle', 'RequestBattle'): target = 0
            elif a['kind'] == 'MoveCamera': target = 1 # reviewed schema SineOut
            s['Command'].append(dict(opcode=OPCODES[a['kind']], actor_index=actor_indexes[a['actor']], phrase=a['phrase'],
                target_index=target, flags=0, vector=[f32(v) for v in a['vector']],
                value=a['value'] if a['kind'] != 'AnimateActor' else 0,
                duration=a['duration'], auxiliary_index=NONE))
        s['Program'] = [dict(stable_id=1, first_command=0, command_count=len(actions), phrase_count=len(receipt['dialogue']))]
        self.record_map('Program/Command/Binding/Battle', 'reports/m5-lamp-dialogue-reference/dialogue.json;compatibility/reviews/lamp-dialogue-v0410.json',
                        'lamp_dialogue.compile_receipt reviewed native YAML parser receipt and fixed handler order',
                        'source-only names resolved offline to explicit clip/resource/actor/binding/battle indices; no runtime string dispatch')
        self.record_map('Battle.flags/advantage', 'upstream/MOTHER-Encore/Scripts/UI/DialogueBox.gd',
                        'start_battle arguments + ovbattlemusic reviewed YAML field')
        self.extend_doll(hn,resources,clip_names,add_clip,sprite_position,shadow_offset,emote_initial,bubble_extra,flags,battle_args)
        foreground_resource = layers_receipt['resources']['above']
        size = image_size(self.source(foreground_resource['source'], foreground_resource['source_sha256']))
        resources['above'] = self.add_resource('house-layers/' + foreground_resource['output'], *size, 1, 1,
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
        for program,path in zip(s['Program'],('Podunk/cutscenes/lamp_attack','Podunk/cutscenes/doll_attack','Podunk/cutscenes/doll_defeated')):
            self.source('Data/Dialogue/'+path+'.yaml')
            program['source_path_string']=self.string(path)
        # Append after all established rows AND strings, preserving identities.
        self.extend_melody(flags)
        from tools.link_phone_content import link_room
        link_room(self)
        from tools.link_pillow_content import append_room
        append_room(self,clip_names,add_clip)
        for module in ('world_geometry', 'house_layers', 'character_animation', 'lamp_dialogue', 'doll_dialogue', 'doll_postwin', 'melody_dialogue', 'reference_animation', 'scene_data', 'reference_progression', 'upstream', 'map_asset'):
            self.file('tools/' + module + '.py')
        self.file('tools/extract_native_content.py')
        source_map = dict(schema=1, upstream_commit=self.lock['commit'], scope='Existing scoped reviews only; not whole game/scene/script approval',
                          fields=self.mapping, stable_namespaces=dict(body=body_ids, owner=owner_ids,
                              clips={name: index + 1 for name, index in clip_names.items()}))
        ir = dict(schema=1, family=0x454e0002, rules=7, capabilities=7, scene_id=1,
                  upstream_commit=self.lock['commit'], exporter_version=1, adapter_revision=8,
                  strings=self.strings, sections=s, provenance=dict(sources=self.sources,
                      notes=['Baseline extraction from original source and existing scoped native exports/reviews; no new broad source approval.',
                             'C++ sources and generated headers are never extraction dependencies.',
                             'Audio entries are request-only upstream URIs; no audio backend or battle implementation claimed.',
                             'Float32 values retain native rounding; detailed field selectors are in native-source-map.json.']))
        return ir, source_map

    def extend_doll(self,hn,resources,clip_names,add_clip,sprite_position,shadow_offset,emote_initial,bubble_extra,flags,battle_args):
        s=self.sections;native=doll_receipt(self);doc=native['yaml'][0]
        presentation=self.document('content/native-house-presentation.json')
        for role in ('doll','mimmie'):
            candidates=[r for r in presentation['resources']if r['role']==role]
            require(len(candidates)==1,'Doll source resource identity')
            r=candidates[0];resources[role]=self.add_resource(r['path'],r['width'],r['height'],r['columns'],r['rows'],expected=r['sha256'])
            source_node=node_block(self.text('Maps/podunk/Nintens House.tscn'),'Objects/npcdoll'if role=='doll'else'Objects/npc2')
            self.source('Graphics/Character Sprites/'+json.loads(prop(source_node,'sprite'))+'.png')
        def add_native(name,clip,frames,extra=0,channel=0):
            flags_=int(clip['loop'])|extra|(16 if clip['keys'][0][0]>0 else 0)
            clip_names[name]=add_clip(clip['length'],clip['keys'],flags_,frame_count=frames,channel=channel)
            return clip_names[name]
        # Player AnimationPlayer tracks remain unchanged. Actor replacement uses
        # a distinct motion selector for its native PartyMember YAML Walk tracks.
        directions=['Down','Left','Right','Up','DownLeft','DownRight','UpLeft','UpRight']
        for d,name in enumerate(directions):
            c=add_native('Ninten Actor Walk '+name,native['animations'][0]['Walk '+name],math.prod(native['yaml'][1]['size']))
            s['AnimationBinding'].append(dict(actor_profile_index=0,motion_state=4,direction=d,clip_index=c))
        s['ActorProfile'][0]['animation_binding_count']=len(s['AnimationBinding'])
        s['ActorProfile'][1]['animation_binding_first']=len(s['AnimationBinding'])
        for role,profile_index,yaml_index,animation_index,nodepath in [('doll',2,3,2,'Objects/npcdoll'),('mimmie',3,2,1,'Objects/npc2')]:
            yaml=native['yaml'][yaml_index];frames=math.prod(yaml['size']);r=s['Resource'][resources[role]]
            native_clips=native['animations'][animation_index];first_binding=len(s['AnimationBinding']);first_direction=len(s['DirectionFrame'])
            if role=='doll':
                # Source Actor.play_anim emits finished_action immediately for
                # the idle animation. Initialization only selects the clip and
                # therefore does not emit this play-command event.
                idle=add_native('Doll Idle',native_clips['Idle'],frames,4)
                add_native('Doll Float',native_clips['Float'],frames)
                direction_count=0
            else:
                for motion,animation in [(0,'Idle'),(4,'Walk'),(5,'Talk')]:
                    for d,name in enumerate(directions[:4]):
                        c=add_native('Mimmie '+animation+' '+name,native_clips[animation+' '+name],frames)
                        s['AnimationBinding'].append(dict(actor_profile_index=profile_index,motion_state=motion,direction=d,clip_index=c))
                idle=clip_names['Mimmie Idle Down'];direction_count=4
                s['DirectionFrame'].extend(dict(frame=int(direction[1][0])-1)for direction in yaml['animations']['Idle']['directions'])
            node=hn[nodepath];properties=node['properties'];source_position=pair(prop(node_block(self.text('Nodes/Reusables/npc.tscn'),'CharacterSprite'),'position'))
            offset=[f32(yaml['offset'][0]),f32(-int(r['height']/float(r['rows']*2))+yaml['offset'][1])]
            source_node=node_block(self.text('Maps/podunk/Nintens House.tscn'),nodepath)
            no_shadow=bool(re.search(r'^no_shadow = true$',source_node,re.M))
            s['ActorProfile'].append(dict(stable_id=profile_index+1,execution_kind=2 if role=='doll'else 3,flags=int(not no_shadow),
                primary_resource=resources[role],shadow_resource=resources['shadow'],emote_resource=resources['emote'],
                animation_binding_first=first_binding,animation_binding_count=len(s['AnimationBinding'])-first_binding,
                initial_frame=s['Key'][s['Clip'][idle]['first_key']]['frame'],emote_initial_frame=emote_initial,sprite_position=source_position,
                sprite_offset=offset,emote_offset=[0,f32(-(r['height']/r['rows']+bubble_extra))],shadow_offset=shadow_offset,
                direction_first=first_direction,direction_count=direction_count,idle_clip=idle,emote_clip=NONE))
            s['ActorInstance'].append(dict(stable_id=profile_index+1,profile_index=profile_index,binding_kind=2,
                flags=int(all(hn[p]['properties'].get('visible',True)for p in('.', 'Objects',nodepath))),display_name_string=self.string(node['name']),
                position=list(vec(properties['position'])),direction=[0,1],initial_clip=idle))
        add_native('exclamation',native['exclamation'],s['Resource'][resources['emote']]['columns']*s['Resource'][resources['emote']]['rows'],8,1)
        s['ActorProfile'][3]['emote_clip']=clip_names['exclamation']
        s['Binding'].append(dict(stable_id=3,kind=4,flags=0,target_index=1,auxiliary_index=NONE,value=0,duration=0))
        battle=doc['6']['startbattle']
        require(all('ovbattlemusic'not in p for p in doc.values()),'Doll must inherit original overworld battle music state')
        s['Battle'].append(dict(stable_id=2,enemy_string=self.string(next(iter(battle['battlers'][0]))),actor_instance_index=2,
            win_flag_index=NONE,advantage=int(battle_args[1]),flags=int(battle_args[2]=='true')|4|8,
            win_cutscene_string=self.string(battle['wincutscene']),battle_resource_index=self.add_resource('data/doll-entry.encbattle',kind=3)))
        actor_indices={'None':65535,'Ninten':0,'Lamp':1,'Doll':2,'Mimmie':3};first_command=len(s['Command'])
        commands=compile_doll_dialogue(doc)
        for a in commands:
            kind=a['kind'];target=NONE
            if kind=='MoveActorPath':
                p=a['path'];target=len(s['MovementPath']);first=len(s['MovementEntry'])
                for e in p['movement']:
                    s['MovementEntry'].append(dict(kind=int('wait'in e),vector=[0,0]if'wait'in e else[f32(e['x']),f32(e['y'])],duration=e.get('wait',0)))
                s['MovementPath'].append(dict(stable_id=target+1,first_entry=first,entry_count=len(p['movement']),flags=int(p['type']=='step')|int(p.get('moonwalk',False))*2,animation_motion=4 if p.get('animation')=='Walk'else 65535,speed=p['speed']))
            elif kind in('AnimateActor','EmoteActor'):target=clip_names[a['clip']]
            elif kind in('QueueBattle','RequestBattle'):target=1
            elif kind=='SetFlag':target=flags.index(a['flag'])
            elif kind=='ShowDialogue':target=a['dialogue_id']
            elif kind=='CallObjectDeferred':target=a['binding']
            s['Command'].append(dict(opcode=OPCODES[kind],actor_index=actor_indices[a['actor']],phrase=a['phrase'],target_index=target,flags=0,
                vector=[f32(v)for v in a.get('vector',[0,0])],value=a.get('value',0),duration=a.get('duration',0),auxiliary_index=NONE))
        s['Program'].append(dict(stable_id=2,first_command=first_command,command_count=len(commands),phrase_count=len(doc)))
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
        candidates=[r for r in presentation['resources']if r['role']=='minnie']
        require(len(candidates)==1,'Minnie source resource identity')
        r=candidates[0];resources['minnie']=self.add_resource(r['path'],r['width'],r['height'],r['columns'],r['rows'],expected=r['sha256'])
        sound='res://Audio/Sound effects/'+doc['0']['soundeffect'];resources['postwin_sound']=self.add_resource(sound,kind=2)
        nodepath=doc['0']['actors']['minnie'];source_node=node_block(self.text('Maps/podunk/Nintens House.tscn'),nodepath)
        require(json.loads(prop(source_node,'sprite'))=='Npcs/4dir/minnie','Minnie source texture binding')
        self.source('Graphics/Character Sprites/Npcs/4dir/minnie.png')
        require(not re.search(r'^yaml = ',source_node,re.M),'Minnie inherited animation changed')
        yaml=native['yaml'][2];frames=math.prod(yaml['size']);profile_index=len(s['ActorProfile'])
        def add_native(name,clip,channel=0):
            clip_names[name]=add_clip(clip['length'],clip['keys'],int(clip['loop'])|(8 if channel else 0)|(16 if clip['keys'][0][0]>0 else 0),frame_count=frames if not channel else s['Resource'][resources['emote']]['columns']*s['Resource'][resources['emote']]['rows'],channel=channel)
            return clip_names[name]
        first_binding=len(s['AnimationBinding']);first_direction=len(s['DirectionFrame'])
        for motion,animation in [(0,'Idle'),(4,'Walk'),(5,'Talk')]:
            for d,name in enumerate(['Down','Left','Right','Up']):
                clip=add_native('Minnie '+animation+' '+name,native['animations'][1][animation+' '+name])
                s['AnimationBinding'].append(dict(actor_profile_index=profile_index,motion_state=motion,direction=d,clip_index=clip))
        idle=clip_names['Minnie Idle Down']
        s['DirectionFrame'].extend(dict(frame=int(direction[1][0])-1)for direction in yaml['animations']['Idle']['directions'])
        source_position=pair(prop(node_block(self.text('Nodes/Reusables/npc.tscn'),'CharacterSprite'),'position'))
        node=hn[nodepath];offset=[f32(yaml['offset'][0]),f32(-int(r['height']/float(r['rows']*2))+yaml['offset'][1])]
        require(not re.search(r'^no_shadow = true$',source_node,re.M),'Minnie shadow binding changed')
        s['ActorProfile'].append(dict(stable_id=profile_index+1,execution_kind=3,flags=1,primary_resource=resources['minnie'],
            shadow_resource=resources['shadow'],emote_resource=resources['emote'],animation_binding_first=first_binding,
            animation_binding_count=len(s['AnimationBinding'])-first_binding,initial_frame=s['Key'][s['Clip'][idle]['first_key']]['frame'],
            emote_initial_frame=emote_initial,sprite_position=source_position,sprite_offset=offset,
            emote_offset=[0,f32(-(r['height']/r['rows']+bubble_extra))],shadow_offset=shadow_offset,
            direction_first=first_direction,direction_count=4,idle_clip=idle,emote_clip=NONE))
        s['ActorInstance'].append(dict(stable_id=profile_index+1,profile_index=profile_index,binding_kind=2,
            flags=int(all(hn[p]['properties'].get('visible',True)for p in('.', 'Objects',nodepath))),display_name_string=self.string(node['name']),
            position=list(vec(node['properties']['position'])),direction=[0,1],initial_clip=idle))
        add_native('dot',native['dot'],1)
        end_duration=postwin_return_duration(self.text('Scripts/UI/DialogueBox.gd'))
        commands=compile_postwin_dialogue(doc,end_duration);first_command=len(s['Command'])
        actor_indices={'None':65535,'Ninten':0,'Lamp':1,'Doll':2,'Mimmie':3,'Minnie':4}
        for a in commands:
            kind=a['kind'];target=NONE
            if kind=='MoveActorPath':
                p=a['path'];target=len(s['MovementPath']);first=len(s['MovementEntry'])
                for e in p['movement']:s['MovementEntry'].append(dict(kind=0,vector=[f32(e['x']),f32(e['y'])],duration=0))
                s['MovementPath'].append(dict(stable_id=target+1,first_entry=first,entry_count=len(p['movement']),flags=int(p['type']=='step'),animation_motion=4 if p.get('animation')=='Walk'else 65535,speed=p['speed']))
            elif kind in('AnimateActor','EmoteActor'):target=clip_names[a['clip']]
            elif kind=='PlaySound':target=resources['postwin_sound']
            elif kind=='SetFlag':target=flags.index(a['flag'])
            elif kind=='ShowDialogue':target=a['dialogue_id']
            s['Command'].append(dict(opcode=OPCODES[kind],actor_index=actor_indices[a['actor']],phrase=a['phrase'],target_index=target,flags=0,
                vector=[f32(v)for v in a.get('vector',[0,0])],value=a.get('value',0),duration=a.get('duration',0),auxiliary_index=NONE))
        s['Program'].append(dict(stable_id=3,first_command=first_command,command_count=len(commands),phrase_count=len(doc)))
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
        self.document('content/native-world-effect.json')
        effect=self.add_resource('world-effect/melody.encfx',kind=4)
        audio={}
        for path in ('Audio/Music/Melodies/melody1.mp3','Audio/Sound effects/M3/heal_se.wav','Audio/Music/House.mp3'):
            audio['res://'+path]=self.add_resource('res://'+path,kind=2)
        house=audio['res://Audio/Music/House.mp3']
        root_music=node_block(self.text('Maps/podunk/Nintens House.tscn'),'MusicArea')
        require(json.loads(prop(root_music,'loop'))=='House.mp3','Melody root music binding')
        bindings={}
        for name,kind,target in [('stop_house_music',5,house),('effect_appear',6,effect),('house_music',1,house),('effect_disappear',7,effect)]:
            index=len(s['Binding']);bindings[name]=index
            s['Binding'].append(dict(stable_id=index+1,kind=kind,flags=0,target_index=target,auxiliary_index=NONE,value=0,duration=0))
        end=postwin_return_duration(self.text('Scripts/UI/DialogueBox.gd'))
        for path,commands,labels in [('Podunk/dollmelody',compile_melody(native['yaml'][0],end),['0','3','4']),
                                     ('Podunk/cutscenes/mimmie_ignore',compile_guard(native['yaml'][1],end),['0','1'])]:
            first=len(s['Command'])
            for a in commands:
                kind=a['kind'];target=NONE
                if kind=='MoveActorPath':
                    p=a['path'];target=len(s['MovementPath']);entry=len(s['MovementEntry'])
                    for e in p['movement']:s['MovementEntry'].append(dict(kind=0,vector=[f32(e['x']),f32(e['y'])],duration=0))
                    s['MovementPath'].append(dict(stable_id=target+1,first_entry=entry,entry_count=len(p['movement']),flags=int(p['type']=='step'),animation_motion=4 if p.get('animation')=='Walk'else 65535,speed=p['speed']))
                elif kind=='CallObjectDeferred':target=bindings[a['binding']]
                elif kind in('PlaySound','PlayMusicImmediate'):target=audio[a['resource']]
                elif kind=='SetFlag':target=flags.index(a['flag'])
                elif kind=='ShowDialogue':target=a['dialogue_id']
                s['Command'].append(dict(opcode=OPCODES[kind],actor_index={'None':65535,'Ninten':0,'Mimmie':3}[a['actor']],phrase=a['phrase'],target_index=target,
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
