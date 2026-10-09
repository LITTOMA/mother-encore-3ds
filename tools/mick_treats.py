#!/usr/bin/env python3
"""Compile Podunk Mick into ENCMIK01.

Build-time adapter only. Spoken trees, the south-fence bark, 4dir clips and the
field-enter RNG ledger are data. Telepathy, the heart emote and other NPC
movement stay out. DialogueBox graphics are not restored; phrase bodies are
plain bottom-screen text with [PartyLead] substituted at runtime.
Outputs: data/podunk.encmick and graphics/world/mick/mick.t3x.
"""
from __future__ import annotations
import argparse, csv, hashlib, io, json, math, re, struct, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.extract_battle_entry import Extractor, PIN, require  # noqa: E402
from tools.godot_text import Document, Rect2, Vec2, instantiate, owner_of  # noqa: E402

IR = 'content/mick-treats.json'
REVIEW = 'reports/mick-treats/source-review.json'
RECEIPT = 'content/asset-receipts/graphics/world/mick/source.json'
PACK = 'data/podunk.encmick'
SCENE = 'Maps/podunk/podunk.tscn'
NPC = 'Nodes/Reusables/npc.tscn'
SPRITE = 'Graphics/Character Sprites/Npcs/4dir/mick.png'
ANIM = 'Data/Animations/4dir.yaml'
DIALOGUE_TABLE = 'Translations/TranslatedText/dialogue_Podunk - sheet.csv'
OUTPUT = 'graphics/world/mick/mick.t3x'
ACTOR_PATH = 'Objects/NPCS/npc21'
BARK_PATH = 'Cutscenes/Cutscene Area11'
BARK_DIALOGUE = 'Data/Dialogue/Podunk/cutscenes/mick_bark.yaml'
ITEM = 'DogTreats'
REQUIRE = 'got_dog_treats'
CONSUME = 'gave_treats'
RAY = 16.0
MAGIC = b'ENCMIK01'
LIMIT = 64 * 1024
SECTIONS = ('Bytes', 'Strings', 'Texture', 'Actor', 'Programmes', 'Commands', 'Texts', 'Clips', 'Rng')
STRIDES = (1, 8, 12, 200, 16, 20, 8, 8, 32)
OPCODES = {'ShowText': 1, 'AwaitText': 2, 'RemoveKeyItem': 3, 'SetFlag': 4, 'End': 5,
           'Choice': 6, 'PlaySound': 7, 'JumpActor': 8, 'TurnActor': 9, 'MovePlayer': 10, 'Wait': 11}
PROGRAMME_KINDS = {'Talk': 1, 'Bark': 2}
RNG_KINDS = {'Randi': 1, 'Randf': 2, 'RandRange': 3, 'ArmWander': 4}
SPOKEN = (('', 'Podunk/woof'), ('mick_scratch', 'Podunk/woof_secret'), ('mick_telepathy', 'Podunk/woof_deal'),
          ('got_dog_treats', 'Podunk/woof_treats'), ('gave_treats', 'Podunk/woof_animals'))
PHRASE_KEYS = {'name', 'text', 'goto', 'options', 'soundeffect', 'setflags', 'removeitem', 'talkeremote',
               'actors', 'talker', 'actorsjump', 'actorsturn', 'actorsmove', 'wait', 'autoadvance', 'caninput', 'showbox'}
READY_SCRIPTS = {'Scripts/misc/grass spawner.gd', 'Scripts/misc/birds.gd', 'Scripts/misc/butterfly.gd', 'Scripts/misc/sparkles.gd'}
CANVAS = {'Node2D', 'Sprite', 'Position2D', 'YSort', 'TileMap', 'Area2D', 'StaticBody2D', 'KinematicBody2D',
          'CollisionShape2D', 'CollisionPolygon2D', 'VisibilityNotifier2D', 'VisibilityEnabler2D', 'AnimatedSprite',
          'RayCast2D', 'Camera2D', 'AudioStreamPlayer2D', 'TextureRect', 'HBoxContainer', 'Label', 'ReferenceRect'}
LOCALES = (('en', 'en'), ('zh_Hans_CN', 'zh_CN'))
LICENSE_REVIEW = ('Pinned upstream LICENSE permits game-related forks/modifications; original Mick art '
                  'is used by this Mother: Encore port under upstream terms, not relicensed as MIT.')


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def canonical(value):
    return json.dumps(value, sort_keys=True, ensure_ascii=False, separators=(',', ':'), allow_nan=False)


def read_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes((json.dumps(value, ensure_ascii=False, indent=2) + '\n').encode('utf-8'))


def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


def table(ex, path):
    rows = {}
    for row in csv.DictReader(io.StringIO(ex.text(path))):
        require(row['key'] not in rows, 'Duplicate translation key in ' + path)
        rows[row['key']] = row
    return rows


