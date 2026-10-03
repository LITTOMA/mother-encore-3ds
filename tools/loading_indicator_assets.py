#!/usr/bin/env python3
"""Checked, source-backed Ninten walk reused for the platform loading indicator.

The production loading paths inspected do not supply a dedicated loading sprite.
This is an explicit platform adaptation, not an original loading-screen claim.
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
import math
import re
import struct
import subprocess
import sys
import zlib
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.extract_battle_entry import Extractor, animation, node, one, require
from tools.upstream import read_json, write_json

RECIPE = ROOT / 'content/native-loading-indicator.json'
PACK = ROOT / 'romfs/loading-preview/indicator.encload'
REVIEW = ROOT / 'compatibility/reviews/ninten-animation-v0410.json'
REFERENCE = ROOT / 'reports/m3-actor-reference-reviewed/animation.json'
SCENE = 'Nodes/Reusables/Player.tscn'
TEXTURE = 'Graphics/Character Sprites/Ninten/main.png'
TEXTURE_PATH = 'loading-preview/ninten-walk-right.t3x'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def f32(number):
    return struct.unpack('<f', struct.pack('<f', number))[0]


def extract():
    ex = Extractor(ROOT)
    review = read_json(REVIEW)
    require(review['scene'] == SCENE and review['texture'] == TEXTURE and
            review['commit'] == ex.lock['commit'] and review['licence_review'], 'Unreviewed character source')
    scene = ex.text(SCENE)
    image = Image.open(io.BytesIO(ex.data(TEXTURE))).convert('RGBA')
    require(ex.sources[SCENE] == review['scene_sha256'] and ex.sources[TEXTURE] == review['texture_sha256'],
            'Character differs from reviewed animation')
    imported = ex.text(TEXTURE + '.import')
    require('flags/filter=false' in imported and 'flags/mipmaps=false' in imported, 'Unreviewed character sampler')
    ex.data('LICENSE')
    inspected = ['Scripts/global/SceneTransition.gd', 'Scripts/global/cacheLoader.gd',
                 'Maps/Testing/loader.gd', 'addons/ZoneLoadingSystem/scripts/world.gd',
                 'addons/ZoneLoadingSystem/templates/2d/template_2d.tscn']
    for path in inspected:
        ex.data(path)
    project = ex.text('project.godot')
    channels = [float(v) for v in one(r'^environment/default_clear_color=Color\(\s*([^)]*)\)', project,
                                     'source clear color')[1].split(',')]
    require(len(channels) == 4 and all(math.isfinite(v) and 0 <= v <= 1 for v in channels), 'Invalid clear color')
    clear = [round(v * 255) for v in channels]
    sprite = node(scene, 'Position/main')
    grid = [sprite['hframes'], sprite['vframes']]
    require(grid == review['grid'] and list(image.size) == review['texture_size'] and sprite.get('centered', True),
            'Unreviewed character grid')
    size = [image.width // grid[0], image.height // grid[1]]
    require(image.width % grid[0] == 0 and image.height % grid[1] == 0, 'Character grid has partial cells')
    identifier = int(one(r'^"anims/Walk Right" = SubResource\( (\d+) \)', scene, 'Walk Right binding')[1])
    clip = animation(scene, identifier, SCENE, 'Walk Right')
    require(clip['loop'] is True and len(clip['tracks']) == 3, 'Unreviewed walking animation')
    track = clip['tracks'][0]
    require(track['path'] == 'Position/main:frame' and track['type'] == 'value' and track['interp'] == 1 and
            track['loop_wrap'] and track['keys']['update'] == 1 and all(v == 1 for v in track['keys']['transitions']),
            'Unsupported walking frame track')
    for item, path, value in zip(clip['tracks'][1:], ['Position/main:visible', 'SpecialAnimations:visible'], [True, False]):
        require(item['path'] == path and item['keys']['times'] == [0] and item['keys']['values'] == [value] and
                item['keys']['update'] == 1, 'Unsupported walking visibility')
    reference = read_json(REFERENCE)
    native = [row for row in reference['timelines'] if row['name'] == clip['name']]
    require(len(native) == 1, 'Native walking reference missing')
    native = native[0]
    times = [f32(float(t)) for t in track['keys']['times']]
    frames = track['keys']['values']
    require(native['loop'] is True and f32(float(native['length'])) == f32(clip['length']) and
            [[f32(float(t)), frame] for t, frame in native['keys']] == list(map(list, zip(times, frames))),
            'Walking native reference differs from source')
    unique = list(dict.fromkeys(frames))
    require(all(type(frame) is int and 0 <= frame < grid[0] * grid[1] for frame in unique), 'Source frame outside grid')
    result = dict(schema=1, kind='encore.loading-indicator.source-ir', commit=ex.lock['commit'], sources=ex.sources,
                  scope='Platform loading indicator reuses original Ninten Walk Right. No dedicated production loading sprite was found in the inspected loading paths; original scene transitions use fades.',
                  inspected_loading_paths=inspected,
                  dependencies={str(REVIEW.relative_to(ROOT)): sha(REVIEW), str(REFERENCE.relative_to(ROOT)): sha(REFERENCE)},
                  animation=dict(name=clip['name'], source_resource=f'{SCENE}::{identifier}', length=f32(clip['length']),
                                 loop=True, keys=[dict(time=t, frame=unique.index(frame), source_frame=frame) for t, frame in zip(times, frames)]),
                  texture=dict(path=TEXTURE_PATH, source=TEXTURE, source_grid=grid, source_frames=unique,
                               width=size[0] * len(unique), height=size[1], frame_width=size[0], frame_height=size[1]),
                  background_color=sum(value << (8 * index) for index, value in enumerate(clear)),
                  background_source='project.godot:environment/default_clear_color',
                  layout=dict(anchor='bottom-right', right_margin=8, bottom_margin=8, scale=1,
                              viewports=[[320, 180], [400, 240]],
                              platform_adaptation='Eight-pixel inset from the active game viewport; source pixels stay 1:1. Loading is presentation-only and uses elapsed wall time.'))
    validate_ir(result)
    return ex, result, image


def validate_ir(r):
    require(r['schema'] == 1 and r['kind'] == 'encore.loading-indicator.source-ir', 'Unsupported loading recipe')
    t, a, l = r['texture'], r['animation'], r['layout']
    require(t['path'] == TEXTURE_PATH and all(type(t[k]) is int and 0 < t[k] <= 1024 for k in ['width', 'height', 'frame_width', 'frame_height']), 'Invalid loading texture')
    require(0 < len(t['source_frames']) <= 16 and t['width'] == t['frame_width'] * len(t['source_frames']) and t['height'] == t['frame_height'], 'Invalid loading strip')
    require(a['loop'] is True and math.isfinite(a['length']) and 0 < a['length'] <= 120 and 0 < len(a['keys']) <= 16, 'Invalid loading timeline')
    previous = -1
    for k in a['keys']:
        require(set(k) == {'time', 'frame', 'source_frame'} and math.isfinite(k['time']) and previous < k['time'] < a['length'] and
                type(k['frame']) is int and 0 <= k['frame'] < len(t['source_frames']) and t['source_frames'][k['frame']] == k['source_frame'], 'Invalid loading key')
        previous = k['time']
    require(a['keys'][0]['time'] == 0, 'Loading timeline must begin at zero')
    require(l['anchor'] == 'bottom-right' and l['scale'] == 1 and all(type(l[k]) is int and 0 <= l[k] <= 1024 for k in ['right_margin', 'bottom_margin']), 'Invalid loading layout')
    require(type(r['background_color']) is int and 0 <= r['background_color'] <= 0xffffffff and r['background_color'] >> 24 == 255, 'Invalid loading backdrop')
    require(0 < len(l['viewports']) <= 8 and len({tuple(v) for v in l['viewports']}) == len(l['viewports']), 'Invalid loading viewport list')
    for v in l['viewports']:
        require(len(v) == 2 and all(type(x) is int and 0 < x <= 4096 for x in v) and
                v[0] >= t['frame_width'] + l['right_margin'] and v[1] >= t['frame_height'] + l['bottom_margin'], 'Loading frame does not fit viewport')


def encode(r):
    validate_ir(r)
    t, a, l = r['texture'], r['animation'], r['layout']
    payload = bytearray(struct.pack('<8IfI', r['background_color'], t['width'], t['height'], t['frame_width'], t['frame_height'],
                                   len(t['source_frames']), l['right_margin'], l['bottom_margin'], a['length'], len(a['keys'])))
    for k in a['keys']:
        payload.extend(struct.pack('<fI', k['time'], k['frame']))
    for frame in t['source_frames']:
        payload.extend(struct.pack('<I', frame))
    path = t['path'].encode('ascii')
    payload.extend(struct.pack('<I', len(path)) + path)
    payload.extend(struct.pack('<I', len(l['viewports'])))
    for v in l['viewports']:
        payload.extend(struct.pack('<2I', *v))
    return struct.pack('<8s4I', b'ENCLD001', 1, 24 + len(payload), zlib.crc32(payload), 1) + payload


def verify_recipe(r):
    _, source, _ = extract()
    require(set(r) == set(source) | {'outputs'}, 'Unknown/missing loading recipe fields')
    require({k: v for k, v in r.items() if k != 'outputs'} == source, 'Loading recipe differs from reviewed source/adaptation')
    require(set(r['outputs']) == {TEXTURE_PATH}, 'Unexpected loading texture outputs')
    for path, info in r['outputs'].items():
        output = ROOT / 'romfs' / path
        require(info == dict(sha256=sha(output), bytes=output.stat().st_size), 'Loading texture hash/size mismatch')


def stage_files(root):
    r = read_json(RECIPE)
    verify_recipe(r)
    relative_pack = PACK.relative_to(ROOT / 'romfs')
    binary = (Path(root) / relative_pack).read_bytes()
    require(binary == encode(r), 'Loading pack is stale')
    texture = (Path(root) / TEXTURE_PATH).read_bytes()
    require(hashlib.sha256(texture).hexdigest() == r['outputs'][TEXTURE_PATH]['sha256'], 'Staged loading texture differs')
    return {relative_pack: binary, Path(TEXTURE_PATH): texture}


staged_files = stage_files


def assets(tex3ds):
    _, r, source = extract()
    t = r['texture']
    strip = Image.new('RGBA', (t['width'], t['height']))
    for index, frame in enumerate(t['source_frames']):
        x = frame % t['source_grid'][0] * t['frame_width']
        y = frame // t['source_grid'][0] * t['frame_height']
        strip.paste(source.crop((x, y, x + t['frame_width'], y + t['frame_height'])), (index * t['frame_width'], 0))
    build = ROOT / 'build/loading-indicator-assets'
    build.mkdir(parents=True, exist_ok=True)
    png = build / 'ninten-walk-right.png'
    strip.save(png)
    target = ROOT / 'romfs' / TEXTURE_PATH
    target.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(tex3ds), '-f', 'rgba8', '-z', 'none', '-o', str(target), str(png)], check=True)
    r['outputs'] = {TEXTURE_PATH: dict(sha256=sha(target), bytes=target.stat().st_size)}
    write_json(RECIPE, r)
    PACK.write_bytes(encode(r))
    print(f'Loading indicator: {t["width"]}x{t["height"]} strip, {target.stat().st_size} texture bytes, {PACK.stat().st_size} checked data bytes')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['assets', 'compile', 'verify'])
    parser.add_argument('--tex3ds', type=Path)
    args = parser.parse_args()
    try:
        if args.action == 'assets':
            require(args.tex3ds is not None, '--tex3ds is required for assets')
            assets(args.tex3ds)
        else:
            r = read_json(RECIPE)
            verify_recipe(r)
            binary = encode(r)
            if args.action == 'compile':
                PACK.parent.mkdir(parents=True, exist_ok=True)
                PACK.write_bytes(binary)
            else:
                require(PACK.read_bytes() == binary, 'Loading pack is stale')
            print('Verified loading indicator source, texture and checked data')
        return 0
    except (OSError, ValueError, KeyError, TypeError, subprocess.CalledProcessError) as error:
        print('LOADING INDICATOR ERROR: ' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
