"""Actual original Catacombs stepping areas against original player polygon.

Only the source areas/shapes/scripts are instantiated. Player/party identity,
motion and selected PartyObject Shadow methods are explicit bounded services.
"""
import argparse,hashlib,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_steps as steps
from tools.extract_battle_entry import Extractor,require
from tools.godot_text import Document,node_path
from tools.reference_movement import extract
from tools.run_dungeon_key_contact_reference import player_source
from tools.run_scene_reference import invoke

PROBE=r'''extends Node
var config
var bodies=[]
var areas=[]
var source
var rows=[]
var events=[]
func _ready():call_deferred("run")
func shadow(body):
    var s=body.get_node("Shadow")
    return {"mode":body.get_shadow(),"behind":s.show_behind_parent,"playing":s.playing}
func notice(body,index,entered):
    events.append({"area":index,"entered":entered,"body":bodies.find(body),"wanted":global.player.run_sound,"player":shadow(bodies[1]),"party":shadow(bodies[0])})
func settle():
    for _i in 4:yield(get_tree(),"physics_frame")
    yield(get_tree(),"idle_frame")
func capture(ci,phase):
    var overlaps=[]
    for area in areas:
        var active=[]
        for body in bodies:active.append(area.overlaps_body(body))
        overlaps.append(active)
    var positions=[]
    for body in bodies:positions.append({"position":[body.position.x,body.position.y],"layer":body.collision_layer,"mask":body.collision_mask})
    rows.append({"case":ci,"phase":phase,"wanted":global.player.run_sound,"player":shadow(bodies[1]),"party":shadow(bodies[0]),"bodies":positions,"overlaps":overlaps,"events":events.duplicate(true)})
    events.clear()
func geometry():
    var result=[]
    for i in areas.size():
        var area=areas[i]
        var shapes=[]
        for child in area.get_children():
            var points=[]
            if child is CollisionPolygon2D:
                for point in child.polygon:
                    var p=child.global_transform.xform(point)
                    points.append([p.x,p.y])
            else:
                assert(child is CollisionShape2D and child.shape is RectangleShape2D)
                var e=child.shape.extents
                for point in [Vector2(-e.x,-e.y),Vector2(e.x,-e.y),Vector2(e.x,e.y),Vector2(-e.x,e.y)]:
                    var p=child.global_transform.xform(point)
                    points.append([p.x,p.y])
            var parts=[]
            for owner in area.get_shape_owners():
                if area.shape_owner_get_owner(owner)!=child:continue
                for j in area.shape_owner_get_shape_count(owner):
                    var shape=area.shape_owner_get_shape(owner,j)
                    var native=[]
                    if shape is ConvexPolygonShape2D:
                        for point in shape.points:
                            var p=child.global_transform.xform(point)
                            native.append([p.x,p.y])
                    else:
                        assert(shape is RectangleShape2D)
                        native=points.duplicate(true)
                    parts.append(native)
            shapes.append({"path":config.areas[i].path+"/"+child.name,"disabled":child.disabled,"points":points,"parts":parts})
        result.append({"path":config.areas[i].path,"layer":area.collision_layer,"mask":area.collision_mask,"monitoring":area.monitoring,"monitorable":area.monitorable,"enter":area.entering_sound,"exit":area.exiting_sound,"enter_shadow":area.enter_shadow_effect,"exit_shadow":area.exit_shadow_effect,"shapes":shapes})
    return result
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    config=JSON.parse(file.get_as_text()).result
    file.close()
    for i in 3:
        var body=load("res://actor.gd").new() if i==1 else(load("res://party.gd").new() if i==0 else KinematicBody2D.new())
        body.name="Body"+str(i)
        body.collision_layer=config.player.layer
        body.collision_mask=config.player.mask
        var collider=CollisionPolygon2D.new()
        collider.name="CollisionShape2D"
        collider.position=Vector2(config.player.position[0],config.player.position[1])
        var points=PoolVector2Array()
        for point in config.player.points:points.append(Vector2(point[0],point[1]))
        collider.polygon=points
        body.add_child(collider)
        if i!=2:body.add_child(load("res://"+config.shadow_scene).instance())
        body.position=Vector2(20000,20000)
        add_child(body)
        bodies.append(body)
    global.player=bodies[1]
    var original=[]
    for ci in config.cases.size():
        var test=config.cases[ci]
        for body in bodies:
            body.position=Vector2(20000,20000)
            body.collision_layer=config.player.layer
            body.collision_mask=config.player.mask
        bodies[1].run_sound=config.default
        bodies[1].set_shadow(config.shadow)
        bodies[0].set_shadow(config.shadow)
        var body=bodies[test.body]
        body.collision_layer=test.layer
        body.collision_mask=test.mask
        var inside=Vector2(test.point[0]-config.player.center[0],test.point[1]-config.player.center[1])
        if test.initial:body.position=inside
        source=load("res://source.tscn").instance()
        add_child(source)
        areas=[]
        for i in config.areas.size():
            var area=source.get_node(config.areas[i].path)
            area.enabled=test.enabled
            assert(area.connect("body_entered",self,"notice",[i,true])==OK)
            assert(area.connect("body_exited",self,"notice",[i,false])==OK)
            areas.append(area)
        if original.empty():original=geometry()
        events.clear()
        yield(settle(),"completed")
        capture(ci,"initial")
        body.position=inside
        yield(settle(),"completed")
        capture(ci,"inside")
        yield(settle(),"completed")
        capture(ci,"stay")
        for area in areas:area.disable()
        body.position=Vector2(20000,20000)
        yield(settle(),"completed")
        capture(ci,"disabled_exit")
        source.queue_free()
        yield(settle(),"completed")
        areas=[]
        events.clear()
    assert(file.open("res://../contacts.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"geometry":original,"rows":rows},"  "))
    file.close()
    for body in bodies:body.queue_free()
    yield(get_tree(),"idle_frame")
    yield(get_tree(),"idle_frame")
    get_tree().quit()
'''

