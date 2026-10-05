#!/usr/bin/env python3
"""Extract the original bounded Dad-normal option panel into ENCCHOIC data.

Reuses checked House cursor and Battle EBMain assets. Headless Godot checks the
original GridContainer/Label layout; no alternate menu artwork is generated.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.extract_battle_entry import Extractor, require, one, node
from tools.dad_record_dialogue import build as dad_build
from tools.phone_dialogue import write_json
from tools.menu_audio_binding import source_sound

RECIPE = ROOT / 'content/native-dialogue-choices.json'
PACK = ROOT / 'romfs/data/opening.encchoices'
REPORT = ROOT / 'reports/dad-record'
SOURCES = ['Nodes/Ui/DialogueBox.tscn', 'Nodes/Ui/DialogueOptions.tscn', 'Nodes/Ui/arrow.tscn',
           'Scripts/UI/DialogueBox.gd', 'Scripts/UI/AbstractDialogueBox.gd', 'Scripts/UI/cursor.gd',
           'Fonts/EBMain.tres', 'Fonts/EBMain.ttf', 'Graphics/UI/Inventory/cursor.png',
           'Graphics/UI/Inventory/cursor.png.import', 'Audio/Sound effects/Cursor 2.mp3',
           'Audio/Sound effects/Cursor 1.mp3', 'LICENSE']


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def rect(p):
    x, y = p.get('margin_left', 0), p.get('margin_top', 0)
    return [x, y, p.get('margin_right', 0) - x, p.get('margin_bottom', 0) - y]


def extract():
    ex = Extractor(ROOT)
    for path in SOURCES:
        ex.data(path)
    graph = dad_build(ROOT)
    scene = ex.text('Nodes/Ui/DialogueBox.tscn')
    grid = node(scene, 'Dialoguebox/Options')
    arrow = node(scene, 'Dialoguebox/Arrow')
    label = node(ex.text('Nodes/Ui/DialogueOptions.tscn'), '.')
    cursor = ex.text('Scripts/UI/cursor.gd')
    dialogue = ex.text('Scripts/UI/DialogueBox.gd')
    require(grid['columns'] == 3 and grid['custom_constants/hseparation'] == 12
            and label['rect_min_size'] == [60, 10] and label['size_flags_horizontal'] == 3,
            'Unreviewed option grid/label geometry')
    require(arrow['skip_empty_labels'] and arrow['skip_hidden_items'] and arrow['move_sfx']
            and all(k not in arrow for k in ('loop_around', 'select_sfx', 'cancel_sfx', 'cancel_on')),
            'Unreviewed option cursor behavior')
    require('export var loop_around := false' in cursor and 'export var select_sfx := false' in cursor
            and 'export var cancel_sfx := false' in cursor and '_clear_dialogue()\n\t\t\t\t\t_handle_phrase()\n\t\t\t\t\t$InputSound.play()' in dialogue,
            'Option accept/cancel semantics changed')
    require('_options.clear()\n\t\t_print_new_line()' in dialogue
            and 'if _options_count > 3:\n\t\t\t_print_new_line()' in dialogue,
            'Option trailing blank line behavior changed')
    arrow_scene = ex.text('Nodes/Ui/arrow.tscn')
    speed = float(one(r'"speed": ([\d.]+)', arrow_scene, 'arrow rate')[1])
    frames = [int(n)-1 for n in re.findall(r'SubResource\( (\d) \)',
              one(r'"frames": \[([^\n]+)\]', arrow_scene, 'arrow frames')[1])]
    size = [float(v) for v in one(r'export var _cursor_size := Vector2\(([^)]+)\)', cursor, 'arrow size')[1].split(',')]
    require(size == [8, 8], 'Unreviewed arrow source size')
    house = json.loads((ROOT / 'content/native-house-presentation.json').read_text())
    original = next(r for r in house['resources'] if r['role'] == 'cursor')
    require(ex.png_size('Graphics/UI/Inventory/cursor.png') == [original['width'], original['height']],
            'Shared House cursor dimensions changed')
    resource = {k: original[k] for k in ('path', 'width', 'height', 'columns', 'rows', 'sha256')}
    require(sha(ROOT / 'romfs' / resource['path']) == resource['sha256'], 'Shared House cursor changed')
    battle = json.loads((ROOT / 'content/asset-receipts/graphics/battle/lamp/source.json').read_text())
    require(battle['recipe']['sources']['Fonts/EBMain.ttf'] == ex.sources['Fonts/EBMain.ttf'],
            'Shared Battle font has different source')
    groups = list(graph['program']['choice_groups'])
    require(len(groups) == 1 and len(groups[0]['options']) == 2, 'Only reviewed Dad-normal option pair supported')
    from tools.pillow_dialogue import load_documents, tutorial_graph, TUTORIAL
    from tools.link_pillow_content import texts
    docs = load_documents(ex)
    _, translations = texts(ex, docs)
    from tools.doll_postwin import return_duration
    groups.extend(tutorial_graph(docs[TUTORIAL], translations, return_duration(dialogue))['choice_groups'])
    from tools.storage_dialogue import load as storage_dialogue
    storage=storage_dialogue();groups.append(storage['choice_group'])
    for path in storage['sources']:ex.data(path)
    from tools.basement_progression import load as basement_source
    basement=basement_source(ROOT);groups.extend(basement['choice_groups'])
    for path in basement['sources']:ex.data(path)
    recipe = dict(schema=1, commit=ex.lock['commit'], sources=ex.sources,
        scope='Original Dad-normal two-option panel; English source; no general dialogue UI translator',
        graph_sha256=hashlib.sha256(json.dumps(graph['program'], sort_keys=True).encode()).hexdigest(),
        columns=grid['columns'], child_count=6, grid=rect(grid), option_min_size=label['rect_min_size'],
        trailing_blank_lines=1 + int(len(groups[0]['options']) > 3),
        separation=grid['custom_constants/hseparation'], arrow_size=size,
        arrow_offset=[arrow['cursor_offset'][0]-size[0]/6, arrow['cursor_offset'][1]+size[1]/2],
        arrow_move=float(one(r'const TWEEN_LENGTH := ([\d.]+)', cursor, 'arrow tween')[1]),
        arrow_loop=len(frames)/speed, arrow_keys=[dict(time=i/speed, frame=f) for i, f in enumerate(frames)],
        arrow_resource=resource, font_path='graphics/battle/lamp/font.t3x', groups=groups,
        sounds=['cursor1', 'cursor2', 'cursor2'],
        policies=dict(loop_around=False, show_after_text_complete=True, clear_text_on_result=True,
                      confirm_and_cancel_sound_after_target_phrase=True, navigation_uses_caller_repeat=True),
        dependencies={p: sha(ROOT / p) for p in ['content/native-house-presentation.json',
            'content/asset-receipts/graphics/battle/lamp/source.json', 'romfs/graphics/battle/lamp/font.t3x', 'romfs/' + resource['path']]})
    probe = dict(font=str(ex.upstream / 'Fonts/EBMain.ttf'), grid=grid,
                 minimum=label['rect_min_size'], size_flags=label['size_flags_horizontal'],
                 labels=[o['text'] for o in groups[0]['options']], child_count=recipe['child_count'])
    return recipe, probe


PROBE = '''extends SceneTree
func _init(): call_deferred("run")
func run():
 var f=File.new()
 assert(f.open("res://input.json",File.READ)==OK)
 var c=JSON.parse(f.get_as_text()).result
 f.close()
 var data=DynamicFontData.new()
 data.font_path=c.font
 var font=DynamicFont.new()
 font.font_data=data
 font.extra_spacing_top=-1
 font.extra_spacing_bottom=-1
 var root=Control.new()
 root.rect_size=Vector2(320,180)
 get_root().add_child(root)
 var grid=GridContainer.new()
 root.add_child(grid)
 for key in c.grid: grid.set(key,c.grid[key])
 var labels=[]
 for i in c.child_count:
  var label=Label.new()
  label.rect_min_size=Vector2(c.minimum[0],c.minimum[1])
  label.size_flags_horizontal=c.size_flags
  label.add_font_override("font",font)
  label.text=c.labels[i] if i<c.labels.size() else "Option%d"%(i+1)
  grid.add_child(label)
  label.visible=i<c.labels.size()
  labels.append(label)
 for _i in range(4): yield(self,"idle_frame")
 var out={"engine":Engine.get_version_info(),"font_height":font.get_height(),"text_color":[],"labels":[],"grid":[grid.rect_position.x,grid.rect_position.y,grid.rect_size.x,grid.rect_size.y]}
 var color=labels[0].get_color("font_color")
 out.text_color=[color.r8,color.g8,color.b8,color.a8]
 for i in c.labels.size():
  var l=labels[i]
  out.labels.append([l.rect_position.x,l.rect_position.y,l.rect_size.x,l.rect_size.y])
 assert(f.open("res://native-layout.json",File.WRITE)==OK)
 f.store_string(JSON.print(out,"  "))
 f.close()
 print("DIALOGUE_CHOICES_REFERENCE_OK")
 quit()
'''


def generate(godot):
    recipe, probe = extract()
    work = REPORT / 'native-probe'
    work.mkdir(parents=True, exist_ok=True)
    write_json(work / 'input.json', probe)
    (work / 'project.godot').write_text('config_version=4\n[logging]\nfile_logging/enable_logging=false\n')
    (work / 'probe.gd').write_text(PROBE)
    result = subprocess.run([str(godot.resolve()), '--path', str(work.resolve()), '-s', 'probe.gd'],
        capture_output=True, text=True, timeout=30, env=dict(os.environ, XDG_DATA_HOME=str(work / 'userdata')))
    (REPORT / 'native-layout.log').write_text(result.stdout + result.stderr)
    require(result.returncode == 0 and 'DIALOGUE_CHOICES_REFERENCE_OK' in result.stdout
            and 'ERROR' not in result.stdout + result.stderr, 'Original option layout probe failed')
    ref = json.loads((work / 'native-layout.json').read_text())
    require(ref['engine']['hash'] == '3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8', 'Unknown Godot layout version')
    write_json(REPORT / 'native-layout.json', ref)
    recipe.update(native_reference_sha256=sha(REPORT / 'native-layout.json'),
                  option_rects=ref['labels'], font_height=ref['font_height'],
                  text_color=sum(v << (8*i) for i, v in enumerate(ref['text_color'])))
    require(ref['grid'] == recipe['grid'], 'Unexpected native option grid resize')
    extra_refs = []
    extra_rects = []
    for index, group in enumerate(recipe['groups'][1:]):
        probe['labels'] = [option['text'] for option in group['options']]
        write_json(work / 'input.json', probe)
        result = subprocess.run([str(godot.resolve()), '--path', str(work.resolve()), '-s', 'probe.gd'], capture_output=True, text=True, timeout=30, env=dict(os.environ, XDG_DATA_HOME=str(work / 'userdata')))
        require(result.returncode == 0 and 'DIALOGUE_CHOICES_REFERENCE_OK' in result.stdout and 'ERROR' not in result.stdout + result.stderr, 'Additional source option layout probe failed')
        extra = json.loads((work / 'native-layout.json').read_text())
        require(extra['engine'] == ref['engine'] and extra['font_height'] == ref['font_height'] and extra['grid'] == ref['grid'] and extra['text_color'] == ref['text_color'], 'Additional option layout context changed')
        target = REPORT / ('native-layout-extra-' + str(index) + '.json')
        write_json(target, extra); extra_refs.append(sha(target)); extra_rects.append(extra['labels'])
    recipe.update(extra_native_reference_sha256=extra_refs, extra_option_rects=extra_rects)
    write_json(RECIPE, recipe)
    compile_pack(recipe)


def verify_recipe(recipe):
    source, _ = extract()
    require(set(recipe) == set(source) | {'native_reference_sha256', 'option_rects', 'font_height', 'text_color', 'extra_native_reference_sha256', 'extra_option_rects'},
            'Unknown choice recipe fields')
    for key, value in source.items():
        require(recipe[key] == value, 'Changed source-derived choices: ' + key)
    require(sha(REPORT / 'native-layout.json') == recipe['native_reference_sha256'], 'Changed native choice reference')
    ref = json.loads((REPORT / 'native-layout.json').read_text())
    require(recipe['option_rects'] == ref['labels'] and recipe['font_height'] == ref['font_height']
            and recipe['text_color'] == sum(v << (8*i) for i, v in enumerate(ref['text_color'])),
            'Changed native choice geometry/color')
    require(len(recipe['extra_option_rects']) == len(recipe['extra_native_reference_sha256']) == len(recipe['groups']) - 1, 'Additional choice geometry count')
    for index, digest in enumerate(recipe['extra_native_reference_sha256']):
        path = REPORT / ('native-layout-extra-' + str(index) + '.json')
        require(sha(path) == digest, 'Changed additional native choice reference')
        extra = json.loads(path.read_text())
        require(extra['labels'] == recipe['extra_option_rects'][index] and extra['grid'] == ref['grid'] and extra['font_height'] == ref['font_height'], 'Changed additional choice geometry')


def encode(r):
    payload = bytearray()
    def put(fmt, *v): payload.extend(struct.pack('<' + fmt, *v))
    def string(s):
        b = s.encode('ascii'); put('I', len(b)); payload.extend(b)
    put('2I4f2f2f2f2df2I', r['columns'], r['child_count'], *r['grid'], *r['option_min_size'],
        *r['arrow_offset'], *r['arrow_size'], r['arrow_move'], r['arrow_loop'], r['font_height'], r['text_color'], r['trailing_blank_lines'])
    for s in r['sounds']: string(source_sound(s))
    a = r['arrow_resource']; string(a['path']); put('4I', a['width'], a['height'], a['columns'], a['rows'])
    string(r['font_path'])
    put('I', len(r['arrow_keys']))
    for k in r['arrow_keys']: put('dI', k['time'], k['frame'])
    put('I', len(r['groups']))
    for group_index, group in enumerate(r['groups']):
        for k in ('id', 'program_identity', 'source_label'): string(group[k])
        put('4I', group['program_command_count'], group['initial_selection'], group['cancel_target_pc'], len(group['options']))
        for option, rect_value in zip(group['options'], r['option_rects'] if group_index == 0 else r['extra_option_rects'][group_index - 1]):
            string(option['translation_key']); string(option['text']); put('I4f', option['target_pc'], *rect_value)
    return struct.pack('<8s4I', b'ENCCHOIC', 1, len(payload)+24, zlib.crc32(payload), 1) + payload


def compile_pack(recipe=None):
    recipe = recipe or json.loads(RECIPE.read_text())
    verify_recipe(recipe)
    PACK.parent.mkdir(parents=True, exist_ok=True)
    PACK.write_bytes(encode(recipe))


def stage_files(source_root):
    recipe = json.loads(RECIPE.read_text())
    verify_recipe(recipe)
    root = Path(source_root).resolve()
    out = {}
    for name in ('data/opening.encchoices', recipe['arrow_resource']['path'], recipe['font_path']):
        p = Path(name)
        require(not p.is_absolute() and '..' not in p.parts and '\\' not in name and ':' not in name, 'Choice stage path rejected')
        target = (root / p).resolve()
        require(target.is_relative_to(root), 'Choice stage path escaped root')
        raw = target.read_bytes()
        if name == 'data/opening.encchoices': require(raw == encode(recipe), 'Stale choice pack')
        else: require(hashlib.sha256(raw).hexdigest() == recipe['dependencies']['romfs/' + name], 'Changed choice shared resource')
        out[p] = raw
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['extract', 'compile', 'verify'])
    parser.add_argument('--godot', type=Path, default=Path('/workspace/scratch/c6ba063dd54d/toolchain/godot-3.6.2/Godot_v3.6.2-stable_linux_headless.64'))
    args = parser.parse_args()
    if args.action == 'extract': generate(args.godot)
    elif args.action == 'compile': compile_pack()
    else:
        recipe = json.loads(RECIPE.read_text()); verify_recipe(recipe)
        require(PACK.read_bytes() == encode(recipe), 'Stale choice pack')
    print('Dialogue choices ' + args.action + ' complete')


if __name__ == '__main__': main()
