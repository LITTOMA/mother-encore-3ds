#!/usr/bin/env python3
"""Compile the original four-frame phone atlas and source-backed object IR.

Outputs are independent staging resources. No NPC proxy, shadow, payphone gate,
automatic ring-on-load, shipping audio pack or existing house pack is created.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools"))
from tools.doll_dialogue import PIN, sha, require
from tools.extract_battle_entry import Extractor, node, properties, one, animation
from tools.phone_dialogue import write_json
from tools.upstream import safe_path
from tools.asset_receipts import receipt_path, receipt_entries
from tools import phone_presentation_bindings as presentation

HOUSE = 'Maps/podunk/Nintens House.tscn'
_identity = presentation.identity(ROOT)
SCENE = _identity['scene']
TEXTURE = _identity['sprite']['source']
RING_SOUND = _identity['sounds']['ring']
HANGUP_SOUND = _identity['sounds']['hangup']
SOURCES = ['LICENSE', HOUSE, SCENE, 'Maps/Testing/phone.gd', 'Scripts/Main/Interact Dialog.gd',
           'Scripts/Main/npc.gd', 'Scripts/Main/party/Player.gd', 'Scripts/Main/party/party_object.gd',
           'Scripts/Main/CutsceneArea.gd', 'Nodes/Reusables/CutsceneArea.tscn',
           'Scripts/Main/Flag Landmarks.gd', 'Scripts/global/global.gd',
           *[p for asset in [TEXTURE, RING_SOUND, HANGUP_SOUND] for p in [asset, asset + '.import']]]
OUT = ROOT / 'romfs/graphics/ui/phone'
IR = ROOT / 'content/phone-stage/presentation.json'
RECIPE = ROOT / 'content/phone-stage/assets.json'
REVIEW = ROOT / 'compatibility/reviews/phone-presentation.json'


def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


def shape(scene, identity):
    return properties(one(r'^\[sub_resource type="RectangleShape2D" id=' + str(identity) +
        r'\]\n(.*?)(?=^\[)', scene, 'phone rectangle', re.M | re.S)[1])['extents']


def transformed(origin, offset, scale=(1, 1)):
    # Godot Vector2 and transforms use native real_t float32 in this official build.
    return [f32(f32(origin[i]) + f32(f32(offset[i]) * f32(scale[i]))) for i in range(2)]


def build(root=ROOT):
    bindings = presentation.load(root)
    clips = {row['role']: row for row in bindings['clips']}
    scene, texture = bindings['scene'], bindings['sprite']['source']
    ring_sound, hangup_sound = bindings['sounds']['ring'], bindings['sounds']['hangup']
    idle_name, ring_name = clips['idle']['animation']['name'], clips['ring']['animation']['name']
    ex = Extractor(root)
    for path in SOURCES:
        ex.data(path)
    house, phone = ex.text(HOUSE), ex.text(scene)
    instance, base = node(house, 'Objects/Phone'), node(phone, '.')
    require(set(instance) == {'position', 'dialog', '_all_dialog'}, 'Unreviewed house phone override')
    require(node(house, '.').get('position', [0, 0]) == [0, 0]
            and not any(k in node(house, 'Objects') for k in ['position', 'rotation', 'scale']),
            'Unreviewed phone ancestor transform')
    require(base == {'position': [0, -4], 'script': {'ExtResource': 1},
                    'dialog': 'Reusable/dad_normal', '_key_item': 'PhoneCard'}, 'Unreviewed phone base')
    require(instance == {'position': [148, 677], 'dialog': 'Reusable/phonenoanswer',
        '_all_dialog': [['phone_ring', 'Podunk/dad_poltergeist'], ['talked_to_dad', 'Reusable/dad_normal']]},
        'Unreviewed phone dispatch or position')
    main = node(phone, bindings['sprite']['node'])
    width, height = ex.png_size(texture)
    columns, rows = main.get('hframes', 1), main.get('vframes', 1)
    collision, interact = node(phone, 'StaticBody2D/CollisionShape2D'), node(phone, 'interact/CollisionShape2D')
    solid = node(phone, 'StaticBody2D')
    require(solid == {'collision_layer': 573, 'collision_mask': 0}, 'Phone collision policy changed')
    require(collision == {'position': [1, 8.5], 'shape': {'SubResource': 2}}, 'Phone collision transform changed')
    require(shape(phone, 2) == [7.5, 4.5] and shape(phone, 3) == [15.7493, 19.1346], 'Phone shape changed')
    require(interact == {'modulate': [0, 1, .717647, 1], 'position': [-1.19209e-07, 15],
                        'scale': [1.01592, .940705], 'shape': {'SubResource': 3}}, 'Phone interaction transform changed')
    require(node(phone, 'interact') == {}, 'Phone inherited Area2D defaults changed')
    idle, ring = clips['idle']['animation'], clips['ring']['animation']
    script = ex.text(bindings['script'])
    require('export (bool) var _is_payphone: bool' in script and 'export var _save_location := ""' in script,
            'Phone free/default-location declaration changed')
    audio = node(phone, bindings['audio']['node'])
    position = instance['position']
    ring_events = presentation.events(clips['ring'], bindings)
    resource = dict(id=bindings['sprite']['resource_id'], role=bindings['sprite']['role'],
        source=texture, path=bindings['sprite']['path'], kind=bindings['sprite']['kind'],
        width=width, height=height, columns=columns, rows=rows)
    obj = dict(identity='house_phone', source_path='Objects/Phone', type='InteractDialog', position=position,
        sprite=dict(resource=resource['role'], offset=main['position'], center=transformed(position, main['position']),
                    frame_size=[width // columns, height // rows], initial_frame=main.get('frame', 0), centered=True, y_sort_origin=position,
                    shadow=False, direction_count=0, talk_animation=False),
        interaction=dict(center=transformed(position, interact['position']),
            source_offset=interact['position'], source_extents=shape(phone, 3), source_scale=interact['scale'],
            effective_extents=[f32(f32(x) * f32(s)) for x, s in zip(shape(phone, 3), interact['scale'])],
            player_turn={'x': True, 'y': True}, collision_layer=1, collision_mask=1,
            creates_actor=False, turns_phone=False, sets_talker=False, marks_npc_seen=False),
        collider=dict(source_path='Objects/Phone/StaticBody2D', center=transformed(position, collision['position']),
                      extents=shape(phone, 2), **solid),
        dispatch=dict(default=instance['dialog'], policy='last_matching_nonempty_flag',
                      overrides=[dict(flag=f, dialogue=d) for f, d in instance['_all_dialog']]),
        use=dict(is_payphone=False, save_location='', key_item=base['_key_item'],
                 searches_for_phone_card=True, consumes_phone_card=False, consumes_cash=False,
                 ordered_actions=[dict(kind='SetAudioStream', resource=hangup_sound),
                     dict(kind='PlayAnimation', clip=idle_name), dict(kind='SetPhoneLocation', value=''),
                     dict(kind='OpenDialogue', dispatch='last_matching_nonempty_flag'), dict(kind='StartAudio')]),
        audio=dict(source_path='Objects/Phone/AudioStreamPlayer2D', bus=audio['bus'],
                   center=transformed(position, audio['position']), positional=True,
                   ring=ring_sound, hangup=hangup_sound),
        ring=dict(binding=bindings['ring_binding']['flag'], method=bindings['callbacks']['ring']['method'], idempotent_when_current=ring_name,
                  stream=ring_sound, clip=ring_name, automatic_on_load=False),
        clips=[dict(name=idle_name, length=idle['length'], loop=idle['loop'], events=presentation.events(clips['idle'], bindings)),
               dict(name=ring_name, length=ring['length'], loop=ring['loop'], events=ring_events,
                    simultaneous_tick_order='Source track order: frame track0 before audio track1')])
    area_scene = ex.text('Nodes/Reusables/CutsceneArea.tscn')
    offset = node(area_scene, 'CollisionShape2D')['position']
    extents = shape(area_scene, 1)
    require(offset == [4, 4] and extents == [4, 4], 'Inherited phone-area offset changed')
    triggers = []
    for name, center in [('Cutscene Area4', [136, 816]), ('Cutscene Area3', [136, 824])]:
        area = node(house, name)
        actual_center = transformed(area['position'], offset, area['scale'])
        actual_extents = [abs(area['scale'][i]) * extents[i] for i in range(2)]
        require(actual_center == center and actual_extents == [32, 8], 'Phone area effective shape changed')
        triggers.append(dict(source_path=name, root_position=area['position'], scale=area['scale'],
            inherited_offset=offset, center=actual_center, extents=actual_extents, dialogue=area['dialog'],
            conditions=[dict(flag=area['appear_flag'], value=True), dict(flag=area['disappear_flag'], value=False)],
            activation='player_contact_then_source_idle_process', disposition='ExecuteProgram'))
    carol = node(house, 'Objects/npc')
    root_name = one(r'^\[node name="([^"]+)" type="Node2D"\]', house, 'house root')[1].replace("\\'", "'")
    carol_rows = [dict(flag='', dialogue=carol['dialog'])] + [dict(flag=f, dialogue=d) for f, d in carol['_all_dialog']]
    for row in carol_rows:
        row['seen_key'] = '/root/' + root_name + '/Objects/npc:' + row['flag'] + ':1:' + row['dialogue']
        row['staged_here'] = row['dialogue'] in ['Podunk/carol_call', 'Podunk/carol_phone']
    return dict(schema=1, kind='encore.phone-stage.presentation-ir', commit=PIN, sources=ex.sources,
        shipping_integrated=False, resources=[resource], objects=[obj], story_triggers=triggers,
        carol_dispatch=dict(source_path='Objects/npc', policy='last_matching_ordered_row',
                            marks_selected_seen=True, rows=carol_rows),
        entrance_guard=dict(source_path='DoorBlock', disappear_flag=node(house, 'DoorBlock')['disappear_flag']),
        dormant_animations=dict(source=HOUSE, animation='PhoneRing', policy='Not an automatic trigger'),
        licence_review='Original asset use only for this related Mother: Encore port; retain upstream LICENSE')


def recipe_from_ir(ir):
    return dict(schema=1, commit=PIN, sources=ir['sources'], licence_review=ir['licence_review'],
                resources=ir['resources'], format='rgba8', compression='none')


def validate_recipe(root, recipe):
    require(recipe == recipe_from_ir(build(root)), 'Changed/unreviewed phone asset recipe')


def extract(root=ROOT):
    root = Path(root)
    ir = build(root)
    write_json(root / IR.relative_to(ROOT), ir)
    write_json(root / RECIPE.relative_to(ROOT), recipe_from_ir(ir))
    write_json(root / REVIEW.relative_to(ROOT), dict(schema=1, commit=PIN, whole_handler_approved=False,
        sources=ir['sources'], ir_sha256=sha(root / IR.relative_to(ROOT)),
        scope='Original house Phone InteractDialog geometry, four-frame atlas, idle/ring tracks, ordered dispatch and Carol areas',
        semantics=['House root override replaces reusable root -4Y; no additive root offset',
            'Exact inherited Area shape offsets produce centers 136,816 and 136,824',
            'Free phone card lookup does not require or consume a card/cash',
            'Phone interaction stops Ring via Idle, dispatches, then starts hangup audio',
            'Ring idempotence is current-animation based, not phone_ring flag based',
            'Phone is never an Actor, NPC, talker, directional character, or seen-dialogue entity'],
        unverified=['Integrated runtime', 'Audio audibility', '3DS rendering', 'GUI gameplay']))
    return ir


def compile_assets(root, tex3ds, out=OUT):
    root, tex3ds, out = Path(root), Path(tex3ds), Path(out)
    ir = extract(root)
    recipe = recipe_from_ir(ir)
    out.mkdir(parents=True, exist_ok=True)
    resource = ir['resources'][0]
    filename = Path(resource['path']).name
    source = safe_path(root / 'upstream/MOTHER-Encore', resource['source'])
    with Image.open(source) as image:
        require(image.format == 'PNG' and image.size == (resource['width'], resource['height']), 'Invalid original phone PNG')
    subprocess.run([str(tex3ds), '-f', 'rgba8', '-z', 'none', '-o', str(out / filename), str(source)], check=True)
    write_json(receipt_path(out, ROOT), dict(schema=1, recipe=recipe, tex3ds_sha256=sha(tex3ds),
        outputs={filename: dict(bytes=(out / filename).stat().st_size, sha256=sha(out / filename))}))


def verify(root=ROOT, out=OUT):
    root, out = Path(root), Path(out)
    ir = build(root)
    require(json.loads((root / IR.relative_to(ROOT)).read_text()) == ir, 'Stale phone presentation')
    recipe = json.loads((root / RECIPE.relative_to(ROOT)).read_text())
    validate_recipe(root, recipe)
    review = json.loads((root / REVIEW.relative_to(ROOT)).read_text())
    require(review['schema'] == 1 and review['commit'] == PIN and review['whole_handler_approved'] is False
            and review['sources'] == ir['sources'] and review['ir_sha256'] == sha(root / IR.relative_to(ROOT)),
            'Unreviewed phone presentation receipt')
    receipt = json.loads((receipt_path(out, ROOT)).read_text())
    require(set(receipt) == {'schema', 'recipe', 'tex3ds_sha256', 'outputs'} and receipt['schema'] == 1
            and receipt['recipe'] == recipe and set(receipt['outputs']) == {Path(r['path']).name for r in ir['resources']}
            and {p.name for p in out.iterdir()} == set(receipt['outputs']) |receipt_entries(out, ROOT), 'Stale/missing/unexpected phone assets')
    for name, record in receipt['outputs'].items():
        path = safe_path(out, name)
        require(set(record) == {'bytes', 'sha256'} and sha(path) == record['sha256']
                and path.stat().st_size == record['bytes'], 'Changed phone asset: ' + name)


def record_native_animation(root, godot):
    """Native Godot AnimationPlayer oracle with original isolated Animation resources.

    Recording setter nodes replace graphics/audio devices; no phone or dialogue
    method is reimplemented by this probe. It establishes property-track timing.
    """
    root=Path(root);ex=Extractor(root);source=ex.text(SCENE)
    with tempfile.TemporaryDirectory(prefix='encore-phone-animation-')as td:
        work=Path(td)
        (work/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Phone original AnimationPlayer oracle"\n[logging]\nfile_logging/enable_logging=false\n')
        for identity,name in [(103,'Idle'),(104,'Ring')]:
            block=one(r'^\[sub_resource type="Animation" id='+str(identity)+r'\]\n(.*?)(?=^\[|\Z)',source,
                      'original phone '+name,re.M|re.S)[1]
            (work/(name+'.tres')).write_text('[gd_resource type="Animation" format=2]\n\n[resource]\n'+block)
        (work/'recorder.gd').write_text('''extends Node
var events=[]
var frame=0 setget set_frame
var playing=false setget set_playing
func set_frame(value):
    frame=value
    events.append(["frame",value])
func set_playing(value):
    playing=value
    events.append(["sound",value])
''')
        (work/'probe.gd').write_text('''extends SceneTree
func precise(value):
    if typeof(value)==TYPE_REAL:
        var bits=StreamPeerBuffer.new()
        bits.put_double(value)
        return {"native_f64_decimal":"%.17f"%value,"native_f64_le_hex":bits.data_array.hex_encode()}
    if value is Array:
        var out=[]
        for x in value:out.append(precise(x))
        return out
    if value is Dictionary:
        var out={}
        for k in value:out[k]=precise(value[k])
        return out
    return value
func _init():
    var scene=Node.new()
    scene.name="Phone"
    get_root().add_child(scene)
    var frame=load("res://recorder.gd").new()
    frame.name="main"
    scene.add_child(frame)
    var sound=load("res://recorder.gd").new()
    sound.name="AudioStreamPlayer2D"
    scene.add_child(sound)
    var player=AnimationPlayer.new()
    scene.add_child(player)
    player.add_animation("Idle",load("res://Idle.tres"))
    player.add_animation("Ring",load("res://Ring.tres"))
    player.set_process(false)
    var bits=StreamPeerBuffer.new()
    bits.put_float(1.0/60.0)
    bits.seek(0)
    var delta=bits.get_float()
    var records=[]
    var clips={}
    for name in ["Idle","Ring"]:
        var animation=player.get_animation(name)
        var tracks=[]
        for track in animation.get_track_count():
            var keys=[]
            for index in animation.track_get_key_count(track):
                keys.append([animation.track_get_key_time(track,index),animation.track_get_key_value(track,index)])
            tracks.append({"path":str(animation.track_get_path(track)),"keys":keys})
        clips[name]={"length":animation.length,"loop":animation.loop,"tracks":tracks}
    for tick in range(360):
        var action=0
        if tick==0 or tick==12 or tick==180:
            action=1
            if player.current_animation!="Ring":player.play("Ring")
        elif tick==155:
            action=2
            player.play("Idle")
        var before=frame.frame
        frame.events=[]
        sound.events=[]
        player.advance(delta)
        records.append({"tick":tick,"action":action,"before":before,"frame":frame.frame,
            "sound_count":sound.events.size(),"ringing":player.current_animation=="Ring"})
    var file=File.new()
    file.open("res://result.json",File.WRITE)
    file.store_string(JSON.print(precise({"godot":Engine.get_version_info(),"delta":delta,"clips":clips,"records":records})))
    file.close()
    scene.free()
    quit()
''')
        env=dict(os.environ,XDG_DATA_HOME=str(work/'data'),XDG_CONFIG_HOME=str(work/'config'),XDG_CACHE_HOME=str(work/'cache'))
        result=subprocess.run([str(godot),'--path',str(work),'--script','probe.gd'],capture_output=True,text=True,timeout=60,env=env)
        require(result.returncode==0 and 'ERROR:'not in result.stdout+result.stderr,'Phone native AnimationPlayer failed: '+result.stdout+result.stderr)
        data=json.loads((work/'result.json').read_text())
        data.update(schema=1,commit=PIN,sources=ex.sources,log=result.stdout+result.stderr,
                    scope='Original Idle/Ring Animation resources and native AnimationPlayer.advance; setter recorders replace sprite/audio devices')
    # The binary oracle stores exact integer observations and the source f32 idle delta.
    write_json(root/'content/phone-stage/native-animation.json',data)
    blob=bytearray(struct.pack('<8sId',b'PNREF001',len(data['records']),f32(1/60)))
    for row in data['records']:
        blob.extend(struct.pack('<6I',row['tick'],row['action'],row['before'],row['frame'],row['sound_count'],int(row['ringing'])))
    (root/'content/phone-stage/native-animation.bin').write_bytes(blob)
    write_json(root/'compatibility/reviews/phone-animation.json',dict(schema=1,commit=PIN,whole_handler_approved=False,
        sources=data['sources'],native_receipt_sha256=sha(root/'content/phone-stage/native-animation.json'),
        binary_fixture_sha256=sha(root/'content/phone-stage/native-animation.bin'),scope=data['scope']))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('action', choices=['extract', 'compile', 'verify', 'record-native'])
    ap.add_argument('--root', type=Path, default=ROOT)
    ap.add_argument('--tex3ds', type=Path)
    ap.add_argument('--godot', type=Path)
    args = ap.parse_args()
    try:
        if args.action == 'record-native':
            require(args.godot is not None, 'Native phone reference requires official Godot')
            record_native_animation(args.root,args.godot)
        elif args.action == 'extract':
            extract(args.root)
        elif args.action == 'compile':
            require(args.tex3ds is not None, 'Phone compilation requires official tex3ds')
            compile_assets(args.root, args.tex3ds)
        else:
            verify(args.root)
        print('Phone assets ' + args.action + ' complete')
        return 0
    except (ValueError, OSError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print('PHONE ASSET ERROR:', error, file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