def geometry_source(p,m,ex):
 text=ex.text(p['scene']);doc=Document(p['scene'],text);wanted={'Stepping Sounds'};ids=set();ext=set();nodes={node_path(n.attrs):n for n in doc.nodes}
 for area in m['areas']:
  wanted.add(area['path']);ext.add(nodes[area['path']].attrs['instance'].id)
  for shape in area['shapes']:
   wanted.add(shape['path']);node=nodes[shape['path']]
   if 'shape'in node.props:ids.add(node.props['shape'].id)
 starts=list(re.finditer(r'^\[([a-z_]+)([^\n]*)\]\r?$',text,re.M));sections=[];ext_blocks=[];sub_blocks=[];index=0
 for i,match in enumerate(starts):
  raw=text[match.start():starts[i+1].start()if i+1<len(starts)else len(text)]
  if match[1]=='node':
   node=doc.nodes[index];index+=1
   if node_path(node.attrs)in wanted:sections.append(raw)
  elif match[1]in['ext_resource','sub_resource']:
   ident=re.search(r'\bid=([0-9]+)',match[2]);require(ident,'Stepping geometry resource identity')
   if match[1]=='ext_resource'and int(ident[1])in ext:ext_blocks.append(raw)
   if match[1]=='sub_resource'and int(ident[1])in ids:sub_blocks.append(raw)
 require(len(ext_blocks)==1 and len(sub_blocks)==len(ids)and len(sections)==len(wanted),'Stepping verbatim geometry coverage')
 return '[gd_scene load_steps='+str(1+len(ext_blocks)+len(sub_blocks))+' format=2]\n\n'+''.join(ext_blocks+sub_blocks)+'[node name="SourceAreas" type="Node2D"]\n\n'+''.join(sections)

