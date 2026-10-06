#!/usr/bin/env python3
"""Reviewed DialogueBox constructor/input bindings, borrowing the 47-node recipe."""
from __future__ import annotations
import argparse, hashlib, re, struct, sys, zlib
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.podunk_scene import PIN, read, write, sha, require
IR = ROOT / 'content/native-field-dialogue-root-script.json'
REVIEW = ROOT / 'reports/field-dialogue-root-script/source-review.json'
OUT = ROOT / 'romfs/data/podunk-dialogue-root-script.encdroot'
SCENE = 'Nodes/Ui/DialogueBox.tscn'
DIALOGUE = 'Scripts/UI/DialogueBox.gd'
ABSTRACT = 'Scripts/UI/AbstractDialogueBox.gd'

def extract():
    recipe = read(ROOT / 'content/dialogue-node-recipe.json')
    life = read(ROOT / 'content/native-field-dialogue-lifecycle.json')
    ui = read(ROOT / 'content/native-field-dialogue-ui.json')
    source = ROOT / 'upstream/MOTHER-Encore'
    a = (source / ABSTRACT).read_text(encoding='utf8')
    g = (source / DIALOGUE).read_text(encoding='utf8')
    require(recipe['commit'] == PIN and len(recipe['records']) == 47, 'Actual recipe changed')
    actions = re.findall(r'event\.is_action_pressed\("([^"]+)"\)', a)
    actions = list(dict.fromkeys(actions))
    require(len(actions) == 3 and '_action_press(btn_next, btn_cancel)' in a, 'Source input contract changed')
    require('!$AnimationPlayer.is_playing() and $WaitTimer.time_left == 0 and _can_input' in g, 'Source input gates changed')
    require('$Dialoguebox/Arrow.cursor_index = 0' in g and '$InputSound.play()' in g, 'Source selection contract changed')
    ctor = re.search(r'preload\("res://([^"]+)"\)', g)
    bullet = re.search(r'var _bullet_string := "([^"]+)" % tr\("([^"]+)"\)', a)
    require(ctor and bullet, 'Constructor resource/translation missing')
    defaults = {}
    for text in (a, g):
        for name, value in re.findall(r'^var (_\w+) := (true|false|-?[0-9]+(?:\.[0-9]+)?|"[^"]*"|\{\}|\[\])\s*$', text, re.M):
            if value in ('true', 'false'): v = value == 'true'
            elif value == '{}': v = {}
            elif value == '[]': v = []
            elif value.startswith('"'): v = value[1:-1]
            else: v = float(value) if '.' in value else int(value)
            defaults[name] = v
    refs = []
    for path in re.findall(r'\$([\w/]+)', g[:g.index('func start_from_id')]):
        record = next(r for r in recipe['records'] if r['node'] == path)
        refs.append(dict(path=path, id=record['id'], native_class=record['native_class']))
    # Options is an onready reference. UI owns its precise native layout.
    option = next(r for r in ui['nodes'] if r['role'] == 9)
    require(any(r['id'] == option['id'] for r in refs), 'Options reference not source-derived')
    sources = {p: sha(source/p) for p in (SCENE, DIALOGUE, ABSTRACT, ctor[1])}
    for p, h in sources.items():
        require(h == read(ROOT/'compatibility/upstream-inventory.json')['files'][p]['sha256'], 'Source changed '+p)
    data = dict(schema=1, kind='encore.field-dialogue-root-script.source-ir', commit=PIN,
        scene=SCENE, scene_id=recipe['scene_id'], scene_sha256=recipe['source_sha256'],
        recipe_sha256=sha(ROOT/'content/dialogue-node-recipe.json'),
        lifecycle_sha256=sha(ROOT/'content/native-field-dialogue-lifecycle.json'),
        actions=actions, defaults=defaults, constructor_resource=ctor[1],
        bullet_format=bullet[1], bullet_key=bullet[2], references=refs,
        open_animation=re.search(r'\$AnimationPlayer.play\("([^"]+)"\)', g[g.index('func _show_box'):])[1], options_id=option['id'], cursor_reset=int(re.search(r'Arrow.cursor_index = ([0-9]+)', g)[1]), sources=sources,
        wait_method=re.search(r'func (_on_\w+_timeout)\(', g)[1],
        name_method=re.search(r'func (_set_nametag)\(', g)[1],
        wait_signal=re.search(r'\[connection signal="([^"]+)" from="WaitTimer" to="\." method="_on_WaitTimer_timeout"\]', (source/SCENE).read_text(encoding='utf8'))[1],
        scene_admitted=False,
        pending=['Programme executes reviewed phrase commands and lifecycle executes actual begin/end; this owner does not interpret GDScript',
                 'Actor preload, translated bullet, native AnimationPlayer observation, real signals and actual shared physics/input dispatch are required'])
    write(IR, data)
    write(REVIEW, dict(schema=1, commit=PIN, ir_sha256=sha(IR), producer_sha256=sha(Path(__file__)), sources=sources,
        scope='Source constructor, onready resolution, sole existing printer physics, action gates and actual Cursor bridge; no automatic scene Ready'))

