"""Native original key/door prefab geometry and stream metadata.

Original subresources and relevant Sprite/body properties are retained. Scripts
and their admitted signals are quarantined; ButtonPrompt is an explicit UI
service, and terrain is not instantiated. This is not gameplay or audibility.
"""
import argparse,hashlib,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_key_media as d,dungeon_key_clocks as c
from tools.godot_text import Document,instantiate
from tools.run_dungeon_keys_reference import GLOBAL
from tools.run_scene_reference import invoke

PROBE='''extends Node
func vec(v):return ["%.17f"%v.x,"%.17f"%v.y]
func rect(v):return {"position":vec(v.position),"size":vec(v.size)}
func _ready():call_deferred("run")
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var input=JSON.parse(file.get_as_text()).result
    file.close()
    var scene=Node2D.new()
    scene.name="SourceGeometry"
    scene.position=Vector2(input.root_position[0],input.root_position[1])
    var parents={}
    for row in input.parents:
        var parent=TileMap.new()
        parent.name=row.path
        parent.position=Vector2(row.position[0],row.position[1])
        parent.cell_size=Vector2(row.cell_size[0],row.cell_size[1])
        parent.cell_y_sort=row.sort
        parent.visible=row.visible
        parent.z_index=row.z
        parent.z_as_relative=row.z_relative
        scene.add_child(parent)
        parents[row.path]=parent
    var targets=[]
    for row in input.owners:
        var target=load("res://"+row.scene).instance()
        target.name=row.name
        target.position=Vector2(row.position[0],row.position[1])
        parents[row.parent].add_child(target)
        target.get_node("AnimationPlayer").playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
        if row.child:target.get_node("Lock/AnimationPlayer").playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
        targets.append(target)
    add_child(scene)
    var sprites=[]
    var shapes=[]
    var sounds=[]
    for i in targets.size():
        var target=targets[i]
        var owner=input.owners[i]
        for selection in owner.sprites:
            var sprite=target.get_node(selection.path)
            for frame in sprite.hframes*sprite.vframes:
                sprite.frame=frame
                sprites.append({"owner":owner.id,"profile":selection.profile,"path":selection.path,"frame":frame,"local_position":vec(sprite.position),"global_position":vec(sprite.global_position),"rect":rect(sprite.get_rect()),"tree_visible":sprite.is_visible_in_tree(),"centered":sprite.centered,"offset":vec(sprite.offset),"flip_h":sprite.flip_h,"flip_v":sprite.flip_v,"z":sprite.z_index,"z_relative":sprite.z_as_relative,"parent_type":target.get_parent().get_class(),"parent_sort":target.get_parent().cell_y_sort})
        for selection in owner.shapes:
            var node=target.get_node(selection.path)
            assert(node is CollisionShape2D and node.shape is RectangleShape2D)
            var body=node.get_parent()
            var corners=[]
            for point in [Vector2(-1,-1),Vector2(1,-1),Vector2(1,1),Vector2(-1,1)]:corners.append(vec(node.global_transform*(point*node.shape.extents)))
            shapes.append({"owner":owner.id,"path":selection.path,"role":selection.role,"type":body.get_class(),"center":vec(node.global_transform.origin),"extents":vec(node.shape.extents),"scale":vec(node.global_transform.get_scale()),"corners":corners,"layer":body.collision_layer,"mask":body.collision_mask,"disabled":node.disabled,"one_way":node.one_way_collision,"margin":node.one_way_collision_margin,"monitoring":body.monitoring if body is Area2D else false,"monitorable":body.monitorable if body is Area2D else false})
        var audio=target.get_node(owner.sound_path)
        var stream=audio.stream
        sounds.append({"owner":owner.id,"path":owner.sound_path,"bus":audio.bus,"gain":"%.17f"%audio.volume_db,"pitch":"%.17f"%audio.pitch_scale,"pause":audio.pause_mode,"stream_type":stream.get_class(),"length":"%.17f"%stream.get_length()})
    var reflected=load("res://Scripts/Main/Lock.gd").new()
    var turn=reflected.player_turn.duplicate()
    reflected.free()
    assert(file.open("res://../geometry.json",File.WRITE)==OK)
    file.store_string(JSON.print({"engine":Engine.get_version_info(),"sprites":sprites,"shapes":shapes,"sounds":sounds,"player_turn":turn},"  "))
    file.close()
    scene.queue_free()
    yield(get_tree(),"idle_frame")
    yield(get_tree(),"idle_frame")
    print("KEY_MEDIA_GEOMETRY_COMPLETE")
    get_tree().quit()
'''

