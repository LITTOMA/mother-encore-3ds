#!/usr/bin/env python3
"""Checked Pillow entry adapter, preserving the complete original two-layer BBG.

Only the three original Pillow images are converted here. Shared transitions,
fonts and UI retain their existing receipts. No new rendering path is selected.
"""
from __future__ import annotations
import argparse, copy, csv, io, json, re, shutil, struct, subprocess, sys
from pathlib import Path
from PIL import Image
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT)); sys.path.insert(0, str(ROOT / 'tools'))
from tools.extract_battle_entry import Extractor, require, properties, node
from tools.battle_assets import encode_indexed, decode_indexed, verify as verify_common_assets
from tools.upstream import read_json, write_json
from tools import native_battle
from tools.doll_entry_asset import sha

PIN = '7d9246600fffe518408f5830d4848635019005a3'
IR_PATH = ROOT / 'content/pillow-entry.json'
OUT = ROOT / 'romfs/pillow-preview'
PACK = ROOT / 'romfs/data/pillow-entry.encbattle'
RECEIPT = OUT / 'source.json'
REPORT = ROOT / 'reports/pillow-battle'
WORLD_SOURCE = 'Graphics/Character Sprites/Npcs/1dir/pillow.png'
WORLD_OUTPUT = 'pillow-preview/pillow-world.t3x'
EXTRA_ART = [('background', 'Graphics/Battle BGS/pillow.png', 'pillow-background.bpx', 'indexed'),
             ('enemy', 'Graphics/Battle Sprites/pillow.png', 'pillow-enemy.t3x', 'texture')]
POST_WIN = 'Podunk/cutscenes/minnie_leave'

def prepare_world(tex3ds):
    ex = Extractor(ROOT); size = ex.png_size(WORLD_SOURCE); ex.data(WORLD_SOURCE + '.import')
    actor = node(ex.text('Maps/podunk/Nintens House.tscn'), 'Objects/pillow')
    animation = ex.yaml(actor['yaml'].removeprefix('res://'))
    require(actor['sprite'] == 'Npcs/1dir/pillow' and animation['size'] == [4, 1], 'Changed Pillow world sprite binding')
    OUT.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(tex3ds), '-f', 'rgba8', '-z', 'none', '-o', str(ROOT / 'romfs' / WORLD_OUTPUT), str(ex.upstream / WORLD_SOURCE)], check=True)
    write_json(OUT / 'world-source.json', dict(commit=PIN, sources=ex.sources, path=WORLD_OUTPUT,
               width=size[0], height=size[1], columns=4, rows=1, sha256=sha(ROOT / 'romfs' / WORLD_OUTPUT),
               tex3ds_sha256=sha(tex3ds)))

