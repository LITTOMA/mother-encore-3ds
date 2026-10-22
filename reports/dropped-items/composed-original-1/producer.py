"""Unchanged seven original coffins composed with actual dropped-item scripts.

The observer supplies a hit and a later interaction, party holders, player pose,
voice/rumble recorders and input labels. Terrain, automatic hit detection, PCM,
dialogue playback and the complete room are not admitted by this CPU oracle.
"""
import argparse
import hashlib
import json
import posixpath
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools import run_dropped_item_reference as item
from tools import run_breakable_reference as barrel
from tools import dropped_items as d

ROOT = d.ROOT
PROBE = item.PROBE[:item.PROBE.index('func _ready():')] + '''func sample(frame,cover,object):
 var row=capture(frame,object)
 var anim=cover.get_node("AnimationPlayer")
 row.cover={"parent":str(global.currentScene.get_path_to(cover.get_parent())),"position":point(cover.position),"visible":cover.visible,"sprite_visible":cover.get_node("Sprite").visible,"frame_index":cover.get_node("Sprite").frame,"animation":anim.assigned_animation,"playing":anim.is_playing(),"time":bits(anim.current_animation_position) if anim.assigned_animation!="" else null,"body_disabled":cover.get_node("StaticBody2D/CollisionShape2D").disabled,"body_layer":cover.get_node("StaticBody2D").collision_layer,"hit_layer":cover.get_node("Hurtbox").collision_layer,"hit_mask":cover.get_node("Hurtbox").collision_mask,"timer_left":bits(cover.get_node("Timer").time_left),"timer_stopped":cover.get_node("Timer").is_stopped(),"audio":cover.get_node("AudioStreamPlayer").requests.duplicate(true),"rumble":global.rumble.duplicate(true)}
 if is_instance_valid(object):
  row.state.parent=str(global.currentScene.get_path_to(object.get_parent()))
  row.state.physics=object.is_physics_processing()
  row.state.body_disabled=object.get_node("StaticBody2D/CollisionShape2D").disabled
  row.state.area_disabled=object.get_node("interact/CollisionShape2D").disabled
 return row
func _ready():call_deferred("run")
func run():
 var file=File.new()
 assert(file.open("res://input.json",File.READ)==OK)
 config=JSON.parse(file.get_as_text()).result
 file.close()
 for test in config.cases:
  Item.used_uids_tab.clear()
  global.item=null
  global.party=[]
  global.rumble=[]
  globaldata.flags={}
  globaldata.object_flags={}
  globaldata.button_prompts=test.prompts
  globaldata.key_items=Inventory.new(Inventory.InvType.KEY)
  uiManager.calls=[]
  global.get_player().pause_state=false
  global.get_player().position=Vector2(13,27)
  for count in test.fill:
   var member=preload("res://character.gd").new()
   member.inv=Inventory.new()
   for i in int(count):member.inv.add_item(Item.new(config.filler,false,1,1000+global.party.size()*100+i))
   global.party.append(member)
  var binding=config.bindings[int(test.binding)]
  var owner=config.covers[int(test.cover)]
  var scene=preload("res://scene.gd").new()
  scene.name=config.scene_name
  global.currentScene=scene
  for name in config.parents:
   var parent=Node2D.new()
   parent.name=name
   scene.add_child(parent)
  if test.flag:globaldata.object_flags[binding.flag]=true
  var object=load("res://"+config.prefab).instance()
  object.name=binding.name
  object.position=Vector2(binding.position[0],binding.position[1])
  object.item=config.templates[str(int(binding.template))].source
  var cover=load("res://"+config.cover_prefab).instance()
  cover.name=owner.path.get_file()
  cover.position=Vector2(owner.position[0],owner.position[1])
  for key in ["appear","disappear","flag"]:
   cover.set(key+"_flag" if key in ["appear","disappear"] else key,owner[key])
  cover.is_object_flag=owner.flag_kind=="object"
  cover.emit_flag_updated_signal=owner.emit_flag
  cover.reset_when_leaving_area=owner.reset_area
  cover.reset_when_leaving_region=owner.reset_region
  cover.Drop_item_if_hidden=owner.Drop_item_if_hidden
  cover.interact_content=owner.interact_content
  cover.pause_on_break=owner.pause_on_break
  cover.coins_spawned=owner.coins
  cover.contents=NodePath(owner.content_relative)
  cover.Drop_where=NodePath(owner.drop_literal)
  cover.New_parent=NodePath(owner.new_parent_relative)
  var old_voice=cover.get_node("AudioStreamPlayer")
  cover.remove_child(old_voice)
  old_voice.free()
  var voice=load("res://voice.gd").new()
  voice.name="AudioStreamPlayer"
  cover.add_child(voice)
  # Preserve actual sibling order: the coffin is earlier than its item.
  scene.get_node(owner.parent).add_child(cover)
  seed(int(config.seed))
  scene.get_node(binding.parent).add_child(object)
  watched_prompt=object.get_node("interact/ButtonPrompt")
  add_child(scene)
  yield(get_tree(),"idle_frame")
  base_idle=Engine.get_idle_frames()
  base_physics=Engine.get_physics_frames()
  global.base=base_idle
  voice.base=base_idle
  var rows=[sample(-1,cover,object)]
  var tail=randi()
  var emitter=Node2D.new()
  scene.add_child(emitter)
  emitter.global_position=cover.global_position+Vector2(test.distance,0)
  for frame in range(int(test.frames)+1):
   if frame==0:cover._on_Hurtbox_area_entered(emitter)
   if frame==int(test.action) and is_instance_valid(object):object.interact()
   if frame==int(test.repeat) and is_instance_valid(object):object.interact()
   rows.append(sample(frame,cover,object))
   yield(get_tree(),"idle_frame")
  results.append({"case":test.name,"sparkle_rng_tail":tail,"rows":rows})
  allocated.append({"case":test.name,"uids":Item.used_uids_tab.duplicate()})
  scene.free()
  global.currentScene=null
  global.party=[]
  global.item=null
  yield(get_tree(),"idle_frame")
 assert(file.open("res://../reference.json",File.WRITE)==OK)
 file.store_string(JSON.print({"schema":3,"godot":Engine.get_version_info(),"cases":results,"entropy_allocations":allocated}))
 file.close()
 get_tree().quit()
'''


