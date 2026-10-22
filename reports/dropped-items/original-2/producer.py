"""Run unchanged original DroppedItem/ItemHolder/Inventory/Item scripts.

Player, party holders, input-label text and dialogue requests are explicit
providers. UID entropy is genuine OS randomize; retain raw UIDs separately and
compare their ordered opaque identities, rather than invent deterministic UIDs.
The official disable-render-loop option isolates CPU lifecycle observations;
this run makes no claim about shader execution, pixels or GPU verification.
"""
import argparse
import hashlib
import json
import re
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools import dropped_items as d, run_breakable_reference as barrel
from tools.run_scene_reference import invoke

PROBE = '''extends Node
var config
var results=[]
var allocated=[]
var active
var watched_prompt
var base_idle=0
var base_physics=0
func bits(v):
 var stream=StreamPeerBuffer.new()
 stream.put_float(v)
 stream.seek(0)
 return stream.get_u32()
func point(v):return [bits(v.x),bits(v.y)]
func opaque(uid):
 var at=Item.used_uids_tab.find(uid)
 return "allocated:"+str(at) if at>=0 else "provided:"+str(uid)
func inventory(inv):
 var out=[]
 for item in inv.get_items():out.append({"name":item.item_name,"doses":item.doses,"equipped":item.equipped,"uid":opaque(item.uid)})
 return out
func capture(frame,object):
 var row={"frame":frame,"idle":Engine.get_idle_frames()-base_idle,"physics":Engine.get_physics_frames()-base_physics,"delta":bits(get_process_delta_time()),"alive":is_instance_valid(object),"flags":globaldata.object_flags.duplicate(true),"dialogues":uiManager.calls.duplicate(true),"ledger_size":Item.used_uids_tab.size(),"party":[],"keys":inventory(globaldata.key_items),"global_item":null,"prompt":null}
 if is_instance_valid(watched_prompt):
  var anim=watched_prompt.get_node("AnimationPlayer")
  var prompt_time=null
  if anim.assigned_animation!="":prompt_time=bits(anim.current_animation_position)
  row.prompt={"parent":str(global.currentScene.get_path_to(watched_prompt.get_parent())),"visible":watched_prompt.visible,"hidden":watched_prompt._hidden,"pressing":watched_prompt._pressing_button,"position":point(watched_prompt.position),"scale":point(watched_prompt.scale),"enabled":watched_prompt.enabled,"force_show":watched_prompt._force_show,"force_hide":watched_prompt._force_hide,"animation":anim.assigned_animation,"time":prompt_time,"queued":watched_prompt.is_queued_for_deletion()}
 for member in global.party:row.party.append(inventory(member.inv))
 if global.item!=null:row.global_item={"name":global.item.item_name,"doses":global.item.doses,"equipped":global.item.equipped,"uid":opaque(global.item.uid)}
 if is_instance_valid(object):
  var animation=object.get_node("AnimationPlayer")
  var animation_time=null
  if animation.assigned_animation!="":animation_time=bits(animation.current_animation_position)
  row.state={"position":point(object.position),"scale":point(object.scale),"visible":object.visible,"sprite_visible":object.get_node("Sprite").visible,"rotation":bits(object.get_node("Sprite").rotation_degrees),"sparkle":object.get_node("Sparkles").frame,"timer_paused":object.get_node("Timer").paused,"timer_left":bits(object.get_node("Timer").time_left),"timer_stopped":object.get_node("Timer").is_stopped(),"animation":animation.assigned_animation,"animation_speed":bits(animation.playback_speed),"animation_time":animation_time,"queued":object.is_queued_for_deletion()}
  var prompt=object.get_node_or_null("interact/ButtonPrompt")
  row.state.prompt=null
  if prompt!=null:row.state.prompt={"visible":prompt.visible,"hidden":prompt._hidden,"pressing":prompt._pressing_button,"position":point(prompt.position),"scale":point(prompt.scale)}
 return row
func _ready():call_deferred("run")
func run():
 var file=File.new()
 if file.open("res://input.json",File.READ)!=OK:
  get_tree().quit(2)
  return
 config=JSON.parse(file.get_as_text()).result
 file.close()
 for test in config.cases:
  Item.used_uids_tab.clear()
  global.item=null
  global.party=[]
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
  var scene=preload("res://scene.gd").new()
  scene.name=config.scene_name
  global.currentScene=scene
  var objects=Node2D.new()
  objects.name="Objects"
  scene.add_child(objects)
  var binding=config.bindings[int(test.binding)]
  if test.flag:globaldata.object_flags[binding.flag]=true
  var object=load("res://"+config.prefab).instance()
  object.name=binding.name
  object.position=Vector2(binding.position[0],binding.position[1])
  object.item=test.get("item",config.templates[str(int(binding.template))].source)
  object.can_pickup=test.pickup
  object.reset_when_consumed=test.reset
  object.dialog_empty=test.empty_dialogue
  seed(int(config.seed))
  objects.add_child(object)
  base_idle=Engine.get_idle_frames()
  base_physics=Engine.get_physics_frames()
  add_child(scene)
  watched_prompt=object.get_node("interact/ButtonPrompt")
  var rows=[capture(-1,object)]
  var sparkle_tail=randi()
  if test.nearby and is_instance_valid(object):global.get_player().emit_signal("event_detector_entered",object.get_node("interact"))
  for frame in range(int(test.frames)+1):
   if frame==int(test.action) and is_instance_valid(object):
    if test.mode=="collect":object.interact()
    elif test.mode=="disappear":object.disappear()
   if frame==int(test.get("repeat",-100)) and is_instance_valid(object):object.interact()
   if frame==int(test.get("pause",-100)):
    global.get_player().pause_state=true
    global.get_player().emit_signal("paused")
   if frame==int(test.get("unpause",-100)):
    global.get_player().pause_state=false
    global.get_player().emit_signal("unpaused")
   if frame==int(test.get("move_player",-100)):global.get_player().position=Vector2(99,101)
   rows.append(capture(frame,object))
   yield(get_tree(),"idle_frame")
  results.append({"case":test.name,"sparkle_rng_tail":sparkle_tail,"rows":rows})
  allocated.append({"case":test.name,"uids":Item.used_uids_tab.duplicate()})
  scene.free()
  global.currentScene=null
  global.party=[]
  global.item=null
  yield(get_tree(),"idle_frame")
 if file.open("res://../reference.json",File.WRITE)!=OK:
  get_tree().quit(3)
  return
 file.store_string(JSON.print({"schema":2,"godot":Engine.get_version_info(),"cases":results,"entropy_allocations":allocated}))
 file.close()
 get_tree().quit()
'''