def plain(raw):
    """Strip DialogueBox control tags for the field bottom-screen display."""
    text = raw.replace('\\n', '\n')
    text = re.sub(r'\[WAIT@\]', '\n', text)
    text = re.sub(r'\[@\]', '', text)
    text = re.sub(r'\[/?color\]', '', text)
    text = re.sub(r'\n+', '\n', text).strip()
    require(text and '\0' not in text, 'Empty Mick phrase after tag strip')
    return text


def parent_path(path):
    if path == '.':
        return None
    return path.rsplit('/', 1)[0] if '/' in path else '.'


def world(nodes, path):
    """CanvasItem world origin and scale. A non-canvas parent restarts the chain."""
    chain = []
    cursor = path
    while cursor is not None:
        chain.append(cursor)
        cursor = parent_path(cursor)
    pos, scale = Vec2(0, 0), Vec2(1, 1)
    for current in reversed(chain):
        node = nodes[current]
        if node.type not in CANVAS:
            pos, scale = Vec2(0, 0), Vec2(1, 1)
            continue
        local = node.props.get('position', Vec2(0, 0))
        sc = node.props.get('scale', Vec2(1, 1))
        rot = float(node.props.get('rotation', 0) or 0)
        require(rot == 0 and isinstance(local, Vec2) and isinstance(sc, Vec2) and sc.x > 0 and sc.y > 0,
                'Unreviewed transform at ' + current)
        pos = Vec2(pos.x + local.x * scale.x, pos.y + local.y * scale.y)
        scale = Vec2(scale.x * sc.x, scale.y * sc.y)
    return pos, scale


def notifier_rect(nodes, root):
    found = [n for n in nodes.values() if n.type == 'VisibilityNotifier2D' and (n.path == root or n.path.startswith(root + '/'))]
    require(len(found) == 1, 'Visibility notifier count at ' + root)
    node = found[0]
    rect = node.props.get('rect', Rect2(-10, -10, 20, 20))
    require(isinstance(rect, Rect2) and rect.w > 0 and rect.h > 0, 'Unreviewed notifier rect at ' + node.path)
    pos, scale = world(nodes, node.path)
    return dict(x=f32(pos.x + rect.x * scale.x), y=f32(pos.y + rect.y * scale.y),
                w=f32(rect.w * scale.x), h=f32(rect.h * scale.y))


def contract(ex):
    npc = ex.text('Scripts/Main/npc.gd')
    require('character_sprite.set_sprite("res://Graphics/Character Sprites/%s.png" % sprite)' in npc, 'NPC sprite path contract changed')
    require('yaml_path = "res://Data/Animations/4dir.yaml"' in npc, 'NPC 4dir animation default changed')
    require('if no_collision:\n\t\t$CollisionShape2D.disabled = true' in npc, 'NPC collision disable contract changed')
    sprite_script = ex.text('Scripts/Main/character_sprite.gd')
    require('offset.y = -int(texture.get_height()/float(vframes*2))' in sprite_script
            and 'offset += Vector2(_json_data["offset"][0], _json_data["offset"][1])' in sprite_script,
            'Character sprite auto-offset contract changed')
    anim = ex.yaml(ANIM)
    require(anim.get('size') == [5, 4], '4dir sheet size changed')
    idle = anim['animations']['Idle']['directions']
    require(idle[0][1][0] == 2, 'South idle frame changed')
    ex.data(SPRITE)
    npc = ex.text('Scripts/Main/npc.gd')
    require('export var speed := 64' in npc and 'export var walk_frequency := 2' in npc, 'NPC speed defaults changed')
    require('wander_timer.wait_time = rand_range(0.1,walk_frequency)' in npc, 'NPC wander arm contract changed')
    require('if randi()%2 == 1:' in npc and 'wander_radius.shape.radius/2' in npc, 'NPC wander pick contract changed')
    item = ex.yaml('Data/Items/DogTreats.yaml')
    require(item.get('keyitem') is True, 'DogTreats is no longer a key item')
    house = read_json(ROOT / 'content/native-house.json')
    require(house['interaction']['ray_length'] == RAY, 'Player interact ray length changed')