def extract():
    ex = Extractor(ROOT); ir = ex.build()
    enemy = ex.yaml('Data/Battlers/pillow.yaml')
    scene = ex.yaml('Data/Dialogue/Podunk/cutscenes/pillow_attack.yaml')
    require(enemy['boss'] is False and enemy['music'] == '' and enemy['bg'] == 'pillow' and 'battlescript' not in enemy, 'Unknown Pillow encounter behavior')
    require(scene['6']['startbattle'] == {'battlers': [{'pillow': 'pillow'}], 'wincutscene': POST_WIN}, 'Changed Pillow battle boundary')
    ir['binary_version'] = 2
    ir['scope'].update(battle_id='pillow', completion='Pinned Pillow entrance and command menu; selected action execution is supplied by the separate checked round resource')
    ir['entry'].update(win_flag='', encountered_key='pillow', first_encounter_flag='pillow_fought',
                       post_battle_cutscenes={'win': POST_WIN}, can_run=False, source_can_run_argument=False,
                       can_run_explanation='DialogueBox._end_dialogue passes false for queued cutscene battles')
    size = ex.png_size('Graphics/Battle Sprites/pillow.png'); center = [ir['viewport']['width'] / 2, 147 / 2]
    ir['enemy'].update(id='pillow', data=enemy, pool_exp=enemy['exp'], pool_cash=enemy['cash'], sprite_size=size,
                       sprite_center=center, sprite_position=[center[i] - size[i] / 2 for i in range(2)])
    ir['presentation'].update(asset_recipe_path='content/pillow-entry.json', asset_receipt_path='romfs/pillow-preview/source.json')
    for row in csv.reader(io.StringIO(ex.text('Translations/TranslatedText/battlers - sheet.csv'))):
        if row and row[0] in ['PILLOW_NAME', 'PILLOW_DESC', 'PILLOW_ART']: ir['presentation']['translations_en'][row[0]] = row[1]
    for name, path, output, kind in EXTRA_ART:
        size = ex.png_size(path); ex.data(path + '.import')
        ir['presentation']['assets'][name] = dict(source=path, texture_size=size, grid=[1, 1], frame_size=size, output='pillow-preview/' + output, kind=kind)
    source = ex.text('Graphics/Battle BGS/pillow.bbg')
    layers = [dict(index=int(m[1]), properties=properties(m[2])) for m in re.finditer(r'^\[Layer (\d+)\]\s*\n(.*?)(?=^\[|\Z)', source, re.M | re.S)]
    require([l['index'] for l in layers] == [0, 1], 'Unknown Pillow layer count')
    ir['background'].update(source='Graphics/Battle BGS/pillow.bbg', layers=layers,
        texture_size=ir['presentation']['assets']['background']['texture_size'], native_layer_size=ir['presentation']['assets']['background']['texture_size'],
        native_control_size=[320, 180], uv_domain='STRETCH_TILE original texture176x176; source default shader, expanded400x240 viewport; distorted coordinates repeat')
    ir['audio']['overworld_battle_music_note'] = 'Pillow music field empty; existing source overworld music ownership remains; normal encounter jingle'
    world = read_json(OUT / 'world-source.json')
    require(world['commit'] == PIN and sha(ROOT / 'romfs' / world['path']) == world['sha256'], 'Stale Pillow world texture')
    for path, digest in world['sources'].items(): require(sha(ex.upstream / path) == digest, 'Changed world art source'); ex.data(path)
    actor = read_json(ROOT / 'romfs/actor-preview/source.json'); texture = actor['recipe']
    ir['binding'] = dict(stable_id=3, player_instance=0, enemy_instance=6, world_resources={
        'world_player': dict(path='actor-preview/ninten-main.t3x', width=texture['size'][0], height=texture['size'][1], columns=texture['grid'][0], rows=texture['grid'][1], sha256=actor['outputs']['ninten-main.t3x']['sha256']),
        'world_enemy': {k: world[k] for k in ['path', 'width', 'height', 'columns', 'rows', 'sha256']}})
    ex.data('Graphics/Character Sprites/Ninten/main.png'); ex.data('LICENSE')
    ir['party']['runtime_state'] = 'The caller supplies live HP, PP, EXP, level and effective stats. Entry baselines must never heal or reset the carried session.'
    ir['selectors']['background'] = 'Every pillow.bbg field retained; existing v2 move/ping-pong/oscillation mechanism; no palette division, approximation or new GPU path'
    ir['licence_review'] = 'Pinned LICENSE permits assets in game-related forks/modifications/translations; original art retained solely for the Mother: Encore port under its existing conditions'
    ir['sources'] = ex.sources
    return ir

def validate(ir):
    native_battle.verify_sources(ir)
    require(ir['commit'] == PIN and ir.get('binary_version') == 2 and ir['scope']['battle_id'] == 'pillow', 'Unreviewed Pillow identity/version')
    require(not ir['scope']['whole_battle_approved'] and not ir['scope']['combat_implemented'], 'Entry resource cannot approve combat')
    require(ir['binding']['stable_id'] == 3 and ir['binding']['enemy_instance'] == 6 and ir['binding']['player_instance'] == 0, 'Pillow Room binding changed')
    ex = Extractor(ROOT)
    require(ir['enemy']['id'] == 'pillow' and ir['enemy']['data'] == ex.yaml('Data/Battlers/pillow.yaml'), 'Pillow roster changed')
    require(ir['entry']['win_flag'] == '' and ir['entry']['post_battle_cutscenes'] == {'win': POST_WIN} and not ir['entry']['can_run'], 'Pillow battle policy changed')
    source = ex.text('Graphics/Battle BGS/pillow.bbg')
    expected = [dict(index=int(m[1]), properties=properties(m[2])) for m in re.finditer(r'^\[Layer (\d+)\]\s*\n(.*?)(?=^\[|\Z)', source, re.M | re.S)]
    require(ir['background']['source'] == 'Graphics/Battle BGS/pillow.bbg' and ir['background']['layers'] == expected, 'Changed Pillow background parameters')
    require('compatibility_policy' not in ir['background'] and all(not l['properties']['palette_shifting'] for l in expected), 'Unexpected Pillow palette policy')
    require(ir['background']['native_layer_size'] == ex.png_size('Graphics/Battle BGS/pillow.png') and ir.get('licence_review'), 'Changed Pillow background geometry/license')