def load():
    d, r = read(IR), read(REVIEW)
    require(d['schema'] == 1 and d['commit'] == PIN and not d['scene_admitted'] and r['ir_sha256'] == sha(IR) and r['producer_sha256'] == sha(Path(__file__)), 'Root script review changed')
    require(d['recipe_sha256'] == sha(ROOT/'content/dialogue-node-recipe.json') and d['lifecycle_sha256'] == sha(ROOT/'content/native-field-dialogue-lifecycle.json'), 'Root recipe/lifecycle changed')
    for p, h in d['sources'].items(): require(sha(ROOT/'upstream/MOTHER-Encore'/p) == h, 'Root source changed '+p)
    return d

def encode(d):
    b = bytearray(128)
    def u(v): b.extend(struct.pack('<I', v))
    def t(s): v=s.encode('utf8'); u(len(v)); b.extend(v)
    b.extend(bytes.fromhex(d['recipe_sha256']))
    for p in (DIALOGUE, ABSTRACT): b.extend(bytes.fromhex(d['sources'][p]))
    u(d['options_id']); u(d['cursor_reset']); t(ABSTRACT)
    for s in d['actions']+[d['constructor_resource'],d['bullet_format'],d['bullet_key'],d['wait_method'],d['name_method'],d['open_animation'],d['wait_signal']]: t(s)
    b.extend(bytes.fromhex(d['sources'][d['constructor_resource']]))
    # Typed schema binds only source state used here; other initial dictionaries
    # and pending command state stay explicitly empty in the source receipt.
    for key in ('_can_input','_auto_advance','_finished','_stopped','_dialogue_box_shown','_name_box_shown'):
        require(type(d['defaults'][key]) is bool, 'Unknown constructor type '+key); u(int(d['defaults'][key]))
    t(d['defaults']['_phrase_num'])
    b.extend(struct.pack('<q', d['defaults']['_dialog_response']))
    struct.pack_into('<8s8I', b, 0, b'ENCDROT1', 1, 128, len(b), zlib.crc32(b[128:]), 0x454e004e, 1, 47, d['scene_id'])
    b[40:60] = bytes.fromhex(PIN); b[60:92] = bytes.fromhex(d['scene_sha256']); b[92:124] = bytes.fromhex(sha(IR))
    return bytes(b)

def stage_files(source):
    raw=encode(load()); path=Path('data/podunk-dialogue-root-script.encdroot')
    require((Path(source)/path).read_bytes()==raw, 'Staged root script changed')
    return {path:raw}

def main():
    p=argparse.ArgumentParser(); p.add_argument('action',choices=['extract','compile','verify']); a=p.parse_args()
    if a.action=='extract': extract(); return
    raw=encode(load())
    if a.action=='verify': require(OUT.read_bytes()==raw,'Root script pack stale')
    else: OUT.parent.mkdir(parents=True,exist_ok=True); OUT.write_bytes(raw)
    print('Dialogue source root:', len(raw), 'bytes; actual printer/Cursor owner required')
if __name__=='__main__':
    try: main()
    except (ValueError,KeyError,TypeError,OSError,struct.error) as e: sys.exit('DIALOGUE ROOT ERROR: '+str(e))
