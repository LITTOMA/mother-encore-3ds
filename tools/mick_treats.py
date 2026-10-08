#!/usr/bin/env python3
"""Compile Podunk Mick DogTreats → gave_treats (woof_treats) into ENCMIK01.

Build-time adapter only. Mick stays at his pinned npc21 spawn (wander deferred).
Field DialogueBox graphics are not restored; phrase bodies are plain bottom-screen
text with [PartyLead] substituted at runtime. Outputs: data/podunk.encmick and
graphics/world/mick/mick.t3x.
"""
from __future__ import annotations
import argparse, csv, hashlib, io, json, re, struct, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.extract_battle_entry import Extractor, PIN, require  # noqa: E402
from tools.godot_text import Document, Vec2, instantiate  # noqa: E402

IR = 'content/mick-treats.json'
REVIEW = 'reports/mick-treats/source-review.json'
RECEIPT = 'content/asset-receipts/graphics/world/mick/source.json'
PACK = 'data/podunk.encmick'
SCENE = 'Maps/podunk/podunk.tscn'
NPC = 'Nodes/Reusables/npc.tscn'
SPRITE = 'Graphics/Character Sprites/Npcs/4dir/mick.png'
ANIM = 'Data/Animations/4dir.yaml'
DIALOGUE = 'Data/Dialogue/Podunk/woof_treats.yaml'
DIALOGUE_TABLE = 'Translations/TranslatedText/dialogue_Podunk - sheet.csv'
OUTPUT = 'graphics/world/mick/mick.t3x'
ACTOR_PATH = 'Objects/NPCS/npc21'
ITEM = 'DogTreats'
REQUIRE = 'got_dog_treats'
CONSUME = 'gave_treats'
RAY = 16.0
MAGIC = b'ENCMIK01'
LIMIT = 32 * 1024
SECTIONS = ('Bytes', 'Strings', 'Texture', 'Actor', 'Commands', 'Texts')
STRIDES = (1, 8, 12, 68, 20, 8)
OPCODES = {'ShowText': 1, 'AwaitText': 2, 'RemoveKeyItem': 3, 'SetFlag': 4, 'End': 5}
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


