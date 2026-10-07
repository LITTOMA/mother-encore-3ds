#!/usr/bin/env python3
"""Supplement the existing Prompt core with actual native leaf bindings."""
from pathlib import Path
import argparse, hashlib, json, re, struct, sys, zlib
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.podunk_scene import PIN, SCENE, read, write, sha, require, stable
from tools.extract_battle_entry import Extractor, node
from tools.field_prompts import validate as validate_prompts
IR = ROOT / 'content/native-prompt-native.json'
PACK = ROOT / 'romfs/data/podunk.encpromptnative'
REVIEW = ROOT / 'reports/prompt-native/source-review.json'
SOURCE = 'Nodes/Ui/ButtonPrompt.tscn'
ENGINE = Path('D:/GameDev/3DS/encore-native/build/private-podunk-source-20261006')

def derive():
    ex = Extractor(ROOT)
    text = ex.text(SOURCE)
    script = ex.text('Scripts/UI/Button Prompt.gd')
    global_script = ex.text('Scripts/global/globalData.gd')
    input_script = ex.text('Scripts/global/text_tools.gd')
    ex.data('Fonts/BottleRocket.tres'); ex.data('Graphics/UI/select_arrow.png')
    require(re.search(r'yield\(\$AnimationPlayer,\s*"animation_finished"\)',script) and 'set_process(false)' in script, 'Prompt coroutine/process changed')
    require('var button_prompts' in global_script, 'Prompt setting member changed')
    player_script=ex.text('Scripts/Main/party/Player.gd')
    require(re.search(r'func is_paused\([^)]*\)[^\n]*:\s*\n\s*return _paused',player_script), 'Actual Player paused getter changed')
    require('get_key_name(key: String' in input_script, 'Prompt key name endpoint changed')
    box, label, arrow, ap = [node(text, p) for p in ('HBoxContainer', 'HBoxContainer/Label', 'Arrow', 'AnimationPlayer')]
    require(box['alignment'] == label['align'] == 1 and arrow['stretch_mode'] == 3 and arrow['rect_rotation'] == 90, 'Unsupported native Prompt Control layout')
    require(label['text'] == 'A' and arrow['use_parent_material'] is True, 'Prompt source text/material changed')
    require(set(ap) == {'anims/Float','anims/Hide','anims/Press','anims/RESET','anims/Show'}, 'Unknown native Prompt AP properties')
    t = read(ROOT / 'content/podunk-node-tree.json')
    p = validate_prompts(read(ROOT / 'content/native-field-prompts.json'))
    bypath = {r['node']: r for r in t['records']}
    records = []
    for prompt in p['records']:
        for role, suffix, klass in [(1,'HBoxContainer','HBoxContainer'),(2,'HBoxContainer/Label','Label'),(3,'Arrow','TextureRect'),(4,'AnimationPlayer','AnimationPlayer')]:
            path = prompt['node'] + '/' + suffix
            v = bypath[path]
            require(t['classes'][v['class_index']] == klass and not v['script'], 'Prompt native leaf identity/script changed')
            records.append(dict(id=v['id'], prompt=prompt['id'], parent=v['parent'], role=role, path=path, native_class=klass))
    inputs=[dict(v) for v in read(ROOT/'content/story-input-bindings.json')['bindings']]; equipment=read(ROOT/'content/native-field-equipment.json')
    require(len(inputs)==3 and all(equipment['parameters'][v['parameter']]==v['mask'] for v in inputs),'Prompt native input masks differ from checked equipment')
    from tools.field_equipment import PARAMETERS
    for v in inputs: v['parameter_id']=PARAMETERS.index(v['parameter'])+1
    return dict(schema=1, capability=1, rules=1, family=0x454e006b, commit=PIN, scene=SCENE,
                scene_id=t['scene_id'], source_sha256=t['source_sha256'], tree_sha256=sha(ROOT/'content/podunk-node-tree.json'),
                prompt_sha256=sha(ROOT/'content/native-field-prompts.json'), sources=ex.sources,
                settings_member='button_prompts', paused_member='_paused', font='Fonts/BottleRocket.tres',
                internal_group='idle_process_internal', started_signal='animation_started', finished_signal='animation_finished',
                box_size=[box['margin_right']-box['margin_left'], box['margin_bottom']-box['margin_top']],
                box_alignment=box['alignment'], label_alignment=label['align'], arrow_rotation=arrow['rect_rotation'], arrow_stretch=arrow['stretch_mode'],
                controls=[dict(role=role,position=[v['margin_left'],v['margin_top']],size=[v['margin_right']-v['margin_left'],v['margin_bottom']-v['margin_top']],text=v.get('text','')) for role,v in [(1,box),(2,label),(3,arrow)]], inputs=inputs,
                adapter_sha256=sha(ROOT/'content/story-input-bindings.json'),
                equipment_sha256=sha(ROOT/'content/native-field-equipment.json'), records=records)

