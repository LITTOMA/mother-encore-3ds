#!/usr/bin/env python3
"""Headless original Actor.update_npcs signal reference for post-win NPCs.

Runs unchanged source functions with isolated scene services. A pending source
frame_changed wait is measured and retained; no timeout completes restoration.
"""
import argparse, json, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.reference_doll_actor_actions import prepare
from tools.reference_movement import extract
from tools.run_scene_reference import invoke,digest
from tools.upstream import write_json
from tools.doll_postwin import SOURCE,PIN

def main():
 p=argparse.ArgumentParser();p.add_argument('--godot',type=Path,required=True);p.add_argument('--work',type=Path,required=True);p.add_argument('--reports',type=Path,required=True);a=p.parse_args()
 work=a.work.resolve();reports=a.reports.resolve();source=ROOT/'upstream/MOTHER-Encore'
 if work.exists()or reports.exists()or not work.is_relative_to(ROOT/'build')or not reports.is_relative_to(ROOT/'reports'):raise ValueError('Fresh build/report paths required')
 meta=prepare(source,work);reports.mkdir(parents=True)
 actor=source/'Scripts/Main/actor.gd';functions,hashes=extract(actor.read_bytes(),digest(actor),['update_npcs','unmake_persistent'])
 text=(work/'actor.gd').read_text().replace('func _replaced_exists(): return false','func _replaced_exists(): return _replaced != null and is_instance_valid(_replaced)')
 (work/'actor.gd').write_text(text+'\nvar is_party_object = false\n'+functions)
 (work/'global.gd').write_text('extends Node\nvar talker = null\nfunc get_player(): return null\nfunc remove_persistent(_object): pass\n')
 (work/'ui.gd').write_text('extends Node\nfunc is_in_battle(): return false\n')
 project=(work/'project.godot').read_text().replace('[physics]','uiManager="*res://ui.gd"\n[physics]');(work/'project.godot').write_text(project)
 (work/'replacement.gd').write_text('''extends Node2D
var character_sprite
var idle_animation = "Idle"
var start_pos = Vector2.ZERO
var new_pos = Vector2.ZERO
var direction = Vector2.ZERO
var animation_name = "4dir"
func _ready():
    character_sprite=load("res://character_sprite.gd").new()
    var player=AnimationPlayer.new()
    player.name="AnimationPlayer"
    character_sprite.add_child(player)
    add_child(character_sprite)
    character_sprite.set_animation(animation_name, [["Talk","Idle",2]] if animation_name == "4dir" else [])
func set_direction(value):
    direction=value
    character_sprite.blend_position(value)
''')
 (work/'probe.gd').write_bytes((ROOT/'tools/godot_exporter/doll_postwin_cleanup_probe.gd').read_bytes())
 write_json(work/'cleanup_metadata.json',dict(commit=PIN,sources=meta['sources'],additional_sources={**meta['additional_sources'],SOURCE:digest(source/SOURCE)},functions=hashes,scope='Unchanged nonplayer Actor.update_npcs; original CharacterSprite trees; Doll Floater/Minnie and Mimmie4dir replacement signal waits; minimal global/UI stubs'))
 invoke(a.godot.resolve(),work,['--fixed-fps','60','--script',str(work/'probe.gd'),'--encore-out='+str(reports/'reference.json')],reports/'godot.txt')
 data=json.loads((reports/'reference.json').read_text())
 for case in data['cases']:
  expected=True;last=case['frames'][-1]
  if last['replacement_visible']!=expected or last['proxy_exists']==expected:raise ValueError('Native cleanup result differs from source signal policy: '+case['definition']['name'])
  if last['position']!=case['definition']['position']or last['direction']!=case['definition']['final_direction']:raise ValueError('Native cleanup pose copy changed')
 write_json(reports/'receipt.json',dict(engine_sha256=digest(a.godot),reference_sha256=digest(reports/'reference.json'),tool_sha256=digest(Path(__file__)),probe_sha256=digest(ROOT/'tools/godot_exporter/doll_postwin_cleanup_probe.gd'),scope=data['scope'],result='Five cases,120 native idle samples each; immediate pose copy and source frame_changed+idle waits retained',emulator='not run',hardware='not run'))
 print('Verified original post-win replacement signal waits:',reports)
if __name__=='__main__':main()
