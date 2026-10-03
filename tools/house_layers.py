#!/usr/bin/env python3
"""Compile the reviewed house Objects/Y-sort and Above/foreground TileMaps.

The native export supplies resolved cells, offsets, atlas coordinates and scene
order. Above is a later root sibling of Objects at the same Z, so it is emitted
as the fixed foreground pass. The original texture pixels remain unmodified.
"""
import argparse
import hashlib
import json
import math
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.upstream import read_json, write_json, safe_path, git
from tools.scene_data import validate as validate_scene

SOURCE = 'reports/m2-scene-reference-reviewed/house-data.json'
REFERENCE = 'reports/m2-scene-reference-reviewed/receipt.json'
SOURCE_RECEIPT = 'reports/m2-scene-reference-reviewed/house-source.json'
SCENE = 'Maps/podunk/Nintens House.tscn'
TILESET = 'Tilesets/Interior.tres'
TEXTURE = 'Graphics/Tilesets/TilNintensHouse.png'
ABOVE_TEXTURE = 'Graphics/Tilesets/TilPodunkInteriors.png'
TEXTURES = {'objects': (TEXTURE, 'objects.t3x'), 'above': (ABOVE_TEXTURE, 'above.t3x')}
OUT = ROOT / 'romfs/house-layers'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def layout_sha(rows):
    return hashlib.sha256(json.dumps(rows, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value):
    if isinstance(value, dict):
        require(value.get('type') == 'int64', 'Invalid overlay integer')
        value = int(value['value'])
    require(type(value) in (int, float) and math.isfinite(value) and value == int(value),
            'Non-integral overlay coordinate')
    return int(value)


def vector(value):
    require(value.get('type') == 'Vector2', 'Invalid overlay vector')
    return integer(value['x']), integer(value['y'])


def transform(value):
    require(value.get('type') == 'Transform2D', 'Invalid overlay transform')
    return tuple(vector(value[key]) for key in ('x', 'y', 'origin'))


def white(value):
    return value == {'type': 'Color', 'r': 1, 'g': 1, 'b': 1, 'a': 1}


def image_size(path):
    raw = path.read_bytes()
    require(raw[:8] == b'\x89PNG\r\n\x1a\n' and raw[12:16] == b'IHDR', 'Expected overlay source PNG')
    return list(struct.unpack('>II', raw[16:24]))


def validate_canvas(node):
    p = node['properties']
    require(vector(p['position']) == (0, 0) and p['rotation'] == 0 and vector(p['scale']) == (1, 1)
            and transform(node['world_transform']) == ((1, 0), (0, 1), (0, 0)),
            'Unsupported overlay layer transform')
    require(integer(p['z_index']) == 0 and p['z_as_relative'] is True and p['show_behind_parent'] is False,
            'Unreviewed overlay layer depth')
    require(p['visible'] is True and white(p['modulate']) and white(p['self_modulate'])
            and p['material'] is None and p['use_parent_material'] is False and p['script'] is None,
            'Unsupported overlay layer appearance')


def records(data):
    validate_scene(data)
    require(data['source'] == 'res://' + SCENE, 'Unexpected overlay source scene')
    nodes = {n['path']: n for n in data['nodes']}
    paths = list(nodes)
    require(paths.index('Objects') < paths.index('Above'), 'Unreviewed foreground sibling order')
    validate_canvas(nodes['.'])
    resources = {r['id']: r for r in data['resources']}
    result = []
    for name, role, flags, count, mode in [('Objects', 'objects', 0, 14, 0), ('Above', 'above', 1, 276, 2)]:
        node = nodes[name]
        p = node['properties']
        validate_canvas(node)
        require(node['class'] == 'TileMap' and p['cell_y_sort'] is (flags == 0)
                and integer(p['mode']) == 0 and integer(p['cell_tile_origin']) == 0
                and integer(p['cell_half_offset']) == 2 and integer(p['format']) == 1
                and p['centered_textures'] is False and p['compatibility_mode'] is False
                and p['cell_clip_uv'] is False and vector(p['cell_size']) == (16, 16),
                'Unreviewed ' + name + ' TileMap layout')
        custom_size = 64 if name == 'Objects' else 16
        require(transform(p['cell_custom_transform']) == ((custom_size, 0), (0, custom_size), (0, 0)),
                'Unreviewed inactive TileMap custom transform')
        tileset = resources[p['tile_set']['id']]
        require(tileset['class'] == 'TileSet' and tileset['path'] ==
                ('res://' + SCENE + '::101' if name == 'Objects' else 'res://' + TILESET),
                'Unexpected overlay tileset')
        ts = tileset['properties']
        require(len(node['cells']) == count, 'Unreviewed ' + name + ' overlay count')
        require({c['tile'] for c in node['cells']} == (set(range(14)) if name == 'Objects' else {14}),
                'Unreviewed overlay tile identity')
        occupied = set()
        for cell in node['cells']:
            require(not any(cell[k] for k in ('flip_x', 'flip_y', 'transpose')), 'Unreviewed overlay flip/transpose')
            position = vector(cell['position'])
            require(position not in occupied, 'Duplicate overlay cell')
            occupied.add(position)
            origin = vector(cell['local_origin'])
            require(origin == tuple(v * 16 for v in position), 'Inconsistent overlay cell origin')
            key = str(cell['tile']) + '/'
            require(integer(ts[key + 'tile_mode']) == mode and integer(ts[key + 'z_index']) == 0,
                    'Unreviewed overlay mode/depth')
            texture = resources[ts[key + 'texture']['id']]
            require(texture['class'] == 'StreamTexture' and texture['path'] == 'res://' + TEXTURES[role][0],
                    'Unexpected overlay texture')
            texture_size = vector(texture['size'])
            require(texture_size == ((208, 192) if role == 'objects' else (896, 400)), 'Unreviewed overlay texture size')
            require(ts[key + 'material'] is None and ts[key + 'normal_map'] is None and white(ts[key + 'modulate']),
                    'Unsupported overlay tile appearance')
            region = ts[key + 'region']
            u, v = vector(region['position'])
            width, height = vector(region['size'])
            offset = vector(ts[key + 'tex_offset'])
            atlas = vector(cell['autotile'])
            if mode == 2:
                tile_size = vector(ts[key + 'autotile/tile_size'])
                require(tile_size == (16, 16) and integer(ts[key + 'autotile/spacing']) == 0
                        and offset == (0, 0) and ts[key + 'autotile/z_index_map'] == {'type': 'Array', 'value': []},
                        'Unreviewed foreground atlas geometry/depth')
                require(min(atlas) >= 0 and (atlas[0] + 1) * tile_size[0] <= width
                        and (atlas[1] + 1) * tile_size[1] <= height, 'Overlay atlas coordinate outside region')
                u += atlas[0] * tile_size[0]
                v += atlas[1] * tile_size[1]
                width, height = tile_size
            else:
                require(atlas == (0, 0), 'Unreviewed single-tile atlas coordinate')
            require(width > 0 and height > 0 and u >= 0 and v >= 0
                    and u + width <= texture_size[0] and v + height <= texture_size[1], 'Overlay region outside texture')
            result.append(dict(name=ts[key + 'name'], tile=cell['tile'], layer=name, resource=role, flags=flags,
                               x=origin[0] + offset[0], y=origin[1] + offset[1], sort_y=origin[1],
                               u=u, v=v, width=width, height=height))
    return sorted(result, key=lambda row: (row['flags'], row['sort_y'], row['x']))


def validate(root):
    lock = read_json(ROOT / 'upstream.lock')
    inventory = read_json(ROOT / 'compatibility/upstream-inventory.json')
    reference = read_json(ROOT / REFERENCE)
    source = read_json(ROOT / SOURCE_RECEIPT)
    require(lock['commit'] == inventory['commit'] == source['commit'] == reference['house']['commit']
            == git(root, 'rev-parse', 'HEAD'), 'Overlay source pin mismatch')
    require(source['scene'] == SCENE and reference['house']['data_sha256'] == sha(ROOT / SOURCE)
            and reference['house']['source_receipt_sha256'] == sha(ROOT / SOURCE_RECEIPT),
            'Changed overlay native export requires review')
    for path in (SCENE, TILESET, TEXTURE, ABOVE_TEXTURE):
        require(sha(safe_path(root, path)) == inventory['files'][path]['sha256'] == source['files'][path]['sha256'],
                'Changed overlay source requires review')
    return records(read_json(ROOT / SOURCE))


def receipt(root, rows, output):
    resources = {}
    for role, (source, name) in TEXTURES.items():
        width, height = image_size(safe_path(root, source))
        resources[role] = dict(source=source, source_sha256=sha(safe_path(root, source)), output=name,
                               width=width, height=height, sha256=sha(output / name))
    return dict(schema=3, commit=read_json(ROOT / 'upstream.lock')['commit'],
                sources={path: sha(safe_path(root, path)) for path in (SCENE, TILESET, TEXTURE, ABOVE_TEXTURE)},
                scene_export_sha256=sha(ROOT / SOURCE), reference_sha256=sha(ROOT / REFERENCE),
                source_receipt_sha256=sha(ROOT / SOURCE_RECEIPT), resources=resources,
                tiles=rows, layout_sha256=layout_sha(rows))


def compile(root, tex3ds, output=OUT):
    rows = validate(root)
    output.mkdir(parents=True, exist_ok=True)
    for source, name in TEXTURES.values():
        subprocess.run([str(tex3ds), '-f', 'rgba8', '-z', 'none', '-o', str(output / name), str(safe_path(root, source))], check=True)
    write_json(output / 'source.json', receipt(root, rows, output))


def verify(root, output=OUT):
    rows = validate(root)
    require(read_json(output / 'source.json') == receipt(root, rows, output), 'Stale or tampered overlay bundle')
    print('Verified 14 original house Y-sorted overlays and 276 original foreground atlas cells')


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('action', choices=['compile', 'verify'])
    ap.add_argument('--root', type=Path, default=ROOT / 'upstream/MOTHER-Encore')
    ap.add_argument('--tex3ds', type=Path, default=Path('/opt/devkitpro/tools/bin/tex3ds'))
    args = ap.parse_args()
    try:
        if args.action == 'compile':
            compile(args.root, args.tex3ds)
        else:
            verify(args.root)
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        sys.exit('HOUSE LAYERS ERROR: ' + str(error))