def source():
 p,m,proof=steps.source();ex=Extractor(ROOT);raw=ex.data(p['party_script']);party,hashes=extract(raw,hashlib.sha256(raw).hexdigest(),['set_shadow','get_shadow']);player,player_proof=player_source();cases=[]
 def case(name,point,**kw):
  item=dict(name=name,point=point,body=1,layer=player['layer'],mask=player['mask'],enabled=True,initial=False);item.update(kw);cases.append(item)
 for area in m['areas']:
  for si,shape in enumerate(area['shapes']):
   for pi,part in enumerate(shape['parts']):case(area['path']+'/'+str(si)+'/'+str(pi),[sum(v[k]for v in part)/len(part)for k in[0,1]])
 point=cases[0]['point']
 for body in[0,1,2]:case('disabled_body_'+str(body),point,enabled=False,body=body)
 for layer,mask in[(1,0),(2,4353),(0,4353),(2,2048),(0,2048)]:case('filter_'+str(layer)+'_'+str(mask),point,layer=layer,mask=mask)
 for i,area in enumerate(m['areas']):
  part=area['shapes'][0]['parts'][0];case('initial_overlap_'+str(i),[sum(v[k]for v in part)/len(part)for k in[0,1]],initial=True)
 water=m['areas'][0]
 for x,y in[(-1,-1),(-1,1),(1,-1),(1,1),(0,0)]:case('water_broad_bounds_'+str(x)+'_'+str(y),[water['center'][0]+x*(water['extents'][0]-8),water['center'][1]+y*(water['extents'][1]-8)])
 for body in[0,2]:case('ordinary_body_'+str(body),point,body=body)
 project='config_version=4\n_global_script_classes=[{"base":"KinematicBody2D","class":"PartyObject","language":"GDScript","path":"res://party.gd"}]\n_global_script_class_icons={"PartyObject":""}\n[application]\nrun/main_scene="res://probe.tscn"\n[autoload]\nglobal="*res://global.gd"\n[logging]\nfile_logging/enable_logging=false\n[rendering]\nquality/driver/driver_name="GLES2"\n'
 files={'project.godot':project,'source.tscn':geometry_source(p,m,ex),'party.gd':'class_name PartyObject\nextends KinematicBody2D\n'+party,'actor.gd':'extends "res://party.gd"\nvar run_sound="'+m['default']+'"\n','global.gd':'extends Node\nvar player\nfunc get_player():return player\n','probe.gd':PROBE,'probe.tscn':'[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Probe" type="Node"]\nscript=ExtResource( 1 )\n','input.json':json.dumps(dict(player=player,shadow=m['shadow'],default=m['default'],shadow_scene=p['shadow_scene'],areas=m['areas'],cases=cases))}
 copied=[p[k]for k in['area_scene','area_script','shadow_scene','shadow_script']]+[name for name in p['sources']if name.startswith('Graphics/Character Sprites/')and not name.endswith('.import')]
 return files,copied,dict(source=proof,party_methods=hashes,player=player_proof,cases=cases,generated_sha256={name:hashlib.sha256(value.encode()).hexdigest()for name,value in files.items()},copied_source={name:hashlib.sha256(ex.data(name)).hexdigest()for name in copied},adapters=['Verbatim three area instances,thirteen original shape declarations,base area scene and original full callbacks','Original player eight-vertex polygon/local origin/layer/mask on injected body; movement/pause/party behavior not executed','Only PartyObject class identity and original set/get Shadow methods; full original Shadow resource/script','Four physics frames and one idle per observation; exact frame latency and animated frame phase not compared','Initial overlap explicitly constructed; observer/original signal ordering not assumed; settled final state is authoritative; no whole Catacombs root/terrain'],scope='Original area shape decomposition and body contacts only; no complete map/player/graphics/NDSP or hardware pass')

def canonical(points):
 steps.convex(points);values=[tuple(p)for p in points]
 return min(tuple(values[i:]+values[:i])for i in range(len(values)))