def build(root=ROOT):
    root = Path(root)
    ex = Extractor(root)
    contract(ex)
    docs = {}

    def load(path):
        if path not in docs:
            docs[path] = Document(path, ex.text(path))
        return docs[path]

    nodes = {n.path: n for n in instantiate(load, SCENE)}
    require(ACTOR_PATH in nodes and nodes[ACTOR_PATH].scene == NPC, 'Mick npc21 missing from podunk')
    mick = nodes[ACTOR_PATH]
    require(mick.props.get('sprite') == 'Npcs/4dir/mick', 'Mick sprite identity changed')
    require(mick.props.get('initial_dir') == Vec2(0, 1), 'Mick initial facing changed')
    require(mick.props.get('staring') is True and mick.props.get('wander') is True, 'Mick wander/staring changed')
    require(mick.props.get('walk_frequency') == 1, 'Mick walk frequency changed')
    dialog = mick.props.get('_all_dialog')
    spoken = [['', mick.props.get('dialog')]] + list(dialog)
    require(spoken == [list(row) for row in SPOKEN], 'Mick spoken dialogue binding changed')
    # The Podunk instance position replaces npc.tscn's root position; do not add it again.
    require(mick.props.get('no_collision') in (None, False), 'Mick collision is disabled')
    player_mask = int(read_json(root / 'content/podunk-field.json')['player']['collision_mask'])
    require(int(mick.props.get('collision_layer', 1)) & player_mask, 'Mick body does not collide with the player mask')
    position = [f32(mick.props['position'].x), f32(mick.props['position'].y)]

    def origin(path):
        cursor, total = path, Vec2(0, 0)
        while True:
            node = nodes[cursor]
            local = node.props.get('position', Vec2(0, 0))
            require(isinstance(local, Vec2), 'Non-vector position at ' + cursor)
            total = Vec2(total.x + local.x, total.y + local.y)
            if cursor == ACTOR_PATH:
                break
            cursor = cursor.rsplit('/', 1)[0]
        return total

    npc_doc = load(NPC)
    body_shape = nodes[ACTOR_PATH + '/CollisionShape2D']
    body = npc_doc.sub_resource(body_shape.props['shape'], 'RectangleShape2D')
    body_at = origin(body_shape.path)
    collision_center = [f32(body_at.x), f32(body_at.y)]
    collision_extents = [f32(abs(body['extents'].x)), f32(abs(body['extents'].y))]
    interact_shape = nodes[ACTOR_PATH + '/interact/CollisionShape2D']
    interact = npc_doc.sub_resource(interact_shape.props['shape'], 'RectangleShape2D')
    interact_at = origin(interact_shape.path)
    center = [f32(interact_at.x), f32(interact_at.y)]
    extents = [f32(abs(interact['extents'].x)), f32(abs(interact['extents'].y))]
    char_at = origin(ACTOR_PATH + '/CharacterSprite')
    # set_spritesheet overwrites the scene offset: -int(height / (rows * 2)) plus the yaml offset.
    anim = ex.yaml(ANIM)
    columns, rows = anim['size']
    from PIL import Image
    with Image.open(root / 'upstream/MOTHER-Encore' / SPRITE) as image:
        size = image.size
    require(size[0] % columns == 0 and size[1] % rows == 0, 'Mick sheet does not match 4dir size')
    sheet_offset = anim.get('offset', [0, 0])
    require(sheet_offset == [0, 0], '4dir sheet offset changed')
    sprite_draw = [f32(char_at.x + sheet_offset[0]), f32(char_at.y - int(size[1] / float(rows * 2)) + sheet_offset[1])]
    frame = anim['animations']['Idle']['directions'][0][1][0]
    clips = []
    for name, anim_id in (('Idle', 0), ('Walk', 1), ('Talk', 2)):
        directions = anim['animations'][name]['directions']
        require(len(directions) == 4 and anim['animations'][name].get('type') == 0, '4dir clip changed: ' + name)
        for direction, row in enumerate(directions):
            require(isinstance(row, list) and len(row) >= 2, '4dir direction changed: ' + name)
            for sample in row[1:]:
                require(isinstance(sample, list) and len(sample) == 2, '4dir sample changed: ' + name)
                milliseconds = int(round(float(sample[1]) * 1000))
                require(0 < milliseconds <= 10000, '4dir sample duration rejected')
                clips.append(dict(anim=anim_id, direction=direction, frame=int(sample[0]), milliseconds=milliseconds))
    clip_counts = {0: 0, 1: 0, 2: 0}
    for clip in clips:
        clip_counts[clip['anim']] += 1
    near_node = nodes[ACTOR_PATH + '/NearPlayerArea/CollisionShape2D']
    view_node = nodes[ACTOR_PATH + '/ViewArea/CollisionShape2D2']
    wander_node = nodes[ACTOR_PATH + '/WanderRadius/CollisionShape2D2']
    ray = nodes[ACTOR_PATH + '/RayCast2D']
    near_kind, near = owner_of(near_node, 'shape').sub_type(near_node.props['shape']), None
    near = owner_of(near_node, 'shape').sub_resource(near_node.props['shape'], 'RectangleShape2D')
    view = owner_of(view_node, 'shape').sub_resource(view_node.props['shape'], 'CircleShape2D')
    wander = owner_of(wander_node, 'shape').sub_resource(wander_node.props['shape'], 'CircleShape2D')
    require(near_kind == 'RectangleShape2D' and view['radius'] == 44 and wander['radius'] == 44, 'Mick area shapes changed')
    require(ray.props.get('enabled') is True and ray.props.get('cast_to') == Vec2(0, 25) and ray.props.get('collision_mask') == 1537,
            'Mick ray contract changed')
    near_at = origin(near_node.path)
    view_at = origin(view_node.path)
    ray_at = origin(ray.path)
    bark = nodes[BARK_PATH]
    require(bark.scene == 'Nodes/Reusables/CutsceneArea.tscn' and bark.props.get('dialog') == 'Podunk/cutscenes/mick_bark'
            and bark.props.get('disappear_flag') == CONSUME, 'mick_bark binding changed')
    bark_shape = nodes[BARK_PATH + '/CollisionShape2D']
    bark_rect = owner_of(bark_shape, 'shape').sub_resource(bark_shape.props['shape'], 'RectangleShape2D')
    bark_pos, bark_scale = world(nodes, bark_shape.path)
    bark_center = [f32(bark_pos.x), f32(bark_pos.y)]
    bark_extents = [f32(abs(bark_rect['extents'].x * bark_scale.x)), f32(abs(bark_rect['extents'].y * bark_scale.y))]
    sheets = {DIALOGUE_TABLE: table(ex, DIALOGUE_TABLE),
              'Translations/TranslatedText/dialogue_Podunk_cutscenes - sheet.csv': table(ex, 'Translations/TranslatedText/dialogue_Podunk_cutscenes - sheet.csv')}
    texts, commands, programmes = [], [], []

    def compile_tree(source_path):
        csv_rows = sheets['Translations/TranslatedText/dialogue_Podunk_cutscenes - sheet.csv' if '/cutscenes/' in source_path else DIALOGUE_TABLE]
        doc = {str(key): value for key, value in ex.yaml(source_path).items()}
        require(doc and '0' in doc, 'Dialogue lacks phrase 0: ' + source_path)
        local_texts, local_commands, started = [], [], {}

        def intern_text(key):
            require(isinstance(key, str) and key in csv_rows, 'Missing Mick translation ' + str(key))
            for native, column in LOCALES:
                require(column in csv_rows[key] and csv_rows[key][column], 'Missing Mick locale ' + key)
            if key not in intern_text.at:
                intern_text.at[key] = len(local_texts)
                localized = {native: plain(csv_rows[key][column]) for native, column in LOCALES}
                local_texts.append(dict(translation_key=key, localized=localized))
            return intern_text.at[key]
        intern_text.at = {}

        def emit(op, **fields):
            local_commands.append(dict(op=op, **fields))

        def phrase(label):
            require(label in doc and label not in started, 'Unreviewed dialogue flow %s %s' % (source_path, label))
            started[label] = len(local_commands)
            body = doc[label]
            require(isinstance(body, dict) and not (set(body) - PHRASE_KEYS),
                    'Unreviewed dialogue keys in %s phrase %s' % (source_path, label))
            if 'talkeremote' in body:
                require(body['talkeremote'] == 'heart', 'Unreviewed talker emote')
            if 'actors' in body:
                require(body['actors'] == {'ninten': 'leader', 'mick': ACTOR_PATH}, 'Unreviewed bark actors')
            if 'talker' in body:
                require(body['talker'] in ('mick', None), 'Unreviewed talker')
            if 'showbox' in body:
                require(body['showbox'] is False and 'text' not in body, 'DialogueBox showbox is not ported')
            if 'caninput' in body:
                require(body['caninput'] is False, 'Unreviewed caninput')
            if 'soundeffect' in body:
                require(isinstance(body['soundeffect'], str) and body['soundeffect'], 'Bad sound effect')
                emit('PlaySound', a='res://Audio/Sound effects/' + body['soundeffect'])
            flags = body.get('setflags')
            if flags is not None:
                if isinstance(flags, str):
                    flags = [flags]
                require(isinstance(flags, list) and flags and all(isinstance(flag, str) and flag for flag in flags), 'Bad setflags')
                for flag in flags:
                    emit('SetFlag', a=flag, b=1)
            if 'removeitem' in body:
                require(body['removeitem'] == ITEM, 'Mick can only remove DogTreats')
                emit('RemoveKeyItem', a=ITEM)
            if 'actorsjump' in body:
                spec = body['actorsjump']
                require(set(spec) == {'mick'} and set(spec['mick']) <= {'height', 'times'}
                        and spec['mick'].get('height') == 8 and spec['mick'].get('times') == 2, 'Unreviewed jump')
                emit('JumpActor', a=8, b=0.2, c=2)
            if 'actorsturn' in body:
                spec = body['actorsturn']
                require(set(spec) == {'mick'} and set(spec['mick']) == {'x', 'y', 'speed'}
                        and spec['mick']['x'] == 0 and spec['mick']['y'] == 1 and spec['mick']['speed'] == 0.1, 'Unreviewed turn')
                emit('TurnActor', a=0, b=1, c=0.1)
            if 'actorsmove' in body:
                spec = body['actorsmove']
                require(set(spec) == {'ninten'} and set(spec['ninten']) == {'movement', 'speed', 'animation', 'type'}, 'Unreviewed move')
                step = spec['ninten']['movement']
                require(spec['ninten']['speed'] == 32 and spec['ninten']['animation'] == 'Walk' and spec['ninten']['type'] == 'step'
                        and step == [{'x': 0, 'y': -16}], 'Unreviewed move step')
                emit('MovePlayer', a=0, b=-16, c=32)
            if 'wait' in body:
                require(isinstance(body['wait'], (int, float)) and body['wait'] > 0, 'Bad wait')
                emit('Wait', a=float(body['wait']))
            options = body.get('options')
            if options is not None:
                require('text' in body and 'goto' not in body and not body.get('autoadvance'), 'Choice phrase shape changed')
                visible, cancel = [], None
                for key, dest in options.items():
                    if key == 'cancel':
                        cancel = str(dest)
                        continue
                    visible.append((str(key), str(dest)))
                require(len(visible) == 2 and cancel == visible[1][1], 'Choice cancel must select option 1')
                emit('ShowText', a=intern_text(body['text']))
                slot = len(local_commands)
                emit('Choice', a=0, b=0, c=0, d=0)
                for _key, dest in visible:
                    phrase(dest)
                local_commands[slot] = dict(op='Choice', a=intern_text(visible[0][0]), b=started[visible[0][1]],
                                             c=intern_text(visible[1][0]), d=started[visible[1][1]])
                return
            if 'text' in body:
                emit('ShowText', a=intern_text(body['text']))
                if not body.get('autoadvance'):
                    emit('AwaitText')
            else:
                require(body.get('autoadvance') or 'goto' in body or 'wait' in body or 'actorsmove' in body,
                        'Phrase has no text or effect')
            if 'goto' in body:
                phrase(str(body['goto']))
            else:
                emit('End')

        phrase('0')
        require(set(started) == set(doc) and local_commands and local_commands[-1]['op'] == 'End',
                'Dialogue programme does not end: ' + source_path)
        return local_commands, local_texts

    def adopt(source_path, flag, kind):
        local_commands, local_texts = compile_tree(source_path)
        base = len(texts)
        texts.extend(local_texts)
        for command in local_commands:
            if command['op'] == 'ShowText':
                command['a'] += base
            elif command['op'] == 'Choice':
                command['a'] += base
                command['c'] += base
        programmes.append(dict(flag=flag, kind=kind, first=len(commands), count=len(local_commands)))
        commands.extend(local_commands)

    for flag, relative in SPOKEN:
        adopt('Data/Dialogue/' + relative + '.yaml', flag, 'Talk')
    adopt(BARK_DIALOGUE, '', 'Bark')
    bark_programme = len(programmes) - 1
    order = instantiate(load, SCENE)
    nodes = {n.path: n for n in order}
    children = {n.path: [] for n in order}
    for node in order:
        parent = parent_path(node.path)
        if parent is not None:
            children[parent].append(node.path)
    rand_pat = re.compile(r'\b(?:randi|randf|rand_range|randomize|seed)\s*\(')
    func_pat = re.compile(r'^func ([A-Za-z0-9_]+)\(.*?(?=^func |\Z)', re.S | re.M)
    draws = {}
    for node in order:
        script = node.script or ''
        if script in draws or not script.endswith('.gd'):
            continue
        text = ex.text(script)
        funcs = {match.group(1): match.group(0) for match in func_pat.finditer(text)}
        ready = funcs.get('_ready', '')
        called = re.findall(r'\b([A-Za-z_][A-Za-z0-9_]*)\(', ready)
        draws[script] = bool(rand_pat.search(ready)) or any(rand_pat.search(funcs.get(name, '')) for name in called)
    require({script for script, hit in draws.items() if hit} == READY_SCRIPTS, 'Unreviewed field _ready RNG')
    from tools.podunk_field import SourceRandom, godot_string_hash

    class FieldRandom(SourceRandom):
        def randf(self):
            self.randi()

        def rand_range(self, _start, _end):
            if self.randi() != 0:
                self.randi()
                self.randi()

    rng = FieldRandom(0)

    def replay(path):
        script = nodes[path].script
        if script == 'Scripts/misc/grass spawner.gd':
            rng.seed(godot_string_hash(nodes[path].name))
            rng.randi()
            rng.randi()
        elif script == 'Scripts/misc/birds.gd':
            rng.randi()
            rng.randf()
            rng.randi()
            rng.randi()
        elif script == 'Scripts/misc/butterfly.gd':
            rng.randi()
        elif script == 'Scripts/misc/sparkles.gd':
            rng.rand_range(0, 47)

    def ready(path):
        for child in children[path]:
            ready(child)
        replay(path)
    ready('.')
    state = rng.state
    events = []
    for node in order:
        if node.script == 'Scripts/misc/butterfly.gd':
            rect = notifier_rect(nodes, node.path)
            events.append(dict(kind='Randf', a=0, b=0, **rect))
            events.append(dict(kind='Randf', a=0, b=0, **rect))
        elif node.script == 'Scripts/Main/Enemy Spawner.gd':
            events.append(dict(kind='Randi', a=0, b=0, **notifier_rect(nodes, node.path)))
        elif node.script == 'Scripts/Main/npc.gd' and node.props.get('wander') is True:
            frequency = float(node.props.get('walk_frequency', 2))
            rect = notifier_rect(nodes, node.path)
            if node.path == ACTOR_PATH:
                events.append(dict(kind='ArmWander', a=0.1, b=frequency, **rect))
            else:
                events.append(dict(kind='RandRange', a=0.1, b=frequency, **rect))
    require(sum(event['kind'] == 'ArmWander' for event in events) == 1, 'Mick wander arm missing')
    texture = dict(source=SPRITE, size=list(size), columns=columns, rows=rows, output=OUTPUT, frame=frame)
    actor = dict(source_path=ACTOR_PATH, position=position, sprite_position=sprite_draw,
                 interact_center=center, interact_extents=extents,
                 collision_center=collision_center, collision_extents=collision_extents,
                 sort_y=position[1], item=ITEM, require_flag=REQUIRE, consume_flag=CONSUME,
                 ray_length=RAY, frame=frame, speed=64, walk_frequency=1, wander_radius=float(wander['radius']),
                 near_offset=[f32(near_at.x - position[0]), f32(near_at.y - position[1])],
                 near_extents=[f32(abs(near['extents'].x)), f32(abs(near['extents'].y))],
                 view_offset=[f32(view_at.x - position[0]), f32(view_at.y - position[1])],
                 view_radius=float(view['radius']),
                 ray_offset=[f32(ray_at.x - position[0]), f32(ray_at.y - position[1])],
                 bark_center=bark_center, bark_extents=bark_extents, bark_path=BARK_PATH,
                 initial_direction=[0, 1], rng_lo=state & 0xffffffff, rng_hi=(state >> 32) & 0xffffffff,
                 bark_programme=bark_programme,
                 idle_clip=0, idle_clip_count=clip_counts[0],
                 walk_clip=clip_counts[0], walk_clip_count=clip_counts[1],
                 talk_clip=clip_counts[0] + clip_counts[1], talk_clip_count=clip_counts[2])
    return dict(schema=1, kind='encore.mick-treats.source-ir', commit=PIN,
                scope=('Podunk Mick npc21 walks inside his wander radius, faces the player in the view circle, '
                       'and speaks woof, woof_secret, woof_deal, woof_treats and woof_animals. '
                       'Cutscene Area11 runs mick_bark and steps the player 16px north until gave_treats. '
                       'Field-enter RNG replays reviewed _ready draws and screen-entered draws without spawning enemies. '
                       'Telepathy, the heart emote, other NPC dialogue and movement, music and field battles stay deferred.'),
                sources=dict(sorted(ex.sources.items())), texture=texture, actor=actor, texts=texts,
                commands=commands, programmes=programmes, clips=clips, rng=events)


