#!/usr/bin/env python3
"""House gift boxes (Present.tscn / ItemHolder.gd) from the pinned upstream.

All four House presents are drawn with their original sprite, flag state and
Sparkles effect. Only presents whose complete original interaction is ported
receive an effect programme; every other present is an explicit development
boundary. JSON is the offline review IR; the 3DS reads ENCPRS01 only.
"""
from __future__ import annotations
import argparse, csv, hashlib, io, json, re, struct, subprocess, sys, zlib
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.extract_battle_entry import Extractor, PIN, require
from tools.godot_text import Document, ExtRef, SubRef, Vec2, instantiate

IR = 'content/house-presents.json'
REVIEW = 'reports/house-presents/source-review.json'
RECEIPT = 'content/asset-receipts/graphics/world/presents/source.json'
PACK = 'data/opening.encpresent'
SCENE = 'Maps/podunk/Nintens House.tscn'
PRESENT = 'Nodes/Overworld/Objects/Present.tscn'
SPARKLES = 'Nodes/Reusables/Effects/Sparkles.tscn'
SCRIPTS = ('Scripts/Main/Present.gd', 'Scripts/Main/ItemHolder.gd', 'Scripts/Main/FlaggableObject.gd',
           'Scripts/misc/sparkles.gd', 'Scripts/Main/party/Player.gd', 'Scripts/global/Inventory.gd',
           'Scripts/global/Item.gd', 'Scripts/global/text_tools.gd', 'Scripts/UI/DialogueBox.gd')
TEXTURES = {'item': 'Graphics/Objects/Common/Present Box.png', 'map': 'Graphics/Objects/Common/Present Box Blue.png',
            'briefcase': 'Graphics/Objects/Common/Briefcase.png'}
SPARKLE_TEXTURE = 'Graphics/Objects/Common/Sparkles.png'
OUTPUTS = {'item': 'graphics/world/presents/present-box.t3x', 'map': 'graphics/world/presents/present-box-blue.t3x',
           'briefcase': 'graphics/world/presents/briefcase.t3x', 'sparkles': 'graphics/world/presents/sparkles.t3x'}
DIALOGUE = 'Data/Dialogue/%s.yaml'
DIALOGUE_TABLE = 'Translations/TranslatedText/dialogue_ItemDialogue - sheet.csv'
ITEM_TABLE = 'Translations/TranslatedText/items - sheet.csv'
GIFT_SOUND = 'Audio/Sound effects/Gift Box.mp3'
RECEIVED_SOUND = 'Audio/Sound effects/Item Received.mp3'
FIRST_TEXT_ID = 70
LOCALES = (('en', 'en'), ('zh_Hans_CN', 'zh_CN'))
LICENSE_REVIEW = ('Pinned upstream LICENSE permits game-related forks/modifications; original gift-box and sparkle art '
                  'is used by this Mother: Encore port under upstream terms, not relicensed as MIT.')
NONE = 0xffffffff
MAGIC = b'ENCPRS01'
LIMIT = 64 * 1024
SECTIONS = ('Bytes', 'Strings', 'Textures', 'Clips', 'FrameKeys', 'AudioKeys', 'SparkleFrames', 'SparkleOrder', 'Sparkles',
            'Objects', 'Templates', 'Commands', 'Programs', 'Texts')
STRIDES = (1, 8, 12, 24, 8, 8, 8, 4, 28, 52, 12, 20, 8, 8)
OPCODES = {'ShowText': 1, 'AwaitText': 2, 'BranchFlag': 3, 'GrantItem': 4, 'PlaySound': 5, 'SetFlag': 6, 'Jump': 7,
           'End': 8, 'PlayClip': 9}
CLIP_ROLES = {'Unwrapped': 1, 'Wrapped': 2}
POLICY_FLAG, POLICY_OBJECT_FLAG = 1, 2


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


def vec(value, label):
    require(isinstance(value, Vec2), 'Expected Vector2 for ' + label)
    return [float(value.x), float(value.y)]


def table(ex, path):
    rows = {}
    for row in csv.DictReader(io.StringIO(ex.text(path))):
        require(row['key'] not in rows, 'Duplicate translation key in ' + path)
        rows[row['key']] = row
    return rows