def validate_result(doc,proof):
 require(type(doc)is dict and set(doc)=={'schema','godot','geometry','rows'}and type(doc['schema'])is int and doc['schema']==1 and type(doc['godot'])is dict and doc['godot'].get('string')=='3.6.2-stable (official)','Native Catacombs stepping contacts schema/engine');_,m,_=steps.source()
 require(type(doc['geometry'])is list and len(doc['geometry'])==len(m['areas']),'Native stepping area geometry count')
 for row,area in zip(doc['geometry'],m['areas']):
  expected=dict(path=area['path'],layer=area['layer'],mask=area['mask'],monitoring=True,monitorable=False,enter=area['enter'],exit=area['exit'],enter_shadow=area['enter_shadow'],exit_shadow=area['exit_shadow']);require(type(row)is dict and set(row)==set(expected)|{'shapes'}and all(row[k]==v for k,v in expected.items()),'Native stepping area properties mismatch')
  require(type(row['shapes'])is list and len(row['shapes'])==len(area['shapes']),'Native stepping source shape coverage')
  for actual,shape in zip(row['shapes'],area['shapes']):
   require(type(actual)is dict and set(actual)=={'path','disabled','points','parts'}and actual['path']==shape['path']and actual['disabled']is False and actual['points']==shape['points']and sorted(canonical(p)for p in actual['parts'])==sorted(canonical(p)for p in shape['parts']),'Native stepping original polygon/decomposition mismatch:'+shape['path'])
 require(type(doc['rows'])is list and len(doc['rows'])==len(proof['cases'])*4,'Native stepping contact phase count')
 names={s['name']for s in m['sounds']}
 modes={a['name']for a in m['shadows']['animations']}
 def shadow(value):require(type(value)is dict and set(value)=={'mode','behind','playing'}and type(value['mode'])is str and value['mode']in modes and type(value['behind'])is bool and type(value['playing'])is bool,'Native contact Shadow observation domain')
 for ci,test in enumerate(proof['cases']):
  for pi,phase in enumerate(['initial','inside','stay','disabled_exit']):
   row=doc['rows'][ci*4+pi];require(type(row)is dict and set(row)=={'case','phase','wanted','player','party','bodies','overlaps','events'}and type(row['case'])is int and row['case']==ci and row['phase']==phase and row['wanted']in names,'Native stepping contact state/order')
   require(type(row['bodies'])is list and len(row['bodies'])==3,'Native stepping body observation count')
   for body in row['bodies']:require(type(body)is dict and set(body)=={'position','layer','mask'}and type(body['position'])is list and len(body['position'])==2 and all(type(v)in(int,float)and abs(v)<=1e6 for v in body['position'])and all(type(body[k])is int and 0<=body[k]<2**32 for k in['layer','mask']),'Native stepping body observation domain')
   require(type(row['overlaps'])is list and len(row['overlaps'])==len(m['areas'])and all(type(v)is list and len(v)==3 and all(type(b)is bool for b in v)for v in row['overlaps']),'Native stepping contact overlap domain')
   require(type(row['events'])is list and len(row['events'])<=len(m['areas']),'Native stepping contact callback count')
   shadow(row['player']);shadow(row['party'])
   for event in row['events']:
    require(type(event)is dict and set(event)=={'area','entered','body','wanted','player','party'}and type(event['area'])is int and 0<=event['area']<len(m['areas'])and type(event['entered'])is bool and type(event['body'])is int and 0<=event['body']<3 and event['wanted']in names,'Native stepping body event domain');shadow(event['player']);shadow(event['party'])
 return doc

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);a.add_argument('--godot',type=Path,default=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe');o=a.parse_args();out=o.out.resolve();engine=o.godot.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh owned step contact project under build required');require(hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd','Original engine SHA changed')
 files,copied,proof=source();out.mkdir(parents=True);project=out/'temporary';project.mkdir();(project/'.owned-catacombs-step-contacts-reference').write_bytes(b'catacombs-step-contacts-reference-v1');ex=Extractor(ROOT)
 for name in copied:
  if name.endswith('.png'):
   target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(name))
 from tools.run_catacombs_steps_reference import write_files
 write_files(project,{'project.godot':'config_version=4\n[rendering]\nquality/driver/driver_name="GLES2"\n'});invoke(engine,project,['--video-driver','GLES2','--debug','--editor','--quit'],out/'fresh-import.log')
 # The physical contact comparison excludes texture filtering/pixels. This
 # project uses fresh imports; the callback companion verifies source settings.
 for name in copied:
  if name.endswith('.import'):continue
  target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(name))
 write_files(project,files);invoke(engine,project,['--video-driver','GLES2','--debug'],out/'godot.log');result=validate_result(json.loads((out/'contacts.json').read_bytes()),proof)
 proof.update(producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),result_sha256=hashlib.sha256((out/'contacts.json').read_bytes()).hexdigest(),texture_scope='Fresh native PNG imports in contacts project;original settings verified separately by callback companion;no pixel claim')
 (out/'receipt.json').write_bytes((json.dumps(proof,indent=2)+'\n').encode());print('Original Catacombs stepping contacts:',len(proof['cases']),'cases,',len(result['rows']),'observations;13 original shapes;no complete scene pass')
if __name__=='__main__':main()
