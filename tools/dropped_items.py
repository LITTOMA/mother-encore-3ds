"""Checked source bindings for the seven original catacombs dropped items.

This is an offline source adapter. The original inventory/Item constructors and
DroppedItem lifecycle are observed separately; this model admits no new scene.
"""
import hashlib
import json
import re
import math
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools import catacombs_breakables as b, podunk_field as f
from tools.extract_battle_entry import PIN, require
from tools.godot_text import instantiate, owner_of, Vec2, NodePathValue
from tools.dungeon_keys import fields
from tools.dungeon_key_indicator import unique

PATHS = dict(script='Scripts/Main/DroppedItem.gd',
             holder='Scripts/Main/ItemHolder.gd',
             flag='Scripts/Main/FlaggableObject.gd',
             inventory='Scripts/global/Inventory.gd', item='Scripts/global/Item.gd',
             character='Scripts/global/Character.gd',
             prefab='Nodes/Overworld/Objects/DroppedItem.tscn',
             prompt='Scripts/UI/Button Prompt.gd',
             sparkle='Scripts/misc/sparkles.gd')
RECIPE = ROOT / 'content/catacombs-dropped-items.json'
MAGIC = b'ENCDRP01'
STRIDES = (80, 12, 32, 40, 24, 28, 12, 1)
LIMIT = 128 * 1024
POLICY = ['CollectTravelSeconds', 'CollectDelaySeconds', 'CollectScaleSeconds',
          'ScaleStartX', 'ScaleStartY', 'ScaleEndX', 'ScaleEndY',
          'RevertStartDegrees', 'RevertEndDegrees', 'RevertSeconds',
          'DisappearStage1', 'DisappearStage2', 'DisappearStage3', 'DisappearStage4',
          'BlinkInitialSpeed', 'BlinkSecondSpeed', 'BlinkThirdSpeed',
          'SparkleMinimum', 'SparkleMaximum', 'SparkleFPS', 'SparkleFrames',
          'PromptOffsetX', 'PromptOffsetY', 'SparkleOffsetX', 'SparkleOffsetY',
          'CollectTransition', 'CollectEase', 'ScaleTransition', 'ScaleEase',
          'RevertTransition', 'RevertEase']


def vector(value):
    require(type(value) is Vec2, 'Dropped item vector type')
    return [f.f32(b.number(value.x)), f.f32(b.number(value.y))]


def method_blocks(raw):
    lines = raw.decode('utf-8').splitlines(keepends=True)
    result = {}
    for start, line in enumerate(lines):
        match = re.match(r'^(?:static )?func ([A-Za-z_][A-Za-z_0-9]*)\(', line)
        if match is None:
            continue
        name = match[1]
        require(name not in result, 'Dropped item duplicate source method')
        end = start + 1
        while end < len(lines) and (lines[end].startswith('\t') or not lines[end].strip()):
            end += 1
        require(end > start + 1, 'Dropped item missing source method body')
        block = ''.join(lines[start:end]).rstrip('\r\n') + '\n'
        result[name] = block
    require(result, 'Dropped item missing source methods')
    return result


def method_digests(raw):
    return {name: hashlib.sha256(block.encode()).hexdigest()
            for name, block in method_blocks(raw).items()}


def identity_parent(nodes, path):
    by_path = {n.path: n for n in nodes}
    require(len(by_path) == len(nodes), 'Dropped duplicate source ancestor')
    while path:
        require(path in by_path, 'Dropped missing source ancestor')
        node = by_path[path]
        require(not (set(node.props) & {'transform', 'rotation', 'rotation_degrees', 'scale', 'skew'})
                and vector(node.props.get('position', Vec2(0, 0))) == [0, 0],
                'Dropped unreviewed parent coordinate transform')
        path = f.parent_path(path)


