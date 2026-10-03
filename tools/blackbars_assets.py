#!/usr/bin/env python3
"""Compile the pinned source dialogue CanvasLayer bars, without new textures."""
import argparse
import json
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.extract_battle_entry import Extractor, animation, node, one, require

IR = ROOT / 'content/native-blackbars.json'
PACK = Path('data/opening.encbars')
SCENE = 'Nodes/Ui/Blackbars.tscn'


def extract():
    ex = Extractor(ROOT)
    scene = ex.text(SCENE)
    script = ex.text('Scripts/UI/Blackbars.gd')
    manager = ex.text('Scripts/global/uiManager.gd')
    dialogue = ex.text('Scripts/UI/DialogueBox.gd')
    project = ex.text('project.godot')
    require('_anim_player.play("Open")' in script and '_anim_player.play("Close")' in script
            and 'if open and !_is_open:' in script and 'elif !open and _is_open:' in script,
            'Unreviewed blackbar state transitions')
    require('_add_to_canvas(_black_bars, 1)' in manager
            and 'toggle_black_bars(true)\n\t_dialogue_box.start_from_id' in manager
            and '_dialogue_box = null\n\ttoggle_black_bars(false)' in manager,
            'Unreviewed blackbar dialogue ownership/layer')
    require('emit_signal("done", _dialog_response)' in dialogue,
            'Unreviewed dialogue completion')
    canvas = [int(one(r'^window/size/' + key + r'=(\d+)$', project, key)[1])
              for key in ('width', 'height')]
    bars = []
    for name in ('ColorRect', 'ColorRect2'):
        p = node(scene, name)
        require(p['color'] == [0, 0, 0, 1] and p.get('margin_left', 0) == 0
                and p['margin_right'] == canvas[0], 'Unreviewed blackbar rectangle')
        bars.append(dict(height=p.get('margin_bottom', 0) - p['margin_top'],
                         reset_y=p['margin_top']))
    tracks = []
    for ident, name in ((2, 'Open'), (1, 'Close')):
        clip = animation(scene, ident, SCENE, name)
        require(not clip['loop'] and len(clip['tracks']) == 2, 'Unreviewed blackbar clip')
        for bar, track in enumerate(clip['tracks']):
            keys = track['keys']
            require(track['path'] == ('ColorRect' if bar == 0 else 'ColorRect2') + ':rect_position'
                    and track['type'] == 'value' and track['interp'] == 2
                    and track['enabled'] and track['loop_wrap'] and keys['update'] == 0
                    and len(keys['times']) == len(keys['values']) == len(keys['transitions']) == 2
                    and keys['times'][0] == 0 and all(v[0] == 0 for v in keys['values']),
                    'Unreviewed blackbar animation track')
            tracks.append(dict(duration=clip['length'], end_time=keys['times'][1],
                               from_y=keys['values'][0][1], to_y=keys['values'][1][1],
                               ease=keys['transitions'][0]))
    return dict(schema=1, commit=ex.lock['commit'], sources=ex.sources, canvas=canvas,
                bars=bars, color=0xff000000, tracks=tracks,
                scope='Original dialogue-owned layer-1 Blackbars, before dialogue UI. Source 320x180 is exact; native 400x240 stretches width and bottom-anchors the original 28-pixel lower bar. No world mask, collision, movement, camera or actor visibility changes.')


def encode(r):
    require(r['schema'] == 1 and len(r['canvas']) == len(r['bars']) == 2
            and len(r['tracks']) == 4, 'Blackbar IR schema')
    payload = bytearray(struct.pack('<4fI', *r['canvas'], *(b['height'] for b in r['bars']), r['color']))
    for t in r['tracks']:
        payload.extend(struct.pack('<5f', *(t[k] for k in ('duration', 'end_time', 'from_y', 'to_y', 'ease'))))
    return struct.pack('<8s4I', b'ENCBAR01', 1, len(payload)+24, zlib.crc32(payload), 1) + payload


def checked():
    r = extract()
    require(json.loads(IR.read_text()) == r, 'Stale blackbar source IR')
    return encode(r)


def stage_files(root):
    blob = checked()
    require((root / PACK).read_bytes() == blob, 'Stale blackbar pack')
    return {PACK: blob}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=['extract', 'compile', 'verify'])
    args = parser.parse_args()
    if args.action == 'extract':
        IR.write_text(json.dumps(extract(), indent=2) + '\n')
    elif args.action == 'compile':
        (ROOT / 'romfs' / PACK).write_bytes(checked())
    else:
        stage_files(ROOT / 'romfs')
    print('Blackbar ' + args.action + ': source-owned animation and layer verified')


if __name__ == '__main__':
    main()