def source():
    model, proof = d.derive()
    s = d.f.Sources()
    copied = {}

    def copy(path):
        if path in copied:
            return
        raw = s.data(path)
        copied[path] = raw
        if path.endswith(('.tscn', '.tres')):
            for dependency in re.findall(r'\b(?:font_)?path\s*=\s*"res://([^"\n]+)"', raw.decode()):
                copy(dependency)

    for key in ['script', 'holder', 'flag', 'inventory', 'item', 'prefab']:
        copy(model['paths'][key])
    for template in model['templates']:
        copy(template['texture'])
    # A real source key item exercises ItemHolder's key bypass independently of
    # the seven normal catacombs bindings. It does not add a catacombs key drop.
    key_data = s.extractor.yaml('Data/Items/DogTreats.yaml')
    copy('Graphics/Objects/Items/DogTreats.png')
    data = {v['source']: v['document'] for v in model['templates']}
    data['DogTreats'] = key_data
    character = s.text(model['paths']['character'])
    names = ['MAXHP', 'MAXPP', 'OFFENSE', 'DEFENSE', 'SPEED', 'IQ', 'GUTS', 'BOOSTABLE_STATS']
    constants = []
    for name in names:
        found = re.findall(r'^const ' + name + r' := [^\n]+', character, re.M)
        d.require(len(found) == 1, 'Dropped item Character constant source')
        constants.append(found[0])
    # These typed methods are required when parsing the full original Inventory.
    # No pickup path calls them; fail rather than approximate character actions.
    methods = '''func get_hp():
 assert(false)
 return 0
func set_hp(_value):assert(false)
func get_pp():
 assert(false)
 return 0
func set_pp(_value):assert(false)
func get_stat(_key):
 assert(false)
 return 0
func has_status(_key):
 assert(false)
 return false
func remove_status(_key):assert(false)
func apply_boosts(_boost,_performed):assert(false)
'''
    flags_files, _, flags_proof = barrel.source()
    flags = flags_files['flags.gd']
    flags += '\nvar key_items\nvar storage\nvar characters={}\nvar button_prompts="Both"\nvar item_data={}\n'
    flags += 'func get_item_data(name):\n assert(item_data.has(name))\n return item_data[name]\n'
    cases = [dict(name='item-' + str(i + 1), binding=i) for i in range(7)]
    cases += [dict(name='full', fill=[16], repeat=12),
              dict(name='second-holder', fill=[16, 0]),
              dict(name='two-full', fill=[16, 16]),
              dict(name='key-bypass', fill=[16, 16], item='DogTreats'),
              dict(name='already-collected', flag=True),
              dict(name='disabled', pickup=False),
              dict(name='prompt-visible', nearby=True, action=10),
              dict(name='prompts-off', prompts='None'),
              dict(name='repeat-and-moving-target', repeat=1, move_player=1),
              dict(name='reset-after-collect', reset=True),
              dict(name='disappear', mode='disappear', frames=730),
              dict(name='paused-disappear', mode='disappear', frames=760, pause=100, unpause=130)]
    defaults = dict(binding=0, fill=[0], prompts='Both', flag=False, pickup=True,
                    reset=False, empty_dialogue='', nearby=False, frames=65,
                    action=0, mode='collect')
    cases = [dict(defaults, **case) for case in cases]
    classes = []
    for name, base, path in [('FlaggableObject', 'Sprite', model['paths']['flag']),
                             ('ItemHolder', 'FlaggableObject', model['paths']['holder']),
                             ('Item', 'Reference', model['paths']['item']),
                             ('Inventory', 'Reference', model['paths']['inventory']),
                             ('Character', 'Reference', 'character.gd'),
                             ('TextTools', 'Reference', 'text.gd')]:
        classes.append(dict(base=base, **{'class': name}, language='GDScript', path='res://' + path))
    project = 'config_version=4\n_global_script_classes=' + json.dumps(classes)
    project += '\n_global_script_class_icons={}\n[application]\nconfig/name="Dropped item source observation"\nrun/main_scene="res://main.tscn"\n'
    project += '[autoload]\nglobal="*res://global.gd"\nglobaldata="*res://flags.gd"\nuiManager="*res://ui.gd"\n'
    project += '[logging]\nfile_logging/enable_logging=false\n[rendering]\nquality/driver/driver_name="GLES2"\n'
    # Preserve the actual source project's six specific analyzer settings. Full
    # original Item/Inventory scripts intentionally shadow constructor fields.
    # Runtime diagnostics remain fatal in invoke(); no log filters are added.
    source_project = s.text('project.godot')
    warnings = re.findall(r'^gdscript/warnings/[^\n]+', source_project, re.M)
    d.require(warnings == ['gdscript/warnings/' + key + '=false' for key in
              ['unused_variable', 'shadowed_variable', 'unused_argument',
               'standalone_expression', 'unused_signal', 'integer_division']],
              'Dropped item original analyzer configuration changed')
    project += '[debug]\n' + '\n'.join(warnings) + '\n'
    global_source = '''extends Node
signal flags_updated
signal inputs_changed
signal locale_changed
var currentScene
var party=[]
var item
var player
func _ready():
 player=preload("res://player.gd").new()
 add_child(player)
func get_player():return player
func notify_flags():emit_signal("flags_updated")
func notify_inputs():emit_signal("inputs_changed")
func notify_locale():emit_signal("locale_changed")
'''
    player = '''extends Node2D
signal paused
signal unpaused
signal event_detector_entered(object)
signal event_detector_exited(object)
var pause_state=false
func is_paused():return pause_state
func set_pause(value):
 pause_state=value
 if value:emit_signal("paused")
 else:emit_signal("unpaused")
func nearby(object,value):
 if value:emit_signal("event_detector_entered",object)
 else:emit_signal("event_detector_exited",object)
'''
    config = dict(scene_name=model['name'], prefab=model['paths']['prefab'],
                  templates={str(v['id']): v for v in model['templates']},
                  bindings=model['bindings'], cases=cases, seed=1, filler='Hamburger')
    files = {'project.godot': project, 'probe.gd': PROBE, 'input.json': json.dumps(config),
             'main.tscn': '[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Observer" type="Node"]\nscript=ExtResource(1)\n',
             'global.gd': global_source, 'player.gd': player,
             'flags.gd': flags + '\nfunc _ready():item_data=' + json.dumps(data) + '\n',
             'scene.gd': 'extends Node2D\nsignal area_left(region_changed)\nfunc leave(region_changed):emit_signal("area_left",region_changed)\n',
             'character.gd': 'class_name Character\nextends Reference\n' + '\n'.join(constants) + '\nvar inv\n' + methods,
             'text.gd': 'class_name TextTools\nextends Reference\nstatic func get_key_name(_key):return "A"\n',
             'ui.gd': 'extends Node\nvar calls=[]\nfunc open_dialogue_box(path):calls.append(path)\n'}
    # JSON dictionaries in GDScript require its own boolean/null spellings.
    # JSON.parse avoids rewriting any string-valued source item content.
    files['flags.gd'] = flags + '\nfunc _ready():item_data=JSON.parse(' + json.dumps(json.dumps(data)) + ').result\n'
    for name, text in list(files.items()):
        if name.endswith('.gd'):
            files[name] = '\n'.join('\t' * (len(line) - len(line.lstrip(' '))) + line.lstrip(' ')
                                    for line in text.split('\n'))
    proof.update(copied_sha256={k: hashlib.sha256(v).hexdigest() for k, v in copied.items()},
                 generated_sha256={k: hashlib.sha256(v.encode()).hexdigest() for k, v in files.items()},
                 provider_sources=s.digests(), flags_provider=flags_proof,
                 scope=__doc__)
    return files, copied, proof


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--godot', type=Path, required=True)
    args = parser.parse_args()
    work = args.work.resolve()
    engine = args.godot.resolve()
    d.require(work.is_relative_to(ROOT / 'build') and not work.exists(), 'Fresh dropped item work directory')
    engine_sha = hashlib.sha256(engine.read_bytes()).hexdigest()
    d.require(engine_sha == 'e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd', 'Dropped item pinned native engine')
    files, copied, proof = source()
    work.mkdir()
    project = work / 'project'
    project.mkdir()
    (project / '.owned-dropped-item-reference').write_bytes(b'dropped-item-reference-v1')
    for name, raw in copied.items():
        target = project / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(raw)
    def write():
        for name, text in files.items():
            target = project / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(text.encode())
        (work / 'producer.py').write_bytes(Path(__file__).read_bytes())
    write()
    # Import only asset bytes in a separate project. Earlier --editor --quit
    # attempts timed out both with scripts and with assets alone; retain those
    # failures. Use the existing checked importer to quit after scanning ends.
    # Runtime still loads the original full scripts.
    asset_project = work / 'asset-import'
    asset_project.mkdir()
    import_project = 'config_version=4\n[logging]\nfile_logging/enable_logging=false\n[rendering]\nquality/driver/driver_name="GLES2"\n'
    import_project += '[editor_plugins]\nenabled=PoolStringArray("res://addons/source_import/plugin.cfg")\n'
    (asset_project / 'project.godot').write_bytes(import_project.encode())
    plugin = asset_project / 'addons/source_import'
    plugin.mkdir(parents=True)
    plugin_bytes = (ROOT / 'tools/godot_exporter/import_plugin.gd').read_bytes()
    (plugin / 'plugin.gd').write_bytes(plugin_bytes)
    plugin_config = '[plugin]\nname="Checked source import"\ndescription="Wait for source asset scan, then quit"\nauthor="Encore native"\nversion="1.0"\nscript="plugin.gd"\n'
    (plugin / 'plugin.cfg').write_bytes(plugin_config.encode())
    imported_sources = {}
    for name, raw in copied.items():
        if Path(name).suffix in ['.png', '.ttf', '.otf']:
            target = asset_project / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(raw)
            imported_sources[name] = hashlib.sha256(raw).hexdigest()
    proof['import_project_sha256'] = hashlib.sha256(import_project.encode()).hexdigest()
    proof['import_plugin_sha256'] = hashlib.sha256(plugin_bytes).hexdigest()
    proof['import_plugin_config_sha256'] = hashlib.sha256(plugin_config.encode()).hexdigest()
    proof['import_source_sha256'] = imported_sources
    (work / 'inputs.json').write_text(json.dumps(proof, indent=2) + '\n')
    start = time.monotonic()
    invoke(engine, asset_project, ['--video-driver', 'GLES2', '--debug', '--editor'], work / 'editor.log')
    d.require('ENCORE_IMPORT_COMPLETE' in (work / 'editor.log').read_text(),
              'Dropped item native importer did not finish')
    imported = {}
    for target in asset_project.rglob('*'):
        if target.is_file() and (target.name.endswith('.png.import')
                                 or target.relative_to(asset_project).parts[0] == '.import'):
            name = target.relative_to(asset_project).as_posix()
            raw = target.read_bytes()
            dest = project / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(raw)
            imported[name] = hashlib.sha256(raw).hexdigest()
    d.require(imported, 'Dropped item native asset importer produced nothing')
    proof['imported_sha256'] = imported
    write()
    invoke(engine, project, ['--video-driver', 'GLES2', '--debug', '--fixed-fps', '60',
                             '--disable-render-loop'], work / 'godot.log')
    d.require(all((project / k).read_bytes() == v for k, v in copied.items())
              and all((project / k).read_bytes() == v.encode() for k, v in files.items()),
              'Dropped item executed source changed')
    raw = (work / 'reference.json').read_bytes()
    from tools.dropped_item_protocol import validate, semantic_digest
    doc = validate(json.loads(raw), json.loads(files['input.json']))
    proof.update(engine_binary_sha256=engine_sha, result_sha256=hashlib.sha256(raw).hexdigest(),
                 producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                 protocol_sha256=hashlib.sha256((ROOT / 'tools/dropped_item_protocol.py').read_bytes()).hexdigest(),
                 semantic_sha256=semantic_digest(doc),
                 return_code=0, elapsed_seconds=time.monotonic() - start)
    (work / 'receipt.json').write_text(json.dumps(proof, indent=2) + '\n')
    print('Original dropped item cases:', len(doc['cases']), proof['result_sha256'])


if __name__ == '__main__':
    main()