def contract(ex):
    """Exact GDScript statements this port mirrors; any upstream change fails closed."""
    present, holder, flaggable, sparkles, player, inventory, item, text, dialogue = (ex.text(p) for p in SCRIPTS)
    require('if _get_flag_status():\n\t\t$Sprite.frame = 4\n\t\t_update_state()' in present and
            '$AnimationPlayer.play("Unwrapped")' in present and
            'yield($AnimationPlayer,"animation_finished")\n\t$AnimationPlayer.play("Wrapped")' in present and
            '$Sparkles.stop()\n\t\t$Sparkles.hide()' in present and '$Sparkles.show()\n\t\t$Sparkles.play()' in present,
            'Present.gd open/sparkle state changed')
    for kind, path in TEXTURES.items():
        require(f'"{kind}":\n\t\t\tget_node("Sprite").texture = load("res://{path}")' in present, 'Present texture mapping changed: ' + kind)
    require('if dialog == "":\n\t\tdialog = "ItemDialogue/presentcheck"' in present and
            'if dialog_empty == "":\n\t\tdialog_empty = "ItemDialogue/presentempty"' in present, 'Present default dialogues changed')
    check = holder[holder.index('func _check_item():'):holder.index('func _warn_empty():')]
    require(check.index('_play_interact()') < check.index('global.item = Inventory.add_item_available(item)') <
            check.index('_set_flag_status()') < check.index('_update_state()') < check.index('uiManager.open_dialogue_box(dialog)'),
            'ItemHolder grant ordering changed')
    require('if item and (Inventory.has_inventory_space() or globaldata.get_item_data(item).get("keyitem", false)):' in check,
            'ItemHolder key-item admission changed')
    require('if not _get_flag_status():\n\t\t\t_check_item()\n\t\telse:\n\t\t\t_warn_empty()' in holder and
            'uiManager.open_dialogue_box("ItemDialogue/presentempty")' in holder, 'ItemHolder empty path changed')
    require('if flag != null and flag != "":' in flaggable and 'return globaldata.flags.get(flag, false)' in flaggable and
            'globaldata.set_flag(flag, value, emit_flag_updated_signal)' in flaggable and
            'globaldata.object_flags.get(global.currentScene.name + "/" + name, false)' in flaggable, 'FlaggableObject flag policy changed')
    require('frame = int(rand_range(0, 47))' in sparkles, 'Sparkles initial frame RNG changed')
    require('if "interact" in collide.name:' in player and 'global.party_call("try_to_turn",c)' in player and
            'c.interact()' in player, 'Player interaction dispatch changed')
    require('if item_data.get("keyitem", false):\n\t\treturn globaldata.key_items.add_item_by_name(item_name)' in inventory and
            'elif _type == InvType.KEY:\n\t\treturn 0' in inventory, 'Key inventory grant/capacity changed')
    require('randomize()\n\tvar new_uid = randi()' in item and 'self.doses = get_data().get("doses", 1)' in item, 'Item UID/dose default changed')
    require('result = _tr(global.item.get_data().get("name", ""))' in text and
            'result = get_item_or_skill_articles(global.item.get_data(), int(tag_param)) if global.item and tag_param else ""' in text and
            'var receiver: PartyMember = Inventory.get_item_owner(global.item, true)' in text, 'Item text tag semantics changed')
    require('return global.party[0] if default_as_leader else get_inventory_holder(INV_NAME_KEY)' in inventory, 'Key item receiver changed')
    pos = [dialogue.index('if _curr_phrase.has("%s"):' % k) for k in ('item', 'text', 'soundeffect', 'setflags')]
    require(pos == sorted(pos), 'DialogueBox phrase effect ordering changed')


def articles(raw, index):
    parts = raw.split(',')
    return parts[index] if index < len(parts) else ''


def bake(raw, name, article_raw):
    """Fixed-item lowering of [ItemName]/[ItemArtN]; [ItemReceiver] is the singleton leader."""
    text = raw.replace('[ItemReceiver]', '[PartyLead]').replace('[ItemName]', name)
    text = re.sub(r'\[ItemArt(\d+)\]', lambda m: articles(article_raw, int(m[1])), text)
    require(not re.search(r'\[(?:Item|Receiver|Lead)[A-Za-z]*\d*\]', text, re.I), 'Unlowered item/receiver tag: ' + raw)
    return text


