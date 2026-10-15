#!/usr/bin/env python3
"""Generate a fixed segment-motion fixture using the checked official Godot engine.

This is an explicit physics fixture, not a run of Mother: Encore or a trace
captured from the native implementation. Existing convex expectations stay intact.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ENGINE_SHA256 = 'e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd'
ENGINE_COMMIT = '3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'
FIXTURE = ROOT / 'tests/fixtures/segment_v0362.hpp'
PROJECT = 'config_version=4\n[application]\nconfig/name="Segment reference"\n[physics]\ncommon/physics_fps=60\n[logging]\nfile_logging/enable_logging=false\n'
PROBE = r'''extends SceneTree
class Driver:
 extends Node
 var runner
 func _physics_process(delta): runner.tick(delta)
var actor
var wall
var step=0
var rows=[]
func _init():
 assert(Engine.get_version_info().hash=="3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8")
 call_deferred("start")
func start():
 wall=StaticBody2D.new()
 var shape=ConcavePolygonShape2D.new()
 shape.segments=PoolVector2Array([Vector2(-32,0),Vector2(32,0)])
 var node=CollisionShape2D.new()
 node.shape=shape
 wall.add_child(node)
 get_root().add_child(wall)
 actor=KinematicBody2D.new()
 var hull=ConvexPolygonShape2D.new()
 hull.points=PoolVector2Array([Vector2(-7,3),Vector2(6,3),Vector2(6,9),Vector2(-7,9)])
 var collision=CollisionShape2D.new()
 collision.shape=hull
 actor.add_child(collision)
 actor.set_safe_margin(0.08)
 actor.position=Vector2(0,-20)
 get_root().add_child(actor)
 var driver=Driver.new()
 driver.runner=self
 get_root().add_child(driver)
func tick(delta):
 assert(abs(delta-1.0/60.0)<0.00000001)
 if step==0:
  step+=1
  return
 var velocity=Vector2(0,60) if step<=60 or step>70 else Vector2(60,60)
 var result=actor.move_and_slide(velocity)
 rows.append({"position":[actor.position.x,actor.position.y],"velocity":[result.x,result.y]})
 if step==60 or step==70:
  print("SEGMENT ",step," ",actor.position)
 if step==70:
  wall.queue_free()
 if step==100:
  var file=File.new()
  assert(file.open("res://reference.json",File.WRITE)==OK)
  file.store_string(JSON.print({"rows":rows}))
  file.close()
  quit(0)
 step+=1
'''

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def validate_rows(document):
    if type(document) is not dict or set(document) != {'rows'}:
        raise ValueError('Unknown segment reference envelope')
    rows = document['rows']
    if type(rows) is not list or len(rows) != 100:
        raise ValueError('Segment reference requires exactly 100 frames')
    for row in rows:
        if type(row) is not dict or set(row) != {'position', 'velocity'}:
            raise ValueError('Unknown segment frame fields')
        for vector in row.values():
            if type(vector) is not list or len(vector) != 2 or any(
                type(v) not in (int, float) or not math.isfinite(v) or abs(v) > 1000000
                for v in vector):
                raise ValueError('Invalid segment reference vector')
    return rows

def emit_header(rows):
    def number(value):
        text = format(float(value), '.9g')
        return text + ('f' if '.' in text or 'e' in text else '.0f')
    lines = ['// Generated ONLY from official Godot 3.6.2 by tools/run_segment_reference.py.',
             '// Explicit convex actor / concave segment fixture, not game content.',
             '#pragma once', '#include <array>', '#include "encore/collision.hpp"',
             'namespace segment_reference {',
             'struct Frame { encore::upstream::Vec2 position, velocity; };',
             'inline const std::array<Frame,100> frames{{']
    for row in rows:
        vectors = ['{' + ','.join(number(v) for v in row[k]) + '}' for k in ('position', 'velocity')]
        lines.append('    {' + ','.join(vectors) + '},')
    return '\n'.join(lines + ['}};', '}']) + '\n'

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--godot', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--write-fixture', action='store_true')
    args = p.parse_args()
    engine = args.godot.resolve()
    if sha(engine) != ENGINE_SHA256:
        raise ValueError('Unreviewed Godot executable; only checked official Windows 3.6.2 is admitted')
    out = args.out.resolve()
    if not out.is_relative_to(ROOT/'build') or out == ROOT/'build':
        raise ValueError('Reference output must remain inside owned build directory')
    if out.exists() and any(out.iterdir()):
        raise ValueError('Use a fresh reference directory; retain earlier evidence')
    out.mkdir(parents=True, exist_ok=True)
    (out/'project.godot').write_text(PROJECT, encoding='utf8')
    (out/'probe.gd').write_text(PROBE, encoding='utf8')
    env = dict(os.environ, APPDATA=str(out/'userdata'), XDG_DATA_HOME=str(out/'userdata'))
    result = subprocess.run([str(engine), '--no-window', '--path', str(out), '-s', 'probe.gd'],
                            capture_output=True, text=True, env=env, timeout=20)
    log = result.stdout + result.stderr
    (out/'engine.log').write_text(log, encoding='utf8')
    if result.returncode or 'SCRIPT ERROR' in log or len(log.encode()) > 1024*1024:
        raise ValueError('Original engine fixture failed; keep diagnostics')
    rows = validate_rows(json.loads((out/'reference.json').read_text(encoding='utf8')))
    header = emit_header(rows)
    if args.write_fixture: FIXTURE.write_text(header, encoding='utf8')
    (out/'receipt.json').write_text(json.dumps(dict(status='passed', engine_commit=ENGINE_COMMIT,
        engine_sha256=sha(engine), tool_sha256=sha(Path(__file__)), probe_sha256=sha(out/'probe.gd'),
        reference_sha256=sha(out/'reference.json'), fixture_sha256=hashlib.sha256(header.encode()).hexdigest(),
        frames=len(rows), scope='Official engine static-convex actor and concave wall motion only; no game/GUI/hardware claim'), indent=2)+'\n', encoding='utf8')
    print('Official Godot segment reference: 100 frames generated; native output was not used')

if __name__ == '__main__': main()