def numeric(raw,row):
 doc=Document(row['scene'],raw);script=[i for i,(path,kind)in doc.ext.items()if kind=='Script'];d.require(len(script)==1 and doc.ext[script[0]][0]==row['script'],'Key geometry source script quarantine')
 rid=script[0];out,n=re.subn(r'^\[ext_resource path="res://'+re.escape(row['script'])+r'" type="Script" id='+str(rid)+r'\]\n','',raw,flags=re.M);d.require(n==1,'Key geometry script declaration')
 out,n=re.subn(r'^script = ExtResource\( '+str(rid)+r' \)\n','',out,flags=re.M);d.require(n==1,'Key geometry script attachment');removed=['script']
 for name in ['emit_flag_updated_signal','anim_name','anim_player']:
  old=sum(name in node.props for node in doc.nodes);out,n=re.subn(r'^'+name+r' = [^\n]*\n','',out,flags=re.M);d.require(n==old,'Key geometry known script export quarantine');removed.append(name)
 out,n=re.subn(r'^\[connection [^\n]*\]\n?','',out,flags=re.M);d.require(n==len(doc.connections),'Key geometry known signal quarantine');out,n=re.subn('load_steps='+str(doc.header.attrs['load_steps'])+r'\b','load_steps='+str(doc.header.attrs['load_steps']-1),out,count=1);d.require(n==1,'Key geometry resource count')
 after=Document(row['scene'],out);d.require(doc.sub==after.sub and len(doc.nodes)==len(after.nodes),'Key geometry subresource/node change')
 for a,b in zip(doc.nodes,after.nodes):d.require(a.attrs==b.attrs and {k:v for k,v in a.props.items()if k not in removed}==b.props,'Key geometry numeric property changed')
 return out,dict(script=row['script'],removed_properties=removed,connections=doc.connections,scope='Script/signals only; exact Sprite/body/subresources retained. ButtonPrompt dependency is a separate declared UI service.')