def derive():
    recipe, breaks, break_proof = b.source()
    s = f.Sources()
    nodes = instantiate(s.doc, recipe['paths']['scene'])
    objects = [n for n in nodes if n.script == PATHS['script']]
    expected = [v['path'] for v in breaks['contents'] if v['script'] == PATHS['script']]
    # Contents follow their containing coffins; item nodes follow the scene
    # tree. Compare coverage without imposing the unrelated container order.
    require(len(objects) == len(expected) == 7
            and {n.path for n in objects} == set(expected),
            'Dropped item complete source content coverage')
    allowed = {'script', 'position', 'show_behind_parent', 'button_prompt', 'item'}
    bindings = []
    templates = []
    for n in objects:
        identity_parent(nodes, f.parent_path(n.path))
        require(n.type == 'Sprite' and n.scene == PATHS['prefab']
                and set(n.props) == allowed and n.props['show_behind_parent'] is True
                and type(n.props['button_prompt']) is NodePathValue
                and n.props['button_prompt'].path == 'interact/ButtonPrompt',
                'Dropped item unreviewed original overrides')
        name = n.props['item']
        b.text(name)
        if not any(v['source'] == name for v in templates):
            source = 'Data/Items/' + name + '.yaml'
            document = s.extractor.yaml(source)
            require(document['keyitem'] is False and document['transform'] == ''
                    and document['slot'] == '' and document['actions']
                    and all(v['function'] == 'consume' for v in document['actions'])
                    and not document.get('battle_action'),
                    'Dropped item unsupported source traits')
            texture = 'Graphics/Objects/Items/' + name + '.png'
            size = s.extractor.png_size(texture)
            s.data(texture + '.import')
            templates.append(dict(id=b.stable('dropped-template:' + name), source=name,
                                  yaml=source, document=document, texture=texture,
                                  size=size, doses=document.get('doses', 1), key_item=False))
        template = next(v for v in templates if v['source'] == name)
        bindings.append(dict(id=b.stable('dropped-item:' + n.path), order=nodes.index(n),
                             path=n.path, name=n.name, parent=f.parent_path(n.path),
                             template=template['id'], position=vector(n.props['position']),
                             flag=breaks['name'] + '/' + n.name, flag_kind='object'))
    methods = {}
    for key in ['script', 'holder', 'flag', 'inventory', 'item', 'prompt', 'sparkle']:
        raw = s.data(PATHS[key])
        methods[key] = method_digests(raw)
    s.data(PATHS['character'])
    s.data('LICENSE')
    prefab = {n.path: n for n in instantiate(s.doc, PATHS['prefab'])}
    body = prefab['StaticBody2D']
    require(body.props['collision_layer'] == 4 and body.props['collision_mask'] == 0,
            'Dropped item static collision source')
    geometry = {}
    for target in ['StaticBody2D/CollisionShape2D', 'interact/CollisionShape2D']:
        shape = prefab[target]
        doc = owner_of(shape, 'shape')
        sub = doc.sub_resource(shape.props['shape'], 'RectangleShape2D')
        geometry[target] = dict(position=vector(shape.props.get('position', Vec2(0, 0))),
                                extents=vector(sub['extents']))
    model = dict(scene=breaks['scene'], scene_source=breaks['source'], name=breaks['name'],
                 paths=PATHS, bindings=bindings, templates=templates, geometry=geometry)
    proof = dict(commit=PIN, sources=s.digests(), methods=methods,
                 coordinate_scope='Original root and item ancestors have identity transforms; '
                                  'world/local positions agree in the admitted original bindings',
                 breakables=break_proof,
                 producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                 scope='Original seven item bindings, item YAML and inventory methods; '
                       'no release catalog, save-domain or whole-room admission')
    return model, proof