def build(root=ROOT):
    ex = Extractor(root)
    contract(ex)
    ex.data('LICENSE')
    docs = {}

    def load(path):
        if path not in docs:
            docs[path] = Document(path, ex.text(path))
        return docs[path]
    nodes = {n.path: n for n in instantiate(load, SCENE)}
    present_doc = load(PRESENT)
    require(present_doc.ext_path(ExtRef(1), 'Texture') == TEXTURES['item'] and
            present_doc.ext_path(ExtRef(3), 'AudioStream') == GIFT_SOUND, 'Present scene resources changed')
    root_node = next(n for n in instantiate(load, PRESENT) if n.path == '.')
    require(root_node.props.get('dialog') == 'ItemDialogue/presentcheck' and root_node.props.get('type') == 'item',
            'Present scene defaults changed')
    clips = []
    player = next(n for n in instantiate(load, PRESENT) if n.path == 'AnimationPlayer')
    for role in ('Unwrapped', 'Wrapped'):
        anim = present_doc.sub_resource(player.props['anims/' + role], 'Animation')
        tracks = {anim['tracks/%d/path' % i].path: i for i in range(2) if 'tracks/%d/path' % i in anim}
        frame = tracks['Sprite:frame']
        fkeys = anim['tracks/%d/keys' % frame]
        require(anim['tracks/%d/interp' % frame] == 1 and fkeys['update'] == 1, 'Present frame track interpolation changed')
        audio = []
        if 'AudioStreamPlayer:playing' in tracks:
            akeys = anim['tracks/%d/keys' % tracks['AudioStreamPlayer:playing']]
            require(akeys['update'] == 1, 'Present audio track mode changed')
            audio = [[f32(t), int(bool(v))] for t, v in zip(akeys['times'], akeys['values'])]
        require(len(tracks) == (2 if role == 'Unwrapped' else 1), 'Unreviewed Present animation tracks')
        clips.append(dict(role=role, length=f32(anim['length']),
                          frames=[[f32(t), int(v)] for t, v in zip(fkeys['times'], fkeys['values'])], audio=audio))
    require([c['frames'][-1][1] for c in clips] == [4, 0], 'Present clip end frames changed')
    sprite_doc = load(SPARKLES)
    sprite_root = next(n for n in instantiate(load, SPARKLES) if n.path == '.')
    frames_res = sprite_doc.sub_resource(sprite_root.props['frames'], 'SpriteFrames')
    anims = frames_res['animations']
    require(len(anims) == 1 and anims[0]['name'] == 'Sparkle On' and anims[0]['loop'] is True, 'Sparkles animation set changed')
    regions, order = [], []
    for ref in anims[0]['frames']:
        atlas = sprite_doc.sub_resource(ref, 'AtlasTexture')
        require(sprite_doc.ext_path(atlas['atlas'], 'Texture') == SPARKLE_TEXTURE, 'Sparkles atlas changed')
        r = atlas['region']
        box = [int(r.x), int(r.y), int(r.w), int(r.h)]
        if box not in regions:
            regions.append(box)
        order.append(regions.index(box))
    require(sprite_root.props.get('playing') is True and sprite_root.props.get('animation') == 'Sparkle On', 'Sparkles playback changed')
    sparkles_node = next(n for n in instantiate(load, PRESENT) if n.path == 'Sparkles')
    sparkles = dict(texture='sparkles', fps=float(anims[0]['speed']), regions=regions, frames=order,
                    offset=vec(sparkles_node.props['position'], 'Sparkles'), initial_frame_range=[0, 47])
    textures = {}
    for kind, path in list(TEXTURES.items()) + [('sparkles', SPARKLE_TEXTURE)]:
        size = ex.png_size(path)
        ex.data(path + '.import')
        columns = 1 if kind == 'sparkles' else 5
        require(size[0] % columns == 0, 'Present texture frame grid changed')
        textures[kind] = dict(source=path, size=size, columns=columns, rows=1, output=OUTPUTS[kind])
    require(textures['sparkles']['size'] == [35, 7] and all(b[2] == 7 and b[3] == 7 for b in regions), 'Sparkles atlas geometry changed')
    ex.data(GIFT_SOUND); ex.data(GIFT_SOUND + '.import'); ex.data(RECEIVED_SOUND); ex.data(RECEIVED_SOUND + '.import')
    items_csv = table(ex, ITEM_TABLE)
    dialogue_csv = table(ex, DIALOGUE_TABLE)
    require(ex.yaml('Data/save_new_game.yaml')['party'] == ['ninten'], 'Present receiver mapping requires singleton Ninten party')
    room = read_json(Path(root) / 'content/native-opening.json')
    flags = {room['strings'][f['name_string']] for f in room['sections']['Flag']}
    bodies = {room['strings'][b['source_path_string']] for b in room['sections']['BodyRule']}
    check_doc = ex.yaml(DIALOGUE % 'ItemDialogue/presentcheck')
    empty_doc = ex.yaml(DIALOGUE % 'ItemDialogue/presentempty')
    require(check_doc == {'0': dict(text='DIALOGUE_ITEMDIALOGUE_PRESENTCHECK_0', goto='1'),
                          '1': dict(text='DIALOGUE_ITEMDIALOGUE_PRESENTCHECK_1', soundeffect='Item Received.mp3')},
            'Unreviewed presentcheck phrase graph')
    require(empty_doc == {'0': dict(text='DIALOGUE_ITEMDIALOGUE_PRESENTEMPTY_0')}, 'Unreviewed presentempty phrase graph')
    names = [n.path for n in nodes.values() if n.scene == PRESENT]
    require(names == ['Objects/Present1', 'Objects/Present2', 'Objects/Present4', 'Objects/Present3'], 'House present set changed')
    objects, templates, texts, programs = [], [], [], []
    next_text = FIRST_TEXT_ID
    for path in names:
        n = nodes[path]
        unknown = set(n.props) - {'script', 'dialog', 'dialog_full', 'button_prompt', 'type', 'position', 'flag', 'item'}
        require(not unknown, 'Unreviewed present properties at ' + path)
        kind = n.props.get('type', 'item')
        require(kind in TEXTURES, 'Unknown present type at ' + path)
        sprite = nodes[path + '/Sprite']
        override = sprite.overrides.get('texture')
        if override is not None:
            require(override.ext_path(sprite.props['texture'], 'Texture') == TEXTURES[kind], 'Present sprite override disagrees with type')
        interact = nodes[path + '/interact/CollisionShape2D']
        area = nodes[path + '/interact']
        shape = present_doc.sub_resource(interact.props['shape'], 'RectangleShape2D')
        scale = area.props.get('scale', Vec2(1, 1))
        require(abs(scale.x) == abs(scale.y) and 'position' not in area.props, 'Unreviewed present interact transform')
        offset = interact.props.get('position', Vec2(0, 0))
        position = vec(n.props['position'], path)
        center = [f32(position[0] + scale.x * offset.x), f32(position[1] + scale.y * offset.y)]
        extents = [f32(abs(scale.x) * shape['extents'].x), f32(abs(scale.y) * shape['extents'].y)]
        require(path + '/StaticBody2D' in bodies, 'Present collision body absent from Room')
        flag = n.props.get('flag', '')
        item = n.props.get('item', '')
        policy = POLICY_FLAG if flag else POLICY_OBJECT_FLAG
        if flag:
            require(flag in flags, 'Present flag unregistered: ' + flag)
        key = flag if flag else 'Nintens House/' + path.rsplit('/', 1)[1]
        item_doc = ex.yaml('Data/Items/%s.yaml' % item) if item else {}
        require(n.props.get('dialog') in (None, root_node.props['dialog']) or 'dialog' in n.overrides, 'Present dialog default changed')
        supported = (bool(flag) and item_doc.get('keyitem') is True and n.props.get('dialog') == root_node.props['dialog']
                     and 'dialog_full' not in n.overrides and 'dialog_empty' not in n.props)
        row = dict(source_path=path, type=kind, position=position, flag=key, policy=policy, item=item,
                   interact_center=center, interact_extents=extents, program=None, sound=GIFT_SOUND)
        if supported:
            require(set(item_doc) >= {'name', 'article', 'keyitem'} and 'doses' not in item_doc, 'Unreviewed key item document')
            template = len(templates)
            templates.append(dict(source_item=item, doses=1, key_item=True))
            check_ids = []
            for label in ('0', '1'):
                key_name = check_doc[label]['text']
                values = {}
                for native, column in LOCALES:
                    values[native] = bake(dialogue_csv[key_name][column], items_csv[item_doc['name']][column],
                                          items_csv[item_doc['article']][column])
                texts.append(dict(id=next_text, source_path=DIALOGUE % 'ItemDialogue/presentcheck', label=label,
                                  translation_key=key_name, item=item, raw=values['en'], localized=values))
                check_ids.append(next_text)
                next_text += 1
            row['program'] = len(programs)
            programs.append(dict(source_path=path, template=template, check=check_ids))
        objects.append(row)
    empty_key = empty_doc['0']['text']
    empty_id = next_text
    texts.append(dict(id=empty_id, source_path=DIALOGUE % 'ItemDialogue/presentempty', label='0', translation_key=empty_key,
                      item='', raw=dialogue_csv[empty_key]['en'],
                      localized={native: dialogue_csv[empty_key][column] for native, column in LOCALES}))
    for p in programs:
        p['commands'] = program_commands(p, objects, empty_id)
    return dict(schema=1, kind='encore.house-presents.source-ir', commit=PIN,
                scope=('All four original House presents drawn with flag state, Unwrapped clip and Sparkles; only fixed '
                       'key-item presents with default dialogues execute; other presents are explicit development boundaries'),
                sources=dict(sorted(ex.sources.items())), scene=SCENE, textures=textures, sparkles=sparkles, clips=clips,
                objects=objects, templates=templates, texts=texts, programs=programs)