def recipe(ir):
    return dict(schema=1, kind='encore.mick-treats.asset-recipe', commit=PIN, licence_review=LICENSE_REVIEW,
                sources={ir['texture']['source']: ir['sources'][ir['texture']['source']]},
                resources={'mick': dict(source=ir['texture']['source'], size=ir['texture']['size'],
                                        grid=[ir['texture']['columns'], ir['texture']['rows']],
                                        output=ir['texture']['output'], format='rgba8', compression='none')})


def load(root=ROOT):
    root = Path(root)
    ir = read_json(root / IR)
    require(canonical(ir) == canonical(build(root)), 'Stale/unreviewed Mick treats IR')
    review = read_json(root / REVIEW)
    require(set(review) == {'schema', 'commit', 'ir_sha256', 'sources', 'scope', 'semantics', 'unsupported', 'unverified'} and
            review['schema'] == 1 and review['commit'] == PIN and review['ir_sha256'] == digest(root / IR) and
            review['sources'] == ir['sources'] and review['scope'] == ir['scope'], 'Mick treats review mismatch')
    return ir


def verify_receipt(root=ROOT, source=None):
    root = Path(root)
    ir = load(root)
    r = read_json(root / RECEIPT)
    require(set(r) == {'schema', 'commit', 'recipe', 'ir_sha256', 'producer_sha256', 'tex3ds_sha256', 'outputs', 'limits'} and
            r['schema'] == 1 and r['commit'] == PIN and r['recipe'] == recipe(ir) and r['ir_sha256'] == digest(root / IR) and
            r['producer_sha256'] == digest(root / 'tools/mick_treats.py'), 'Mick treats receipt mismatch')
    require(isinstance(r['tex3ds_sha256'], str) and re.fullmatch('[0-9a-f]{64}', r['tex3ds_sha256']) is not None,
            'Mick treats receipt lacks a real texture compiler')
    base = root / 'romfs' if source is None else Path(source)
    require(set(r['outputs']) == {OUTPUT}, 'Mick treats output set mismatch')
    for path, out in r['outputs'].items():
        target = base / path
        require(target.is_file() and target.stat().st_size == out['bytes'] and digest(target) == out['sha256'],
                'Mick treats texture differs from receipt: ' + path)
    return r


