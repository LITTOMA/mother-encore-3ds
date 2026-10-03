#!/usr/bin/env python3
"""Bounded original Actor.update_npcs cleanup signal timing oracle."""
import argparse,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.reference_doll_actor_actions import prepare
from tools.reference_movement import extract
from tools.run_scene_reference import invoke,digest
from tools.upstream import read_json,write_json

def main():
 p=argparse.ArgumentParser();p.add_argument('--godot',type=Path,required=True);p.add_argument('--work',type=Path,required=True);p.add_argument('--reports',type=Path,required=True);a=p.parse_args();source=ROOT/'upstream/MOTHER-Encore';work=a.work.resolve();reports=a.reports.resolve()
 if reports.exists() or not reports.is_relative_to(ROOT/'reports'):raise ValueError('Fresh report path required')
 meta=prepare(source,work);reports.mkdir(parents=True)
 actor_path=source/'Scripts/Main/actor.gd';functions,hashes=extract(actor_path.read_bytes(),digest(actor_path),['update_npcs','unmake_persistent'])
 text=(work/'actor.gd').read_text().replace('func _replaced_exists(): return false','func _replaced_exists(): return _replaced != null and is_instance_valid(_replaced)')
 text+='\nvar is_party_object = false\n'+functions
 (work/'actor.gd').write_text(text)
 (work/'global.gd').write_text('extends Node\nvar talker = null\nfunc get_player(): return null\nfunc remove_persistent(_object): pass\n')
 (work/'ui.gd').write_text('extends Node\nfunc is_in_battle(): return false\n')
 project=(work/'project.godot').read_text().replace('[physics]','uiManager="*res://ui.gd"\n[physics]');(work/'project.godot').write_text(project)
 (work/'replacement.gd').write_text('''extends Node2D
var character_sprite
var idle_animation = "Idle"
var start_pos = Vector2.ZERO
var new_pos = Vector2.ZERO
var direction = Vector2.ZERO
func _ready():
    character_sprite=load("res://character_sprite.gd").new()
    var player=AnimationPlayer.new()
    player.name="AnimationPlayer"
    character_sprite.add_child(player)
    add_child(character_sprite)
    character_sprite.set_animation("4dir", [["Talk","Idle",2]])
func set_direction(value):
    direction=value
    character_sprite.blend_position(value)
''')
 (work/'probe.gd').write_bytes((ROOT/'tools/godot_exporter/doll_actor_cleanup_probe.gd').read_bytes())
 write_json(work/'cleanup_metadata.json',dict(commit=meta['commit'],sources=meta['sources'],additional_sources=meta['additional_sources'],functions=hashes,scope='Unchanged nonplayer Actor.update_npcs and original CharacterSprite; source signal waits with minimal global/UI stubs'))
 invoke(a.godot.resolve(),work,['--fixed-fps','60','--script',str(work/'probe.gd'),'--encore-out='+str(reports/'reference.json')],reports/'godot.txt')
 write_json(reports/'receipt.json',dict(engine_sha256=digest(a.godot),reference_sha256=digest(reports/'reference.json'),tool_sha256=digest(Path(__file__)),emulator='not run',hardware='not run'));print(reports)
if __name__=='__main__':main()
