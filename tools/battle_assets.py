#!/usr/bin/env python3
"""Compile the reviewed battle-entry art; never execute an upstream game script.

BPX1 is a lossless indexed image stream. The palette retains RGB even at alpha
zero: those channels are observable under Transition.shader's blend_disabled.
The original importer's fix_alpha_edges step is delegated to pinned Godot's
native Image method, in an isolated project containing our short helper only.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.upstream import git, read_json, safe_path, write_json
from tools.asset_receipts import receipt_path, receipt_entries

RECIPE = ROOT / 'content/battle-assets.json'
OUT = ROOT / 'romfs/graphics/battle/lamp'
MAGIC = b'ENCBPIX\0'
HEADER = struct.Struct('<8s7I')


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_source(root: Path, recipe: dict, lock: dict) -> None:
    if recipe.get('schema') != 1 or recipe.get('game_version') != lock.get('game_version'):
        raise ValueError('Unreviewed battle asset schema/version')
    if recipe.get('commit') != lock.get('commit') or not recipe.get('licence_review'):
        raise ValueError('Unreviewed battle source/asset permission')
    if not recipe.get('sources') or not recipe.get('resources'):
        raise ValueError('Empty battle asset review')
    for path, digest in recipe['sources'].items():
        if sha(safe_path(root, path)) != digest:
            raise ValueError('Changed battle source requires review: ' + path)
    seen = set()
    for index, resource in enumerate(recipe['resources']):
        required = {'id', 'name', 'source', 'kind', 'size', 'grid', 'output', 'fix_alpha_edges'}
        if set(resource) not in (required, required | {'crop'}):
            raise ValueError('Unknown battle asset fields')
        if resource['id'] != index or resource['kind'] not in ('texture', 'indexed'):
            raise ValueError('Unsupported battle asset kind/order')
        if resource['source'] not in recipe['sources'] or resource['output'] in seen:
            raise ValueError('Unreviewed/duplicate asset binding')
        safe_path(ROOT / 'romfs', resource['output'])
        seen.add(resource['output'])
        if (len(resource['size']) != 2 or len(resource['grid']) != 2 or
            any(type(x) is not int or x <= 0 for x in resource['size'] + resource['grid']) or
            any(s % g for s, g in zip(resource['size'], resource['grid']))):
            raise ValueError('Invalid battle image geometry')
        if 'crop' in resource:
            crop = resource['crop']
            if (len(crop) != 4 or any(type(c) is not int for c in crop) or
                crop[0] < 0 or crop[1] < 0 or crop[2] <= 0 or crop[3] <= 0 or
                crop[0] + crop[2] > resource['size'][0] or crop[1] + crop[3] > resource['size'][1]):
                raise ValueError('Invalid reviewed crop')
    font = recipe['font']
    if (font['source'] not in recipe['sources'] or font['id'] != len(recipe['resources']) or
        font['size'] <= 0 or not 0 <= font['first'] <= font['last'] < 128):
        raise ValueError('Unsupported font domain')


def encode_indexed(image, grid: tuple[int, int]) -> bytes:
    """Reorder atlas tiles as contiguous frames without losing transparent RGB."""
    image = image.convert('RGBA')
    columns, rows = grid
    if columns <= 0 or rows <= 0 or image.width % columns or image.height % rows:
        raise ValueError('Invalid indexed image grid')
    width, height = image.width // columns, image.height // rows
    palette = sorted(set(image.getdata()))
    if not 1 <= len(palette) <= 256:
        raise ValueError('Indexed asset exceeds lossless 8-bit palette')
    ids = {rgba: i for i, rgba in enumerate(palette)}
    pixels = bytearray()
    for row in range(rows):
        for col in range(columns):
            frame = image.crop((col * width, row * height, (col + 1) * width, (row + 1) * height))
            pixels.extend(ids[p] for p in frame.getdata())
    payload = bytes(c for p in palette for c in p) + pixels
    return HEADER.pack(MAGIC, 1, width, height, columns * rows, len(palette), len(payload), zlib.crc32(payload)) + payload


def decode_indexed(raw: bytes) -> dict:
    if len(raw) < HEADER.size:
        raise ValueError('Truncated indexed header')
    magic, version, width, height, frames, colors, length, crc = HEADER.unpack_from(raw)
    if magic != MAGIC or version != 1:
        raise ValueError('Unknown indexed image format/version')
    if not (0 < width <= 1024 and 0 < height <= 1024 and 0 < frames <= 256 and 0 < colors <= 256):
        raise ValueError('Invalid indexed image dimensions/palette')
    if width * height * frames > 16 * 1024 * 1024:
        raise ValueError('Indexed image exceeds resource limit')
    if length != colors * 4 + width * height * frames or len(raw) != HEADER.size + length:
        raise ValueError('Indexed image length mismatch')
    payload = raw[HEADER.size:]
    if zlib.crc32(payload) != crc:
        raise ValueError('Indexed image CRC mismatch')
    palette = [tuple(payload[i:i + 4]) for i in range(0, colors * 4, 4)]
    pixels = payload[colors * 4:]
    if max(pixels) >= colors:
        raise ValueError('Indexed image palette reference out of range')
    return dict(width=width, height=height, frames=frames, palette=palette, pixels=pixels)


def shader_transition(pixel: tuple[int, int, int, int], screen: tuple[int, int, int, int],
                      old: tuple[int, int, int, int], new: tuple[int, int, int, int]):
    if pixel == old:
        return new
    return screen if pixel[3] == 255 else pixel


NATIVE_HELPER = '''extends SceneTree
func _init():
\tvar version = Engine.get_version_info()
\tif version.major != 3 or version.minor != 6 or version.patch != 2 or version.hash != "3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8":
\t\tquit(7)
\t\treturn
\tvar img = Image.new()
\tif img.load(OS.get_environment("BATTLE_INPUT")) != OK:
\t\tquit(2)
\t\treturn
\timg.fix_alpha_edges()
\tif img.save_png(OS.get_environment("BATTLE_OUTPUT")) != OK:
\t\tquit(3)
\t\treturn
\tvar data = DynamicFontData.new()
\tdata.font_path = OS.get_environment("BATTLE_FONT")
\tvar font = DynamicFont.new()
\tfont.font_data = data
\tfont.size = int(OS.get_environment("BATTLE_FONT_SIZE"))
\tfont.extra_spacing_top = int(OS.get_environment("BATTLE_FONT_TOP"))
\tfont.extra_spacing_bottom = int(OS.get_environment("BATTLE_FONT_BOTTOM"))
\tvar advances = []
\tfor i in range(int(OS.get_environment("BATTLE_FIRST")), int(OS.get_environment("BATTLE_LAST")) + 1):
\t\tadvances.append(font.get_char_size(i).x)
\tvar file = File.new()
\tif file.open(OS.get_environment("BATTLE_METRICS"), File.WRITE) != OK:
\t\tquit(4)
\t\treturn
\tfile.store_string(JSON.print({"version":version,"advances":advances,"ascent":font.get_ascent(),"descent":font.get_descent(),"height":font.get_height()}))
\tfile.close()
\tquit(0)
'''


def native_import(root: Path, recipe: dict, godot: Path, build: Path) -> tuple[Path, dict]:
    build.mkdir(parents=True, exist_ok=True)
    (build / 'project.godot').write_text('config_version=4\n')
    (build / 'native_import.gd').write_text(NATIVE_HELPER)
    source = next(r for r in recipe['resources'] if r['fix_alpha_edges'])
    output = build / 'transition-imported.png'
    metrics_path = build / 'font-metrics.json'
    font = recipe['font']
    env = dict(os.environ, BATTLE_INPUT=str(safe_path(root, source['source'])),
               BATTLE_OUTPUT=str(output), BATTLE_FONT=str(safe_path(root, font['source'])),
               BATTLE_FONT_SIZE=str(font['size']), BATTLE_FONT_TOP=str(font['extra_spacing_top']),
               BATTLE_FONT_BOTTOM=str(font['extra_spacing_bottom']), BATTLE_FIRST=str(font['first']),
               BATTLE_LAST=str(font['last']), BATTLE_METRICS=str(metrics_path),
               XDG_DATA_HOME=str(build / 'userdata'))
    result = subprocess.run([str(godot), '--path', str(build), '-s', 'native_import.gd'],
                            env=env, text=True, capture_output=True, timeout=60, check=True)
    (build / 'native-import.log').write_text(result.stdout + result.stderr)
    return output, read_json(metrics_path)


def font_atlas(root: Path, font: dict, metrics: dict, destination: Path) -> tuple[list[dict], list[int]]:
    from PIL import Image, ImageDraw, ImageFont
    face = ImageFont.truetype(str(safe_path(root, font['source'])), font['size'])
    count = font['last'] - font['first'] + 1
    cw, ch = font['cell']
    width, height = font['columns'] * cw, ((count + font['columns'] - 1) // font['columns']) * ch
    atlas = Image.new('RGBA', (width, height))
    draw = ImageDraw.Draw(atlas)
    glyphs = []
    for index, codepoint in enumerate(range(font['first'], font['last'] + 1)):
        char = chr(codepoint)
        advance = float(face.getlength(char))
        native_advance = metrics['advances'][index]
        # A missing glyph must remain missing, not become FreeType's .notdef box.
        if native_advance == 0:
            glyphs.append(dict(codepoint=codepoint, u=0, v=0, width=0, height=0,
                               advance=0, offset_x=0, offset_y=0))
            continue
        if abs(advance - native_advance) > 0.001:
            raise ValueError('Font advance differs from pinned native Godot: ' + repr(char))
        x, y = index % font['columns'] * cw, index // font['columns'] * ch
        bbox = face.getbbox(char)
        if bbox[0] < 0 or bbox[1] < 0 or bbox[2] > cw or bbox[3] > ch:
            raise ValueError('Glyph exceeds reviewed cell: ' + repr(char))
        draw.text((x, y), char, font=face, fill=(255, 255, 255, 255))
        glyphs.append(dict(codepoint=codepoint, u=x, v=y, width=cw, height=ch,
                           advance=advance, offset_x=0, offset_y=metrics['ascent']-face.getmetrics()[0]))
    atlas.save(destination)
    return glyphs, [width, height]


def compile_assets(root: Path, tex3ds: Path, godot: Path, out: Path = OUT) -> None:
    from PIL import Image, __version__ as pillow_version
    recipe = read_json(RECIPE)
    validate_source(root, recipe, read_json(ROOT / 'upstream.lock'))
    if git(root, 'rev-parse', 'HEAD') != recipe['commit'] or git(root, 'status', '--porcelain'):
        raise ValueError('Battle assets require pristine pinned upstream')
    build = ROOT / 'build/battle-assets'
    imported, metrics = native_import(root.resolve(), recipe, godot.resolve(), build.resolve())
    out.mkdir(parents=True, exist_ok=True)
    records = []
    for resource in recipe['resources']:
        source = imported if resource['fix_alpha_edges'] else safe_path(root, resource['source'])
        image = Image.open(source).convert('RGBA')
        if list(image.size) != resource['size']:
            raise ValueError('Battle image dimensions changed: ' + resource['source'])
        if 'crop' in resource:
            x, y, w, h = resource['crop']
            image = image.crop((x, y, x + w, y + h))
            source = build / (resource['name'] + '-crop.png')
            image.save(source)
        target = out / Path(resource['output']).name
        if resource['kind'] == 'indexed':
            target.write_bytes(encode_indexed(image, tuple(resource['grid'])))
            data = decode_indexed(target.read_bytes())
            records.append(dict(resource, width=data['width'], height=data['height'],
                                frames=data['frames'], palette=data['palette']))
        else:
            subprocess.run([str(tex3ds), '-f', 'rgba8', '-z', 'none', '-o', str(target), str(source)], check=True)
            records.append(dict(resource, width=image.width, height=image.height,
                                frames=resource['grid'][0] * resource['grid'][1]))
    glyphs, font_size = font_atlas(root, recipe['font'], metrics, build / 'font.png')
    font_target = out / Path(recipe['font']['output']).name
    subprocess.run([str(tex3ds), '-f', 'rgba8', '-z', 'none', '-o', str(font_target), str(build / 'font.png')], check=True)
    records.append(dict(id=recipe['font']['id'], name='font', kind='texture', output=recipe['font']['output'],
                        width=font_size[0], height=font_size[1], grid=[1, 1], frames=1))
    outputs = {Path(r['output']).name: {'sha256': sha(out / Path(r['output']).name),
               'bytes': (out / Path(r['output']).name).stat().st_size} for r in records}
    receipt = dict(schema=1, recipe=recipe, resources=records, glyphs=glyphs, font_metrics=metrics,
                   pixel_format='Lossless RGBA8 texture and BPX1 palette+indices; nearest sampling',
                   native_import=dict(godot_sha256=sha(godot), helper_sha256=hashlib.sha256(NATIVE_HELPER.encode()).hexdigest(),
                                      result_sha256=sha(imported), imported_palette=records[0]['palette']),
                   tex3ds_sha256=sha(tex3ds), pillow_version=pillow_version, outputs=outputs,
                   limits='Font is original EBMain16px, advances checked against native Godot3.6.2; glyph raster pixels still require rendered GPU comparison. No hardware/performance claim.')
    write_json(receipt_path(out, ROOT), receipt)
    print('Compiled battle assets: ' + str(sum(v['bytes'] for v in outputs.values())) + ' bytes')


def verify(root: Path, out: Path = OUT) -> None:
    recipe = read_json(RECIPE)
    validate_source(root, recipe, read_json(ROOT / 'upstream.lock'))
    receipt = read_json(receipt_path(out, ROOT))
    if receipt.get('schema') != 1 or receipt.get('recipe') != recipe:
        raise ValueError('Stale battle asset receipt')
    expected = {Path(r['output']).name for r in recipe['resources']} | {Path(recipe['font']['output']).name}
    if set(receipt['outputs']) != expected or {p.name for p in out.iterdir()} != expected |receipt_entries(out, ROOT):
        raise ValueError('Unexpected/missing battle output files')
    for name, record in receipt['outputs'].items():
        path = safe_path(out, name)
        if sha(path) != record['sha256'] or path.stat().st_size != record['bytes']:
            raise ValueError('Battle output differs from receipt: ' + name)
        if path.suffix == '.bpx':
            decode_indexed(path.read_bytes())
    print('Verified source-pinned battle entry assets')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['compile', 'verify'])
    parser.add_argument('--root', type=Path, default=ROOT / 'upstream/MOTHER-Encore')
    parser.add_argument('--tex3ds', type=Path, default=Path('/opt/devkitpro/tools/bin/tex3ds'))
    parser.add_argument('--godot', type=Path)
    args = parser.parse_args()
    try:
        if args.action == 'compile':
            if args.godot is None:
                raise ValueError('Compile requires pinned Godot3.6.2 --godot for native image import')
            compile_assets(args.root, args.tex3ds, args.godot)
        else:
            verify(args.root)
        return 0
    except (OSError, ValueError, KeyError, TypeError, ImportError, subprocess.SubprocessError) as error:
        print('BATTLE ASSET ERROR: ' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