def stage_files(source):
    root = ROOT
    ir = load(root)
    verify_receipt(root, source)
    blob = (Path(source) / PACK).read_bytes()
    require(blob == encode(ir), 'Staged Mick pack differs from reviewed IR')
    return {Path(PACK): blob, Path(OUTPUT): (Path(source) / OUTPUT).read_bytes()}


def lower(ir):
    from tools.podunk_field import pack_container
    pool = bytearray()
    strings = []
    index = {}

    def s(value):
        require(isinstance(value, str) and '\0' not in value and len(value.encode()) <= 512, 'Invalid Mick string')
        if value not in index:
            data = value.encode()
            index[value] = len(strings)
            strings.append((len(pool), len(data)))
            pool.extend(data)
        return index[value]

    secs = {name: bytearray() for name in SECTIONS}
    t = ir['texture']
    secs['Texture'] += struct.pack('<I4H', s(t['output']), t['size'][0], t['size'][1], t['columns'], t['rows'])
    text_index = []
    for row in ir['texts']:
        text_index.append(len(text_index))
        secs['Texts'] += struct.pack('<2I', s(row['localized']['en']), s(row['localized']['zh_Hans_CN']))
    a = ir['actor']
    item_s = s(a['item']); req_s = s(a['require_flag']); con_s = s(a['consume_flag']); path_s = s(a['source_path'])
    floats = {'JumpActor': {'a', 'b'}, 'TurnActor': {'a', 'b', 'c'}, 'MovePlayer': {'a', 'b', 'c'}, 'Wait': {'a'}}

    def bits(value):
        return struct.unpack('<I', struct.pack('<f', float(value)))[0]

    def operand(command, name):
        if name not in command:
            return 0
        value = command[name]
        if isinstance(value, str):
            return s(value)
        if name in floats.get(command['op'], ()):
            return bits(value)
        return int(value)

    for c in ir['commands']:
        secs['Commands'] += struct.pack('<5I', OPCODES[c['op']], operand(c, 'a'), operand(c, 'b'), operand(c, 'c'), operand(c, 'd'))
    for programme in ir['programmes']:
        secs['Programmes'] += struct.pack('<4I', 0 if not programme['flag'] else s(programme['flag']),
                                          programme['first'], programme['count'], PROGRAMME_KINDS[programme['kind']])
    for clip in ir['clips']:
        secs['Clips'] += struct.pack('<4H', clip['anim'], clip['direction'], clip['frame'], clip['milliseconds'])
    for event in ir['rng']:
        secs['Rng'] += struct.pack('<I6fI', RNG_KINDS[event['kind']], event['x'], event['y'], event['w'], event['h'],
                                   float(event['a']), float(event['b']), 0)
    secs['Actor'] += struct.pack(
        '<6I8f2If5f18f10I', path_s, 0, a['frame'], item_s, req_s, con_s,
        a['position'][0], a['position'][1], a['sprite_position'][0], a['sprite_position'][1],
        a['interact_center'][0], a['interact_center'][1], a['interact_extents'][0], a['interact_extents'][1],
        0, len(ir['commands']), float(a['ray_length']),
        a['collision_center'][0], a['collision_center'][1], a['collision_extents'][0], a['collision_extents'][1],
        float(a['sort_y']),
        float(a['speed']), float(a['walk_frequency']), float(a['wander_radius']),
        a['near_offset'][0], a['near_offset'][1], a['near_extents'][0], a['near_extents'][1],
        a['view_offset'][0], a['view_offset'][1], float(a['view_radius']),
        a['ray_offset'][0], a['ray_offset'][1],
        a['bark_center'][0], a['bark_center'][1], a['bark_extents'][0], a['bark_extents'][1],
        float(a['initial_direction'][0]), float(a['initial_direction'][1]),
        int(a['rng_lo']), int(a['rng_hi']), int(a['bark_programme']),
        int(a['idle_clip']), int(a['idle_clip_count']), int(a['walk_clip']), int(a['walk_clip_count']),
        int(a['talk_clip']), int(a['talk_clip_count']), s(a['bark_path']))
    require(item_s == s(a['item']), 'Mick item string drifted')
    del text_index
    for off, length in strings:
        secs['Strings'] += struct.pack('<2I', off, length)
    secs['Bytes'] = pool
    return pack_container(MAGIC, 1, [secs[n] for n in SECTIONS], STRIDES, LIMIT)