def program_commands(program, objects, empty_id):
    obj = next(o for o in objects if o['source_path'] == program['source_path'])
    check0, check1 = program['check']
    return [
        dict(op='BranchFlag', a=obj['flag'], b=1, c=10),
        dict(op='PlayClip', a='Unwrapped'),
        dict(op='GrantItem', a=program['template']),
        dict(op='SetFlag', a=obj['flag'], b=1),
        dict(op='ShowText', a=check0),
        dict(op='AwaitText'),
        dict(op='ShowText', a=check1),
        dict(op='PlaySound', a=RECEIVED_SOUND),
        dict(op='AwaitText'),
        dict(op='End'),
        dict(op='ShowText', a=empty_id),
        dict(op='AwaitText'),
        dict(op='End'),
    ]


def recipe(ir):
    return dict(schema=1, kind='encore.house-presents.asset-recipe', commit=PIN, licence_review=LICENSE_REVIEW,
                sources={p: ir['sources'][p] for p in sorted({t['source'] for t in ir['textures'].values()})},
                resources={k: dict(source=t['source'], size=t['size'], grid=[t['columns'], t['rows']], output=t['output'],
                                   format='rgba8', compression='none') for k, t in sorted(ir['textures'].items())})


def load(root=ROOT):
    root = Path(root)
    ir = read_json(root / IR)
    require(canonical(ir) == canonical(build(root)), 'Stale/unreviewed House presents IR')
    review = read_json(root / REVIEW)
    require(set(review) == {'schema', 'commit', 'ir_sha256', 'sources', 'scope', 'semantics', 'unsupported', 'unverified'} and
            review['schema'] == 1 and review['commit'] == PIN and review['ir_sha256'] == digest(root / IR) and
            review['sources'] == ir['sources'] and review['scope'] == ir['scope'], 'House presents review mismatch')
    return ir