def build_model():
    model, proof = derive()
    s = f.Sources()
    source = s.text(PATHS['script'])
    holder = s.text(PATHS['holder'])
    inventory = s.text(PATHS['inventory'])
    prefab = {n.path: n for n in instantiate(s.doc, PATHS['prefab'])}
    travel = b.one(r'tween_property\(self, "global_position", global.get_player\(\).global_position, ([0-9.]+)\)', source, 'dropped travel')
    delay = b.one(r'\.set_delay\(([0-9.]+)\)', source, 'dropped delay')
    scale = b.one(r'tween_property\(self, "scale", Vector2\(([0-9.]+), ([0-9.]+)\), ([0-9.]+)\)', source, 'dropped scale')
    start = b.one(r'\.from\(Vector2\(([0-9.]+), ([0-9.]+)\)\)', source, 'dropped initial scale')
    revert = b.one(r'interpolate_property\(\$Sprite, "rotation_degrees", ([0-9.]+), ([0-9.]+), ([0-9.]+), Tween.TRANS_ELASTIC,Tween.EASE_OUT\)', source, 'dropped revert')
    require('.set_trans(Tween.TRANS_QUART).set_ease(Tween.EASE_OUT)' in source
            and '.set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_IN)' in source,
            'Dropped item unknown source interpolation')
    timers = re.findall(r'\$Timer.start\(([0-9.]+)\)', source)
    speeds = re.findall(r'\$AnimationPlayer.playback_speed = ([0-9.]+)', source)
    require(len(timers) == 4 and len(speeds) == 2, 'Dropped item timer/blink stage coverage')
    sparkle = prefab['Sparkles']
    frames = owner_of(sparkle, 'frames').sub_resource(sparkle.props['frames'], 'SpriteFrames')['animations']
    require(len(frames) == 1 and frames[0]['name'] == sparkle.props['animation']
            and frames[0]['loop'] is True and sparkle.props['playing'] is True,
            'Dropped item sparkle animation')
    sparkle_range = b.one(r'frame = int\(rand_range\(([0-9.]+), ([0-9.]+)\)\)',
                          s.text(PATHS['sparkle']), 'dropped sparkle initial RNG')
    values = [travel, delay, scale[2], *start, *scale[:2], *revert, *timers,
              prefab['AnimationPlayer'].props['playback_speed'], *speeds,
              *sparkle_range, frames[0]['speed'], len(frames[0]['frames'])]
    values += vector(prefab['interact/ButtonPrompt'].props['offset']) + vector(sparkle.props['position'])
    # Godot 3.6.2 Tween enum schema, verified against the pinned tween.h/table.
    transitions = {'TRANS_QUART': 3, 'TRANS_QUAD': 4, 'TRANS_ELASTIC': 6}
    easing = {'EASE_IN': 0, 'EASE_OUT': 1}
    curves = re.findall(r'\.set_trans\(Tween\.(TRANS_[A-Z]+)\).set_ease\(Tween\.(EASE_[A-Z]+)\)', source)
    curves += re.findall(r'([A-Z_]+),Tween\.([A-Z_]+)\)', source)
    require(len(curves) == 3 and all(t in transitions and e in easing for t, e in curves),
            'Dropped source interpolation enum coverage')
    for transition, ease in curves:
        values += [transitions[transition], easing[ease]]
    require(len(values) == len(POLICY), 'Dropped item complete scalar policy')
    model['policy'] = dict(zip(POLICY, map(float, values)))
    model['capacity'] = int(b.one(r'^const _MAX_INVENTORY_SIZE := (\d+)$', inventory, 'dropped ordinary capacity'))
    model['success_dialogue'] = b.one(r'dialog = "([^"]+)"', holder, 'dropped success dialogue')
    model['full_dialogue'] = b.one(r'dialog_full = "([^"]+)"', holder, 'dropped full dialogue')
    model['empty_dialogue'] = b.one(r'open_dialogue_box\("([^"]+)"\)', holder, 'dropped empty dialogue')
    default_pickup = b.one(r'^export \(bool\) var can_pickup := (true|false)$', holder, 'dropped pickup default')
    default_reset = b.one(r'^export \(bool\) var reset_when_consumed := (true|false)$', source, 'dropped reset default')
    model['default_bits'] = int(default_pickup == 'true') | (int(default_reset == 'true') << 1)
    body, area = prefab['StaticBody2D'], prefab['interact']
    model['collision'] = [body.props.get('collision_layer', 1), body.props.get('collision_mask', 1),
                          area.props.get('collision_layer', 1), area.props.get('collision_mask', 1),
                          int(area.props.get('monitoring', True))]
    animation = prefab['AnimationPlayer']
    clips = []
    for name in ['RESET', 'blink']:
        props = owner_of(animation, 'anims/' + name).sub_resource(animation.props['anims/' + name], 'Animation')
        allowed = {'resource_name', 'length', 'loop', 'step'} | {'tracks/0/' + v for v in
                  ['type', 'path', 'interp', 'loop_wrap', 'imported', 'enabled', 'keys']}
        require(set(props) <= allowed and props['tracks/0/type'] == 'value'
                and type(props['tracks/0/path']) is NodePathValue
                and props['tracks/0/path'].path == 'Sprite:visible'
                and props['tracks/0/interp'] == 1 and props['tracks/0/loop_wrap'] is True
                and props['tracks/0/imported'] is False and props['tracks/0/enabled'] is True,
                'Dropped item unmapped visibility track')
        keys = props['tracks/0/keys']
        fields(keys, 'times transitions update values', 'Dropped visibility keys')
        require(len(keys['times']) == len(keys['values']) == len(keys['transitions'])
                and all(v == 1 for v in keys['transitions']) and keys['update'] in [0, 1]
                and all(type(v) is bool for v in keys['values']), 'Dropped visibility key modes')
        clips.append(dict(id=b.stable('dropped-clip:' + name), role=len(clips) + 1, name=name,
                          duration=f.f32(props['length']), loop=props.get('loop', False),
                          keys=[dict(time=f.f32(t), value=int(v), update=keys['update'])
                                for t, v in zip(keys['times'], keys['values'])]))
    model['clips'] = clips
    proof['sources'].update(s.digests())
    proof['mapped_execution'] = dict(collect_curves=['quart_out', 'quad_in'],
                                    revert_curve='elastic_out',
                                    uid='Original Item.get_uid OS randomize/randi/unique generated ledger',
                                    inventory='Original ordered party first non-full; key bypass; full preview still constructs Item')
    return model, proof