def encode(d):
    out = bytearray()
    def u(*v): out.extend(struct.pack('<'+'I'*len(v),*v))
    def s(v): b=v.encode(); u(len(b)); out.extend(b)
    for key in ('settings_member','paused_member','font','internal_group','started_signal','finished_signal'): s(d[key])
    out.extend(struct.pack('<2f',*d['box_size']))
    u(d['box_alignment'],d['label_alignment'],int(d['arrow_rotation']),d['arrow_stretch'])
    out.extend(bytes.fromhex(d['prompt_sha256'])); out.extend(bytes.fromhex(d['tree_sha256']))
    for v in d['controls']:
        u(v['role']); out.extend(struct.pack('<4f',*v['position'],*v['size']))
        s(v['text'])
    u(len(d['inputs']))
    for v in d['inputs']: s(v['action']); s(v['label']); u(v['mask'],v['parameter_id'])
    out.extend(bytes.fromhex(d['adapter_sha256'])); out.extend(bytes.fromhex(d['equipment_sha256']))
    u(len(d['sources']))
    for p,h in sorted(d['sources'].items()): s(p); out.extend(bytes.fromhex(h))
    u(len(d['records']))
    for v in d['records']:
        u(v['id'],v['prompt'],v['parent'],v['role']); s(v['path']); s(v['native_class'])
    head=b'ENCPRN01'+struct.pack('<7I',1,1,1,d['family'],d['scene_id'],len(out),zlib.crc32(out))+bytes.fromhex(PIN)+bytes.fromhex(d['source_sha256'])
    return head+out

def load(root=ROOT):
    require(root == ROOT, 'Prompt native source root changed')
    d=read(IR); require(d==derive(),'Prompt native source binding changed')
    review=read(REVIEW); require(review['ir_sha256']==sha(IR),'Prompt native review changed')
    return d

def stage_files(root):
    d=load(); p=Path(root)/'data/podunk.encpromptnative'; raw=p.read_bytes()
    require(raw==encode(d),'Prompt native staged bytes changed')
    return {Path('data/podunk.encpromptnative'):raw}

def main():
    parser=argparse.ArgumentParser(); parser.add_argument('action',choices=['extract','compile']); args=parser.parse_args()
    if args.action=='extract':
        d=derive(); write(IR,d)
        engine={}
        for p in [ENGINE/'godot-3.6.2-animation_player.cpp',ENGINE/'dialogue-ui-engine/scene-gui-box_container.cpp',ENGINE/'dialogue-ui-engine/scene-gui-label.cpp']:
            require(p.is_file(),'Missing actual fixed engine review source'); engine[p.name]=sha(p)
        write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),engine_sources=engine,semantics=['Same typed Prompt core owns all five clip clocks; native AP runs that owner only in its actual IdleInternal phase','HBox center alignment uses the actual selected source-font width and minimum height; no fixed A label replacement','Source Label has no inherited Flash material; source Arrow inherits the parent Flash shader','Native leaf bindings cross-check actual scene/tree/native class/parent and actual ObjectDB identity; no script Ready is synthesized']))
    else:
        d=load(); PACK.parent.mkdir(parents=True,exist_ok=True); PACK.write_bytes(encode(d)); print('Prompt native:',len(d['records']),'actual leaves')
if __name__=='__main__': main()
