"""Checked source bindings for the seven original catacombs dropped items.

This is an offline source adapter. The original inventory/Item constructors and
DroppedItem lifecycle are observed separately; this model admits no new scene.
"""
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools import catacombs_breakables as b, podunk_field as f
from tools.extract_battle_entry import PIN, require
from tools.godot_text import instantiate, owner_of, Vec2, NodePathValue

PATHS = dict(script='Scripts/Main/DroppedItem.gd',
             holder='Scripts/Main/ItemHolder.gd',
             flag='Scripts/Main/FlaggableObject.gd',
             inventory='Scripts/global/Inventory.gd', item='Scripts/global/Item.gd',
             character='Scripts/global/Character.gd',
             prefab='Nodes/Overworld/Objects/DroppedItem.tscn',
             prompt='Scripts/UI/Button Prompt.gd',
             sparkle='Scripts/misc/sparkles.gd')


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
                 breakables=break_proof,
                 producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                 scope='Original seven item bindings, item YAML and inventory methods; '
                       'no release catalog, save-domain or whole-room admission')
    return model, proof


if __name__ == '__main__':
    model, proof = derive()
    print(json.dumps(dict(bindings=model['bindings'], templates=[
        {k: v[k] for k in ['id', 'source', 'doses', 'size']} for v in model['templates']],
        geometry=model['geometry'], source_count=len(proof['sources'])), indent=2))