def verify_receipt(root=ROOT, source=None):
    root = Path(root)
    ir = load(root)
    r = read_json(root / RECEIPT)
    require(set(r) == {'schema', 'commit', 'recipe', 'ir_sha256', 'producer_sha256', 'tex3ds_sha256', 'outputs', 'limits'} and
            r['schema'] == 1 and r['commit'] == PIN and r['recipe'] == recipe(ir) and r['ir_sha256'] == digest(root / IR) and
            r['producer_sha256'] == digest(root / 'tools/house_presents.py'), 'House presents receipt mismatch')
    require(isinstance(r['tex3ds_sha256'], str) and re.fullmatch('[0-9a-f]{64}', r['tex3ds_sha256']) is not None,
            'House presents receipt lacks a real texture compiler')
    base = root / 'romfs' if source is None else Path(source)
    require(set(r['outputs']) == set(OUTPUTS.values()), 'House presents output set mismatch')
    for path, out in r['outputs'].items():
        target = base / path
        require(target.is_file() and target.stat().st_size == out['bytes'] and digest(target) == out['sha256'],
                'House presents texture differs from receipt: ' + path)
    return r


# ------------------------------------------------------------------ binary
def lower(ir, house):
    """Bind texts to House dialogue IDs and pack ENCPRS01 sections."""
    dialogue = {d['id']: d for d in house['dialogues']}
    for t in ir['texts']:
        require(t['id'] in dialogue and dialogue[t['id']]['source_path'] == t['source_path'], 'Present text absent from House pack')
    pool = bytearray()
    strings = []
    index = {}

    def s(value):
        require(isinstance(value, str) and '\0' not in value and len(value.encode()) <= 512, 'Invalid present string')
        if value not in index:
            data = value.encode()
            index[value] = len(strings)
            strings.append((len(pool), len(data)))
            pool.extend(data)
        return index[value]
    secs = {name: bytearray() for name in SECTIONS}
    kinds = list(OUTPUTS)
    for kind in kinds:
        t = ir['textures'][kind]
        secs['Textures'] += struct.pack('<I4H', s(t['output']), t['size'][0], t['size'][1], t['columns'], t['rows'])
    clip_index = {}
    for clip in ir['clips']:
        clip_index[clip['role']] = len(clip_index)
        first_frame, first_audio = len(secs['FrameKeys']) // 8, len(secs['AudioKeys']) // 8
        for time, frame in clip['frames']:
            secs['FrameKeys'] += struct.pack('<fI', time, frame)
        for time, playing in clip['audio']:
            secs['AudioKeys'] += struct.pack('<fI', time, playing)
        secs['Clips'] += struct.pack('<5If', CLIP_ROLES[clip['role']], first_frame, len(clip['frames']), first_audio,
                                     len(clip['audio']), clip['length'])
    sp = ir['sparkles']
    for box in sp['regions']:
        secs['SparkleFrames'] += struct.pack('<4H', *box)
    # SpriteFrames order as region indices (the source repeats the blank region).
    for region in sp['frames']:
        secs['SparkleOrder'] += struct.pack('<I', region)
    secs['Sparkles'] += struct.pack('<3I4f', kinds.index('sparkles'), 0, len(sp['frames']), sp['fps'], sp['offset'][0],
                                    sp['offset'][1], float(sp['initial_frame_range'][1]))
    for t in ir['templates']:
        secs['Templates'] += struct.pack('<3I', s(t['source_item']), t['doses'], int(t['key_item']))
    text_index = {t['id']: i for i, t in enumerate(ir['texts'])}
    for t in ir['texts']:
        secs['Texts'] += struct.pack('<2I', t['id'], s(t['source_path']))
    for p in ir['programs']:
        first = len(secs['Commands']) // 20
        for c in p['commands']:
            op = OPCODES[c['op']]
            a, b, cc = 0, 0, 0
            if c['op'] in ('BranchFlag', 'SetFlag', 'PlaySound'):
                a = s(c['a'])
            elif c['op'] == 'ShowText':
                a = text_index[c['a']]
            elif c['op'] == 'PlayClip':
                a = clip_index[c['a']]
            elif c['op'] in ('GrantItem', 'Jump'):
                a = c['a']
            b = c.get('b', 0)
            cc = c.get('c', 0)
            secs['Commands'] += struct.pack('<5I', op, a, b, cc, 0)
        secs['Programs'] += struct.pack('<2I', first, len(p['commands']))
    for o in ir['objects']:
        secs['Objects'] += struct.pack('<5I6f2I', s(o['source_path']), kinds.index(o['type']), o['policy'], s(o['flag']),
                                       s(o['item']) if o['item'] else NONE, o['position'][0], o['position'][1],
                                       o['interact_center'][0], o['interact_center'][1], o['interact_extents'][0],
                                       o['interact_extents'][1], NONE if o['program'] is None else o['program'], s(o['sound']))
    for off, length in strings:
        secs['Strings'] += struct.pack('<2I', off, length)
    secs['Bytes'] = pool
    return secs