def source(file=RECIPE):
    recipe = json.loads(Path(file).read_bytes(), object_pairs_hook=unique)
    fields(recipe, 'schema kind commit scene sources methods bindings semantics scope', 'Dropped reviewed recipe')
    require(type(recipe['schema']) is int and recipe['schema'] == 1
            and recipe['kind'] == 'encore.catacombs-dropped-items.reviewed-source'
            and recipe['commit'] == PIN, 'Dropped reviewed source identity')
    model, proof = build_model()
    require(recipe['scene'] == model['scene'] and type(recipe['scene']) is int
            and recipe['sources'] == proof['sources'] and recipe['methods'] == proof['methods']
            and recipe['bindings'] == model['bindings'] and recipe['semantics'] == proof['mapped_execution']
            and type(recipe['scope']) is str and bool(recipe['scope']), 'Dropped reviewed source changed')
    proof.update(recipe_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(), scope=recipe['scope'])
    return recipe, model, proof


def encode(model):
    fields(model, 'scene scene_source name paths bindings templates geometry policy capacity success_dialogue full_dialogue empty_dialogue default_bits collision clips',
           'Dropped resource model')
    fields(model['paths'], ' '.join(PATHS), 'Dropped source paths')
    fields(model['policy'], ' '.join(POLICY), 'Dropped scalar policy')
    b.integer(model['scene'], 1, 65535)
    b.integer(model['capacity'], 1, 4096)
    b.integer(model['default_bits'], 0, 3)
    require(type(model['collision']) is list and len(model['collision']) == 5, 'Dropped collision model')
    for value in model['collision'][:4]:
        b.integer(value, 0, (1 << 20) - 1)
    b.integer(model['collision'][4], 0, 1)
    for value in model['policy'].values():
        b.number(value)
    fields(model['geometry'], 'StaticBody2D/CollisionShape2D interact/CollisionShape2D', 'Dropped shape targets')
    require(type(model['bindings']) is list and type(model['templates']) is list
            and type(model['clips']) is list, 'Dropped resource model arrays')
    for template in model['templates']:
        fields(template, 'id source yaml document texture size doses key_item', 'Dropped template model')
        require(type(template['key_item']) is bool and type(template['doses']) is int,
                'Dropped template source field types')
        b.integer(template['id'], 1, 0xfffffffe)
        b.integer(template['doses'], 1, 65535)
        require(type(template['size']) is list and len(template['size']) == 2,
                'Dropped template dimensions')
        for dimension in template['size']:
            b.integer(dimension, 1, 1024)
    for owner in model['bindings']:
        fields(owner, 'id order path name parent template position flag flag_kind', 'Dropped owner model')
        require(owner['flag_kind'] == 'object', 'Dropped unsupported flag domain')
        for name in ['id', 'order', 'template']:
            b.integer(owner[name], 1, 0xfffffffe if name != 'order' else 65535)
        require(type(owner['position']) is list and len(owner['position']) == 2,
                'Dropped owner position model')
        for value in owner['position']:
            b.number(value)
    for shape in model['geometry'].values():
        fields(shape, 'position extents', 'Dropped shape model')
        for values in shape.values():
            require(type(values) is list and len(values) == 2, 'Dropped shape vector model')
            for value in values:
                b.number(value)
    for clip in model['clips']:
        fields(clip, 'id role name duration loop keys', 'Dropped clip model')
        require(type(clip['loop']) is bool, 'Dropped clip loop type')
        b.integer(clip['id'], 1, 0xfffffffe)
        b.integer(clip['role'], 1, 2)
        b.number(clip['duration'])
        for key in clip['keys']:
            fields(key, 'time value update', 'Dropped key model')
            b.number(key['time'])
            b.integer(key['value'], 0, 1)
            b.integer(key['update'], 0, 1)
    pool = bytearray(b'\0')
    offsets = {'': 0}
    def string(value):
        b.text(value)
        require(not value.startswith('/') and all(v not in ['', '.', '..'] for v in value.split('/')),
                'Dropped resource safe source path')
        if value not in offsets:
            offsets[value] = len(pool)
            pool.extend(value.encode('ascii') + b'\0')
        return offsets[value]
    meta = struct.pack('<20I', model['scene'], string(model['scene_source']), string(model['name']),
                       *[string(model['paths'][k]) for k in ['prefab', 'script', 'holder', 'flag', 'inventory', 'item']],
                       model['capacity'], *model['collision'],
                       *[string(model[k]) for k in ['success_dialogue', 'full_dialogue', 'empty_dialogue']],
                       model['default_bits'], 0)
    policy = b''.join(struct.pack('<Id', i + 1, model['policy'][name]) for i, name in enumerate(POLICY))
    templates = b''.join(struct.pack('<8I', t['id'], string(t['source']), t['doses'], int(t['key_item']),
                                    string(t['texture']), *t['size'], 0) for t in model['templates'])
    owners = b''.join(struct.pack('<8I2f', v['id'], v['order'], *[string(v[k]) for k in ['path', 'name', 'parent']],
                                 v['template'], string(v['flag']), model['default_bits'] | 4, *v['position'])
                      for v in model['bindings'])
    shapes = b''.join(struct.pack('<I4fI', i + 1, *v['position'], *v['extents'], string(path))
                      for i, (path, v) in enumerate(model['geometry'].items()))
    clips = bytearray()
    keys = bytearray()
    for clip in model['clips']:
        clips.extend(struct.pack('<6If', clip['id'], clip['role'], string(clip['name']), len(keys) // 12,
                                 len(clip['keys']), int(clip['loop']), clip['duration']))
        for key in clip['keys']:
            keys.extend(struct.pack('<f2I', key['time'], key['value'], key['update']))
    blob = f.pack_container(MAGIC, model['scene'], [meta, policy, templates, owners, shapes, clips, keys, pool], STRIDES, LIMIT)
    decode(blob)
    return blob