def run(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve();d.require(out.is_relative_to(ROOT/'build')and not out.exists()and engine.is_file(),'Fresh key geometry output and actual engine required');review,source=d.read();clock_review,ex=c.read();clock,_,_=c.extract();model,_,proof=d.source_model();nodes={n.path:n for n in instantiate(source.doc,'Maps/podunk/Catacombs.tscn')};project=out/'project';project.mkdir(parents=True);(project/'.owned-key-media-reference').write_text('source-key-media-reference-v1',encoding='utf8');copied={};quarantine={}
 staged={}
 for row in clock_review['profiles']:
  text,boundary=numeric(source.text(row['scene']),row);staged[row['scene']]=text.encode();quarantine[row['scene']]=boundary
  for name in [row['texture'],row['texture']+'.import']+([row['audio'],row['audio']+'.import']if row['audio']else[]):
   raw=source.data(name);copied[name]=hashlib.sha256(raw).hexdigest()
   if name.endswith('.import'):staged[name]=raw
   else:
    dest=project/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(raw)
 # Original Lock defaults are reflected without entering its script in a tree.
 for name in ['Scripts/Main/Lock.gd','Scripts/Main/FlaggableObject.gd']:
  raw=source.data(name);staged[name]=raw;copied[name]=hashlib.sha256(raw).hexdigest()
 # The editor tries to load the remapped stream before its first import when
 # an upstream descriptor already exists. Bootstrap caches without descriptors,
 # then restore the exact source import settings before the admitted run.
 (project/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Original key assets import"\n',encoding='utf8')
 invoke(engine,project,['--debug','--editor','--quit'],out/'assets-import.log')
 for name,raw in staged.items():
  dest=project/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(raw)
 prompt=project/'Nodes/Ui/ButtonPrompt.tscn';prompt.parent.mkdir(parents=True,exist_ok=True);prompt.write_text('[gd_scene load_steps=2 format=2]\n[ext_resource path="res://prompt.gd" type="Script" id=1]\n[node name="ButtonPrompt" type="Node2D"]\nscript = ExtResource( 1 )\n',encoding='utf8');(project/'prompt.gd').write_text('extends Node2D\nexport var offset=Vector2.ZERO\nexport var enabled=true\n',encoding='utf8')
 config=dict(root_position=d.vector(nodes['.'].props.get('position',d.Vec2(0,0))),parents=[],owners=[]);parents={o['parent']for o in model['owners']}
 for name in sorted(parents):
  d.require('/'not in name,'Key geometry parent service only admits source root TileMaps');node=nodes[name];d.require(node.type=='TileMap','Key geometry source parent type');config['parents'].append(dict(path=name,position=d.vector(node.props.get('position',d.Vec2(0,0))),cell_size=d.vector(node.props['cell_size']),sort=node.props.get('cell_y_sort',False),visible=node.props.get('visible',True),z=node.props.get('z_index',0),z_relative=node.props.get('z_as_relative',True)))
 by={p['id']:p for p in clock['profiles']};rp={p['id']:p for p in clock_review['profiles']}
 for o in model['owners']:
  p=by[o['profile']];n=nodes[o['path']];sprites=[dict(profile=p['id'],path='Sprite')]+([dict(profile=p['child'],path='Lock/Sprite')]if p['child']else[]);shapes=[dict(path=s['path'],role=s['role'])for s in model['shapes']if s['owner']==o['owner']];config['owners'].append(dict(id=o['owner'],parent=o['parent'],name=o['path'].rsplit('/',1)[-1],position=d.vector(n.props.get('position',d.Vec2(0,0))),scene=rp[p['id']]['scene'],child=bool(p['child']),sprites=sprites,shapes=shapes,sound_path=('Lock/'if p['child']else'')+'AudioStreamPlayer'))
 (project/'input.json').write_text(json.dumps(config,indent=2)+'\n',encoding='utf8');(project/'probe.gd').write_text(PROBE,encoding='utf8');(project/'probe.tscn').write_text('[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Probe" type="Node"]\nscript = ExtResource( 1 )\n',encoding='utf8');(project/'global.gd').write_text(GLOBAL,encoding='utf8');(project/'data.gd').write_text('extends Node\nvar flags={}\nvar object_flags={}\nfunc set_flag(_a,_b,_c):pass\nfunc set_object_flag(_a,_b,_c):pass\n',encoding='utf8');(project/'ui.gd').write_text('extends Node\nfunc try_alter_key_count(_v):return false\nfunc open_dialogue_box(_p):pass\nfunc update_key_indicator():pass\n',encoding='utf8')
 (project/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Original key media geometry"\nrun/main_scene="res://probe.tscn"\n[autoload]\nglobal="*res://global.gd"\nglobaldata="*res://data.gd"\nuiManager="*res://ui.gd"\n[debug]\ngdscript/warnings/unused_argument=false\ngdscript/warnings/unused_signal=false\n',encoding='utf8')
 invoke(engine,project,['--debug','--editor','--quit'],out/'import.log');invoke(engine,project,['--debug'],out/'engine.log');observed=json.loads((out/'geometry.json').read_text('utf8'));d.require(observed['engine']['string']=='3.6.2-stable (official)'and len(observed['sprites'])==69 and len(observed['shapes'])==9 and len(observed['sounds'])==6,'Key media original engine/coverage')
 for name,h in copied.items():
  if not name.endswith('.import'):d.require(hashlib.sha256((project/name).read_bytes()).hexdigest()==h,'Key geometry original bytes changed')
  else:
   before=d.parse_godot('[gd_resource format=2]\n'+source.text(name));after=d.parse_godot('[gd_resource format=2]\n'+(project/name).read_text('utf8'));d.require([(s.tag,s.attrs,s.props)for s in before]==[(s.tag,s.attrs,s.props)for s in after],'Key geometry import settings changed')
 proof.update(engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),scope=__doc__,quarantine=quarantine,ui_service='ButtonPrompt subtree only; no font/UI rendering admission',copied_sources=copied,geometry_sha256=hashlib.sha256((out/'geometry.json').read_bytes()).hexdigest(),probe_sha256=hashlib.sha256(PROBE.encode()).hexdigest(),sprites=69,shapes=9,sounds=6);(out/'input.json').write_bytes((project/'input.json').read_bytes());(out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Original key media geometry:69 frame rectangles,9 native shapes,6 source sound nodes; no scene/contact/audible admission')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();run(a.out,a.godot)