def encode(ir):
    blob = lower(ir)
    parse(blob)
    return blob


def parse(blob):
    from tools.podunk_field import unpack_container
    scene, secs = unpack_container(blob, MAGIC, STRIDES, LIMIT)
    require(scene == 1, 'Mick pack scene identity')
    rows = {n: secs[i] for i, n in enumerate(SECTIONS)}
    count = {n: len(rows[n]) // STRIDES[i] for i, n in enumerate(SECTIONS)}
    require(count['Texture'] == 1 and count['Actor'] == 1 and len(rows['Actor']) == 200
            and 0 < count['Programmes'] <= 16 and 0 < count['Commands'] <= 128 and 0 < count['Texts'] <= 64
            and 0 < count['Clips'] <= 64 and 0 < count['Rng'] <= 512, 'Mick pack table counts')
    return rows


def review(ir):
    return dict(schema=1, commit=PIN, ir_sha256=digest(ROOT / IR), sources=ir['sources'], scope=ir['scope'],
                semantics={
                    'spawn': 'npc21 starts at Vector2(-88, 8) facing south and wanders inside the radius-44 circle',
                    'idle': '4dir Idle/Walk/Talk frames; south Idle frame 2 is the rest pose',
                    'interact': 'The last matching spoken row wins: woof, woof_secret, woof_deal, woof_treats, woof_animals',
                    'collision': 'The foot rectangle moves with Mick and blocks the player; the sprite y-sorts on his live position',
                    'bark': 'Cutscene Area11 starts mick_bark and steps the player 16px north while gave_treats is clear',
                    'rng': 'Initial PCG state is after reviewed _ready draws; screen overlap then consumes butterfly, enemy and wander draws',
                    'display': 'Field bottom-screen plain text and a two-line choice; house DialogueBox graphics are not restored'},
                unsupported=['Telepathy thoughts woof_food and woof_key', 'talkeremote heart is recognized and not drawn',
                             'ButtonPrompt over Mick', 'Other Podunk NPC dialogue and walking',
                             'Birds reroll when they leave the screen', 'Enemy bodies and field music',
                             'Mick RayCast2D; the foot box is the wander probe'],
                unverified=['Manual test suites', 'Emulator', 'Hardware', 'Audio audibility'])


def main():
    p = argparse.ArgumentParser()
    p.add_argument('action', choices=('extract', 'compile', 'verify'))
    p.add_argument('--tex3ds', type=Path)
    p.add_argument('--tex3ds-command')
    args = p.parse_args()
    try:
        if args.action == 'extract':
            ir = build()
            write_json(ROOT / IR, ir)
            write_json(ROOT / REVIEW, review(ir))
            print('Mick treats IR: actor', ir['actor']['source_path'], 'commands', len(ir['commands']))
        elif args.action == 'compile':
            ir = load()
            if args.tex3ds is not None:
                import shlex
                command = shlex.split(args.tex3ds_command) if args.tex3ds_command else None
                target = ROOT / 'romfs' / OUTPUT
                target.parent.mkdir(parents=True, exist_ok=True)
                src = ROOT / 'upstream/MOTHER-Encore' / ir['texture']['source']
                if command:
                    rel = lambda q: Path(q).resolve().relative_to(ROOT).as_posix()
                    subprocess.run(command + ['-f', 'rgba8', '-z', 'none', '-o', rel(target), rel(src)], check=True, cwd=ROOT)
                else:
                    subprocess.run([str(args.tex3ds), '-f', 'rgba8', '-z', 'none', '-o', str(target), str(src)], check=True)
                write_json(ROOT / RECEIPT, dict(schema=1, commit=PIN, recipe=recipe(ir), ir_sha256=digest(ROOT / IR),
                                                producer_sha256=digest(ROOT / 'tools/mick_treats.py'),
                                                tex3ds_sha256=digest(args.tex3ds),
                                                outputs={OUTPUT: dict(bytes=target.stat().st_size, sha256=digest(target))},
                                                limits='Pinned original lossless Mick sheet converted by real tex3ds; no hardware claim'))
            blob = encode(ir)
            (ROOT / 'romfs' / PACK).parent.mkdir(parents=True, exist_ok=True)
            (ROOT / 'romfs' / PACK).write_bytes(blob)
            if (ROOT / RECEIPT).is_file() and args.tex3ds is None:
                receipt = read_json(ROOT / RECEIPT)
                target = ROOT / 'romfs' / OUTPUT
                recorded = receipt['outputs'][OUTPUT]
                require(receipt.get('recipe') == recipe(ir) and target.is_file()
                        and target.stat().st_size == recorded['bytes'] and digest(target) == recorded['sha256'],
                        'Mick texture changed; rerun tex3ds')
                receipt['ir_sha256'] = digest(ROOT / IR)
                receipt['producer_sha256'] = digest(ROOT / 'tools/mick_treats.py')
                write_json(ROOT / RECEIPT, receipt)
            if (ROOT / RECEIPT).is_file():
                verify_receipt()
            elif args.tex3ds is None:
                print('Mick treats pack written without textures; console tex3ds compile is required before staging')
            print('Mick treats pack:', len(blob), 'bytes; checked ENCMIK01')
        else:
            ir = load()
            verify_receipt()
            require((ROOT / 'romfs' / PACK).read_bytes() == encode(ir), 'Stale Mick pack')
            print('Mick treats verified')
    except (ValueError, OSError, KeyError, TypeError, struct.error, subprocess.SubprocessError) as error:
        print('MICK TREATS ERROR:', error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