def decode(blob):
    scene, sections = f.unpack_container(blob, MAGIC, STRIDES, LIMIT)
    meta, policy, templates, owners, shapes, clips, keys, pool = sections
    require(0 < scene <= 65535 and len(meta) == 80 and len(policy) == len(POLICY) * 12
            and 0 < len(templates) <= 256 * 32 and 0 < len(owners) <= 4096 * 40
            and len(shapes) == 48 and len(clips) == 56 and 0 < len(keys) <= 4096 * 12
            and 1 < len(pool) <= 65536, 'Dropped resource section counts')
    words = struct.unpack('<20I', meta)
    require(words[0] == scene and 0 < words[9] <= 4096 and all(v < (1 << 20) for v in words[10:14])
            and words[14] <= 1 and words[18] <= 3 and not words[19], 'Dropped resource metadata')
    require(pool[0] == pool[-1] == 0, 'Dropped resource pool boundary')
    strings = {}
    distinct = set()
    cursor = 0
    while cursor < len(pool):
        end = pool.find(b'\0', cursor)
        require(end >= cursor, 'Dropped resource unterminated string')
        value = pool[cursor:end].decode('ascii')
        b.text(value, True)
        require(value not in distinct and ((cursor == 0 and not value) or
                (cursor > 0 and value and not value.startswith('/')
                 and all(v not in ['', '.', '..'] for v in value.split('/')))), 'Dropped resource path/text')
        strings[cursor] = value
        distinct.add(value)
        cursor = end + 1
    used = {0}
    def string(at):
        require(at in strings and strings[at], 'Dropped resource string reference')
        used.add(at)
        return strings[at]
    for at in [*words[1:9], *words[15:18]]:
        string(at)
    values = []
    for i, (role, value) in enumerate(struct.iter_unpack('<Id', policy)):
        require(role == i + 1 and math.isfinite(value) and abs(value) <= 1e6,
                'Dropped resource scalar role/value')
        values.append(value)
    require(all(values[i] > 0 for i in [0, 2, 9, 10, 11, 12, 13, 19, 20])
            and all(values[i] >= 0 for i in [1, 3, 4, 5, 6, 14, 15, 16, 17, 18])
            # Original rand_range(0,47) exceeds its 40-frame animation.
            # AnimatedSprite::set_frame clamps that initial draw; keep both
            # original domains instead of narrowing the random distribution.
            and values[17] <= values[18] and 0 < values[20] <= 1024
            and values[20] == int(values[20]), 'Dropped resource scalar domain')
    for index in [25, 27, 29]:
        require((values[index], values[index + 1]) in [(3, 1), (4, 0), (6, 1)],
                'Dropped resource unsupported interpolation capability')
    ids = set()
    names = set()
    for row in struct.iter_unpack('<8I', templates):
        identity, name, doses, key, texture, width, height, reserved = row
        source_name = string(name)
        string(texture)
        require(0 < identity < 0xffffffff and identity == b.stable('dropped-template:' + source_name)
                and identity not in ids and source_name not in names and 0 < doses <= 65535
                and key <= 1 and 0 < width <= 1024 and 0 < height <= 1024 and not reserved,
                'Dropped resource item template')
        ids.add(identity)
        names.add(source_name)
    owner_ids = set()
    paths = set()
    previous = 0
    for row in struct.iter_unpack('<8I2f', owners):
        identity, order, path, name, parent, template, flag, bits, x, y = row
        path, name, parent, flag = map(string, [path, name, parent, flag])
        require(0 < identity < 0xffffffff and identity == b.stable('dropped-item:' + path)
                and identity not in owner_ids and path not in paths and previous < order <= 65535
                and path == parent + '/' + name and flag == string(words[2]) + '/' + name
                and template in ids and bits <= 7 and bits & 4
                and all(math.isfinite(v) and abs(v) <= 1e6 for v in [x, y]),
                'Dropped resource owner identity/binding')
        owner_ids.add(identity)
        paths.add(path)
        previous = order
    shape_paths = set()
    for index, row in enumerate(struct.iter_unpack('<I4fI', shapes)):
        require(row[0] == index + 1 and all(math.isfinite(v) and abs(v) <= 1e6 for v in row[1:5])
                and row[3] > 0 and row[4] > 0, 'Dropped resource shape')
        path = string(row[5])
        require('/' in path and path not in shape_paths, 'Dropped resource duplicate/invalid child shape binding')
        shape_paths.add(path)
    first = 0
    clip_names = set()
    clip_ids = set()
    for identity, role, name, begin, count, loop, duration in struct.iter_unpack('<6If', clips):
        name = string(name)
        require(name in ['RESET', 'blink'] and name not in clip_names
                and role == len(clip_names) + 1
                and identity == b.stable('dropped-clip:' + name) and identity not in clip_ids
                and begin == first and 0 < count <= 1024 and first + count <= len(keys) // 12
                and loop <= 1 and math.isfinite(duration) and 0 < duration <= 60,
                'Dropped resource clip')
        previous = -1
        for time, value, update in struct.iter_unpack('<f2I', keys[12 * begin:12 * (begin + count)]):
            require(math.isfinite(time) and previous < time <= duration and time >= 0
                    and value <= 1 and update <= 1, 'Dropped resource visibility key')
            previous = time
        first += count
        clip_names.add(name)
        clip_ids.add(identity)
    require(first == len(keys) // 12 and used == set(strings), 'Dropped resource unowned keys/strings')
    return dict(scene=scene, metadata=words, policy=values, strings=strings,
                templates=list(struct.iter_unpack('<8I', templates)),
                owners=list(struct.iter_unpack('<8I2f', owners)))


if __name__ == '__main__':
    model, proof = derive()
    print(json.dumps(dict(bindings=model['bindings'], templates=[
        {k: v[k] for k in ['id', 'source', 'doses', 'size']} for v in model['templates']],
        geometry=model['geometry'], source_count=len(proof['sources'])), indent=2))