def source():
    files, copied, proof = item.source()
    extra, original, cover_proof = barrel.source()
    recipe, breaks, _ = d.b.source()
    config = json.loads(files['input.json'])
    paths = {b['path'] for b in config['bindings']}
    covers = [o for o in breaks['owners'] if o['content'] in paths]
    d.require(len(covers) == 7 and len({o['content'] for o in covers}) == 7,
              'Dropped composition complete original owner coverage')
    d.require(all(not o['spawner'] and not o['interact_content'] and o['drop_parent'] == 'Objects'
                  and o['new_parent'] == 'Bottom2' for o in covers),
              'Dropped composition unreviewed original drop/reparent/interaction')
    for owner in covers:
        owner['content_relative'] = posixpath.relpath(owner['content'], owner['path'])
        owner['new_parent_relative'] = posixpath.relpath(owner['new_parent'], owner['path'])
    # Relative paths are source adapter output, not computed by script stubs.
    probe = PROBE
    for name, raw in original.items():
        d.require(name not in copied or copied[name] == raw, 'Dropped composition conflicting original dependency')
        copied[name] = raw
    for name in ['voice.gd', 'factory.gd']:
        files[name] = extra[name]
    coin_prefab = recipe['paths']['coin_spawner']
    files[coin_prefab] = extra[coin_prefab]
    project = files['project.godot'].splitlines()
    line = next(i for i, value in enumerate(project) if value.startswith('_global_script_classes='))
    classes = json.loads(project[line].split('=', 1)[1])
    classes.append(dict(base='Node2D', **{'class': 'CoinSpawner'}, language='GDScript', path='res://factory.gd'))
    project[line] = '_global_script_classes=' + json.dumps(classes)
    files['project.godot'] = '\n'.join(project) + '\n'
    files['global.gd'] += '\nvar rumble=[]\nvar base=0\nfunc start_joy_vibration(device,weak,strong,duration):rumble.append([Engine.get_idle_frames()-base,device,weak,strong,duration])\n'
    files['player.gd'] += '\nfunc pause():set_pause(true)\n'
    cases = [dict(name=o['path'].replace('/', '-'), cover=i,
                  binding=next(j for j, b in enumerate(config['bindings']) if b['path'] == o['content']))
             for i, o in enumerate(covers)]
    cases += [dict(cases[0], name='full', fill=[16], repeat=52),
              dict(cases[0], name='second-holder', fill=[16, 0]),
              dict(cases[0], name='already-collected', flag=True),
              dict(cases[0], name='distant-hit', distance=100)]
    defaults = dict(fill=[0], prompts='Both', flag=False, frames=95, action=40,
                    repeat=-100, distance=0)
    config.update(cases=[dict(defaults, **case) for case in cases], covers=covers,
                  cover_prefab=next(v['source'] for v in breaks['families'] if v['id'] == covers[0]['family']),
                  parents=[p['path'] for p in breaks['parents']])
    files['input.json'] = json.dumps(config)
    files['probe.gd'] = probe
    for name, text in list(files.items()):
        if name.endswith('.gd'):
            files[name] = '\n'.join('\t' * (len(line) - len(line.lstrip(' '))) + line.lstrip(' ')
                                    for line in text.split('\n'))
    proof.update(composed_breakables=cover_proof, scope=__doc__,
                 cover_adapter_sha256=hashlib.sha256(Path(barrel.__file__).read_bytes()).hexdigest(),
                 source_adapter_sha256=hashlib.sha256(Path(item.__file__).read_bytes()).hexdigest(),
                 copied_sha256={k: hashlib.sha256(v).hexdigest() for k, v in copied.items()},
                 generated_sha256={k: hashlib.sha256(v.encode()).hexdigest() for k, v in files.items()})
    return files, copied, proof


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--godot', type=Path, required=True)
    args = parser.parse_args()
    from tools.dropped_breakable_protocol import validate, semantic_digest
    item.run_reference(args.work, args.godot, *source(), Path(__file__),
                       ROOT / 'tools/dropped_breakable_protocol.py', validate, semantic_digest)


if __name__ == '__main__':
    main()