def encode(ir, house):
    from tools.podunk_field import pack_container
    secs = lower(ir, house)
    blob = pack_container(MAGIC, 1, [secs[n] for n in SECTIONS], STRIDES, LIMIT)
    parse(blob)
    return blob


def parse(blob):
    """Independent structural check mirrored by runtime/present_data.cpp."""
    from tools.podunk_field import unpack_container
    scene, secs = unpack_container(blob, MAGIC, STRIDES, LIMIT)
    require(scene == 1, 'Present pack scene identity')
    rows = {n: secs[i] for i, n in enumerate(SECTIONS)}
    count = {n: len(rows[n]) // STRIDES[i] for i, n in enumerate(SECTIONS)}
    require(count['Textures'] == 4 and count['Clips'] == 2 and count['Sparkles'] == 1 and 0 < count['Objects'] <= 32 and
            count['Programs'] <= count['Objects'] and count['Commands'] <= 256 and 0 < count['SparkleOrder'] <= 256,
            'Present pack table counts')
    return rows


def stage_files(source):
    root = ROOT
    ir = load(root)
    verify_receipt(root, source)
    blob = (Path(source) / PACK).read_bytes()
    require(blob == encode(ir, read_json(root / 'content/native-house.json')), 'Staged present pack differs from reviewed IR')
    files = {Path(PACK): blob}
    for path in OUTPUTS.values():
        files[Path(path)] = (Path(source) / path).read_bytes()
    return files


def review(ir):
    return dict(schema=1, commit=PIN, ir_sha256=digest(ROOT / IR), sources=ir['sources'], scope=ir['scope'],
                semantics={
                    'ready': 'Opened presents (flag true) show frame 4 and hide Sparkles; each Sparkles _ready draws rand_range(0,47) in tree order',
                    'interaction': 'Only the interact Area2D participates in the nearest ray hit; player turns on x and y',
                    'grant': 'Unwrapped clip starts, key item granted to key_items (never full), flag set, presentcheck opens',
                    'text': 'Fixed-item lowering of [ItemName]/[ItemArt3]; [ItemReceiver] is the singleton leader for key items',
                    'empty': 'Opened present shows ItemDialogue/presentempty'},
                unsupported=['Presents with object flags or non-key items (Present1 diary cutscene, Present2 bat, Present3 map)',
                             'ButtonPrompt over presents', 'Reusable/noproblem when the ray hits the StaticBody2D'],
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
            print('House presents IR:', len(ir['objects']), 'objects,', len(ir['programs']), 'programmes')
        elif args.action == 'compile':
            ir = load()
            house = read_json(ROOT / 'content/native-house.json')
            outputs = {}
            if args.tex3ds is not None:
                import shlex
                command = shlex.split(args.tex3ds_command) if args.tex3ds_command else None
                for kind, path in OUTPUTS.items():
                    target = ROOT / 'romfs' / path
                    target.parent.mkdir(parents=True, exist_ok=True)
                    src = ROOT / 'upstream/MOTHER-Encore' / ir['textures'][kind]['source']
                    if command:
                        rel = lambda q: Path(q).resolve().relative_to(ROOT).as_posix()
                        subprocess.run(command + ['-f', 'rgba8', '-z', 'none', '-o', rel(target), rel(src)], check=True, cwd=ROOT)
                    else:
                        subprocess.run([str(args.tex3ds), '-f', 'rgba8', '-z', 'none', '-o', str(target), str(src)], check=True)
                    outputs[path] = dict(bytes=target.stat().st_size, sha256=digest(target))
                write_json(ROOT / RECEIPT, dict(schema=1, commit=PIN, recipe=recipe(ir), ir_sha256=digest(ROOT / IR),
                                                producer_sha256=digest(ROOT / 'tools/house_presents.py'),
                                                tex3ds_sha256=digest(args.tex3ds), outputs=outputs,
                                                limits='Pinned original lossless sprites converted by real tex3ds; no hardware claim'))
            blob = encode(ir, house)
            (ROOT / 'romfs' / PACK).write_bytes(blob)
            if (ROOT / RECEIPT).is_file():
                verify_receipt()
            elif args.tex3ds is None:
                print('House presents pack written without textures; console tex3ds compile is required before staging')
            print('House presents pack:', len(blob), 'bytes; checked ENCPRS01')
        else:
            ir = load()
            verify_receipt()
            require((ROOT / 'romfs' / PACK).read_bytes() == encode(ir, read_json(ROOT / 'content/native-house.json')), 'Stale present pack')
            print('House presents verified')
    except (ValueError, OSError, KeyError, TypeError, struct.error, subprocess.SubprocessError) as error:
        print('HOUSE PRESENTS ERROR:', error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