def contract(ex):
    npc = ex.text('Scripts/Main/npc.gd')
    require('character_sprite.set_sprite("res://Graphics/Character Sprites/%s.png" % sprite)' in npc, 'NPC sprite path contract changed')
    require('yaml_path = "res://Data/Animations/4dir.yaml"' in npc, 'NPC 4dir animation default changed')
    anim = ex.yaml(ANIM)
    require(anim.get('size') == [5, 4], '4dir sheet size changed')
    idle = anim['animations']['Idle']['directions']
    require(idle[0][1][0] == 2, 'South idle frame changed')
    ex.data(SPRITE)
    doc = ex.yaml(DIALOGUE)
    require(doc == {
        '0': dict(name='DIALOGUE_PODUNK_WOOF_TREATS_SPEAKER_Mick', text='DIALOGUE_PODUNK_WOOF_TREATS_0', goto='2'),
        '2': dict(text='DIALOGUE_PODUNK_WOOF_TREATS_2', goto='3'),
        '3': dict(name='DIALOGUE_PODUNK_WOOF_TREATS_SPEAKER_Mick', talkeremote='heart',
                  text='DIALOGUE_PODUNK_WOOF_TREATS_3', removeitem='DogTreats', setflags='gave_treats'),
    }, 'woof_treats phrase graph changed')
    item = ex.yaml('Data/Items/DogTreats.yaml')
    require(item.get('keyitem') is True, 'DogTreats is no longer a key item')


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
    require(mick.props.get('staring') is True, 'Mick staring default changed')
    # Wander is deferred; spawn pose is the reviewed standing position.
    dialog = mick.props.get('_all_dialog')
    require(isinstance(dialog, list) and any(
        isinstance(row, list) and len(row) == 2 and row[0] == 'got_dog_treats' and row[1] == 'Podunk/woof_treats'
        for row in dialog), 'Mick got_dog_treats dialogue binding changed')
    position = [f32(mick.props['position'].x), f32(mick.props['position'].y)]
    npc_nodes = {n.path: n for n in instantiate(load, NPC)}
    body = npc_nodes['.']
    body_offset = body.props.get('position', Vec2(0, 0))
    require(isinstance(body_offset, Vec2), 'NPC body offset changed')
    world_body = [f32(position[0] + body_offset.x), f32(position[1] + body_offset.y)]
    npc_doc = load(NPC)
    interact = npc_nodes['interact']
    shape_node = npc_nodes['interact/CollisionShape2D']
    shape = npc_doc.sub_resource(shape_node.props['shape'], 'RectangleShape2D')
    interact_pos = interact.props.get('position', Vec2(0, 0))
    shape_pos = shape_node.props.get('position', Vec2(0, 0))
    center = [f32(world_body[0] + interact_pos.x + shape_pos.x), f32(world_body[1] + interact_pos.y + shape_pos.y)]
    extents = [f32(abs(shape['extents'].x)), f32(abs(shape['extents'].y))]
    char_sprite = npc_nodes['CharacterSprite']
    char_pos = char_sprite.props.get('position', Vec2(0, 0))
    # character_sprite.tscn default offset (0,-15); 4dir.yaml offset [0,0].
    sprite_draw = [f32(world_body[0] + char_pos.x), f32(world_body[1] + char_pos.y - 15)]
    anim = ex.yaml(ANIM)
    columns, rows = anim['size']
    frame = anim['animations']['Idle']['directions'][0][1][0]
    from PIL import Image
    with Image.open(root / 'upstream/MOTHER-Encore' / SPRITE) as image:
        size = image.size
    require(size[0] % columns == 0 and size[1] % rows == 0, 'Mick sheet does not match 4dir size')
    phrases = ex.yaml(DIALOGUE)
    csv_rows = table(ex, DIALOGUE_TABLE)
    texts = []
    for label in ('0', '2', '3'):
        key = phrases[label]['text']
        localized = {}
        for native, column in LOCALES:
            localized[native] = plain(csv_rows[key][column])
        texts.append(dict(label=label, translation_key=key, localized=localized))
    commands = [
        dict(op='ShowText', a=0), dict(op='AwaitText'),
        dict(op='ShowText', a=1), dict(op='AwaitText'),
        dict(op='RemoveKeyItem', a=ITEM),
        dict(op='SetFlag', a=CONSUME, b=1),
        dict(op='ShowText', a=2), dict(op='AwaitText'),
        dict(op='End'),
    ]
    texture = dict(source=SPRITE, size=list(size), columns=columns, rows=rows, output=OUTPUT, frame=frame)
    actor = dict(source_path=ACTOR_PATH, position=position, sprite_position=sprite_draw,
                 interact_center=center, interact_extents=extents, item=ITEM, require_flag=REQUIRE,
                 consume_flag=CONSUME, ray_length=RAY, frame=frame)
    return dict(schema=1, kind='encore.mick-treats.source-ir', commit=PIN,
                scope=('Podunk Mick npc21 at his pinned spawn: south idle frame, ray interact, '
                       'woof_treats linear programme that removes DogTreats and sets gave_treats; '
                       'wander, other woof trees, heart emote and mick_bark push-back are deferred'),
                sources=dict(sorted(ex.sources.items())), texture=texture, actor=actor, texts=texts, commands=commands)


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
    for c in ir['commands']:
        op = OPCODES[c['op']]
        aa = bb = cc = 0
        if c['op'] == 'ShowText':
            aa = text_index[c['a']]
        elif c['op'] == 'RemoveKeyItem':
            aa = item_s
        elif c['op'] == 'SetFlag':
            aa = con_s; bb = c['b']
        secs['Commands'] += struct.pack('<5I', op, aa, bb, cc, 0)
    secs['Actor'] += struct.pack(
        '<6I8f2If', path_s, 0, a['frame'], item_s, req_s, con_s,
        a['position'][0], a['position'][1], a['sprite_position'][0], a['sprite_position'][1],
        a['interact_center'][0], a['interact_center'][1], a['interact_extents'][0], a['interact_extents'][1],
        0, len(ir['commands']), float(a['ray_length']))
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
    require(count['Texture'] == 1 and count['Actor'] == 1 and 0 < count['Commands'] <= 64 and 0 < count['Texts'] <= 16,
            'Mick pack table counts')
    return rows


def review(ir):
    return dict(schema=1, commit=PIN, ir_sha256=digest(ROOT / IR), sources=ir['sources'], scope=ir['scope'],
                semantics={
                    'spawn': 'npc21 stays at its pinned Vector2(-88, 8); wander is deferred',
                    'idle': 'South Idle frame 2 from Data/Animations/4dir.yaml on the 5x4 sheet',
                    'interact': 'Player ray of length 16 against the NPC interact Area2D; only when got_dog_treats and not gave_treats',
                    'programme': 'woof_treats phrases 0→2→3 with RemoveKeyItem DogTreats then SetFlag gave_treats',
                    'display': 'Field bottom-screen plain text; house DialogueBox graphics are not restored'},
                unsupported=['Mick wander and walk_frequency', 'woof / woof_secret / woof_deal / woof_animals trees',
                             'Telepathy thoughts', 'talkeremote heart', 'mick_bark north push-back cutscene',
                             'ButtonPrompt over Mick', 'NPC collision body'],
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
