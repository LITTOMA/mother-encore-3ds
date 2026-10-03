#!/usr/bin/env python3
"""Reproduce bounded exact-source movement and native Player animation references."""
from __future__ import annotations
import argparse
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.scene_reference import prepare as prepare_scene
from tools.reference_movement import prepare as prepare_movement,fixture as movement_fixture
from tools.character_animation import build
from tools.reference_animation import fixture as animation_fixture
from tools.run_scene_reference import invoke,digest
from tools.upstream import read_json,write_json

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--godot',type=Path,required=True)
    parser.add_argument('--work',type=Path,required=True)
    parser.add_argument('--reports',type=Path,required=True)
    args=parser.parse_args()
    try:
        work=args.work.resolve();reports=args.reports.resolve();engine=args.godot.resolve()
        if not work.is_relative_to(ROOT/'build') or work==ROOT/'build' or work.exists():
            raise ValueError('Use fresh work subdirectory of build/')
        if not reports.is_relative_to(ROOT/'reports') or reports.exists():
            raise ValueError('Use fresh reports directory')
        movement_review=read_json(ROOT/'compatibility/reviews/movement-v0410.json')
        animation_review=read_json(ROOT/'compatibility/reviews/ninten-animation-v0410.json')
        lock=read_json(ROOT/'upstream.lock')
        if any(r['commit']!=lock['commit'] or r['game_version']!=lock['game_version'] for r in [movement_review,animation_review]):
            raise ValueError('Actor reviews differ from actual upstream lock')
        work.mkdir(parents=True);reports.mkdir(parents=True)
        project=work/'movement'
        prepare_movement(ROOT/'upstream/MOTHER-Encore',project,movement_review)
        invoke(engine,project,['--script',str(project/'probe.gd'),'--encore-out='+str(reports/'movement.json')],reports/'movement.txt')
        (reports/'movement_v0410.hpp').write_text(movement_fixture(read_json(reports/'movement.json'),movement_review),encoding='utf-8')
        project=work/'player'
        source=prepare_scene(ROOT/'upstream/MOTHER-Encore',project,animation_review['scene'])
        invoke(engine,project,['--editor'],reports/'player-import.txt')
        if 'ENCORE_IMPORT_COMPLETE' not in (reports/'player-import.txt').read_text():raise ValueError('Import not confirmed')
        invoke(engine,project,['--script',str(project/'scene_data.gd'),'--encore-scene=res://'+animation_review['scene'],
            '--encore-out='+str(reports/'player-data.json')],reports/'player-export.txt')
        invoke(engine,project,['--script',str(ROOT/'tools/godot_exporter/animation_probe.gd'),
            '--encore-out='+str(reports/'animation.json')],reports/'animation.txt')
        profile=build(read_json(reports/'player-data.json'),animation_review,read_json(reports/'animation.json'))
        (reports/'animation_v0410.hpp').write_text(animation_fixture(read_json(reports/'animation.json')),encoding='utf-8')
        write_json(reports/'player-source.json',source)
        write_json(reports/'profile.json',profile)
        write_json(reports/'receipt.json',{
            'schema':1,'commit':lock['commit'],'game_version':lock['game_version'],'engine_sha256':digest(engine),
            'scope':'isolated movement without collisions/skills/followers and native discrete Ninten animation; not full Player/game compatibility',
            'reports_sha256':{p.name:digest(p) for p in sorted(reports.iterdir()) if p.is_file()},
            'tools_sha256':{str(p.relative_to(ROOT)):digest(p) for p in [Path(__file__).resolve(),
                ROOT/'tools/reference_movement.py',ROOT/'tools/reference_animation.py',ROOT/'tools/character_animation.py',
                ROOT/'tools/godot_exporter/movement_wrapper.gd',ROOT/'tools/godot_exporter/movement_probe.gd',
                ROOT/'tools/godot_exporter/animation_probe.gd',ROOT/'tools/scene_reference.py',ROOT/'tools/godot_exporter/scene_data.gd']},
            'hardware':'not run','emulator':'not run'})
        print('Actor references recorded. Generated comparisons require C++ tests; current app remains background viewer.')
        return 0
    except (OSError,ValueError,KeyError,TypeError) as error:
        print('ACTOR REFERENCE ERROR: '+str(error),file=sys.stderr);return 1

if __name__=='__main__':raise SystemExit(main())