def compile_art(ir, tex3ds):
    validate(ir); verify_common_assets(ROOT / 'upstream/MOTHER-Encore')
    receipt = copy.deepcopy(read_json(ROOT / 'romfs/battle-preview/source.json')); OUT.mkdir(parents=True, exist_ok=True)
    for name, path, filename, kind in EXTRA_ART:
        spec = ir['presentation']['assets'][name]; source = ROOT / 'upstream/MOTHER-Encore' / path; target = OUT / filename
        require(spec['source'] == path and spec['kind'] == kind and spec['output'] == 'pillow-preview/' + filename, 'Unreviewed Pillow asset output')
        with Image.open(source) as im: image = im.convert('RGBA')
        require(list(image.size) == spec['texture_size'], 'Changed Pillow dimensions')
        if kind == 'indexed': target.write_bytes(encode_indexed(image, (1, 1))); decode_indexed(target.read_bytes())
        else: subprocess.run([str(tex3ds), '-f', 'rgba8', '-z', 'none', '-o', str(target), str(source)], check=True)
        index = next(i for i, r in enumerate(receipt['resources']) if r['name'] == name)
        receipt['resources'][index] = dict(id=index, name=name, source=path, kind=kind, size=list(image.size), grid=[1, 1], output=spec['output'], fix_alpha_edges=False, width=image.width, height=image.height, frames=1)
    receipt['outputs'] = {Path(r['output']).name: dict(sha256=sha(ROOT / 'romfs' / r['output']), bytes=(ROOT / 'romfs' / r['output']).stat().st_size) for r in receipt['resources']}
    receipt.update(recipe=dict(schema=1, commit=PIN, source_ir_sha256=sha(IR_PATH), sources={p: ir['sources'][p] for _, p, _, _ in EXTRA_ART}, licence_review=ir['licence_review']),
                   tex3ds_sha256=sha(tex3ds), scope='Exact Pillow sprite and lossless background; shared checked entry/UI/font assets; original two layers through existing v2 rendering mechanism')
    write_json(RECEIPT, receipt)

def compile_pack(ir):
    validate(ir); receipt = read_json(RECEIPT)
    require(receipt['recipe']['source_ir_sha256'] == sha(IR_PATH), 'Stale Pillow asset receipt')
    tables = native_battle.lower(ir, receipt); blob = native_battle.encode(tables, ir['commit']); native_battle.parse_sections(blob)
    return blob, tables

def main():
    parser = argparse.ArgumentParser(); parser.add_argument('action', choices=['extract', 'assets', 'compile', 'verify']); parser.add_argument('--tex3ds', type=Path); args = parser.parse_args()
    if args.action in ['extract', 'assets']:
        tex3ds = args.tex3ds or Path(shutil.which('tex3ds') or '')
        require(tex3ds.is_file(), 'Need tex3ds from the reviewed toolchain (--tex3ds)')
    if args.action == 'extract': prepare_world(tex3ds); write_json(IR_PATH, extract()); print('Extracted pinned Pillow entry, complete two-layer background and exact world sprite'); return
    ir = read_json(IR_PATH)
    if args.action == 'assets': compile_art(ir, tex3ds); print('Compiled exact Pillow art and retained shared checked UI'); return
    blob, tables = compile_pack(ir)
    if args.action == 'compile':
        PACK.parent.mkdir(parents=True, exist_ok=True); PACK.write_bytes(blob)
        write_json(REPORT / 'entry-build.json', dict(schema=1, binary_version=2, commit=PIN, ir_sha256=sha(IR_PATH), pack_sha256=sha(PACK), pack_bytes=len(blob), sources=ir['sources'], resources=len(tables['resources']), scope=ir['scope'], renderer='existing two-layer v2; no new GPU path', gpu_reference='not run by this compiler'))
    else: require(PACK.read_bytes() == blob, 'Stale Pillow entry pack'); native_battle.stage_files(ROOT / 'romfs', PACK.relative_to(ROOT / 'romfs'))
    print('Pillow entry ' + args.action + ': ' + str(len(blob)) + ' bytes')

if __name__ == '__main__':
    try: main()
    except (ValueError, OSError, KeyError, TypeError, struct.error, subprocess.SubprocessError) as error: print('PILLOW ENTRY ERROR:', error, file=sys.stderr); raise SystemExit(1)
