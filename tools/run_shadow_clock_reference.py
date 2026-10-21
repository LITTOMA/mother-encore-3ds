"""Actual source Shadow scene at an explicit native fixed frame rate."""
import argparse,hashlib,json,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_shadow_graphics as graphics,catacombs_steps as steps
from tools.extract_battle_entry import Extractor,require
from tools.godot_text import Document,node_path
from tools.reference_movement import extract
from tools.run_scene_reference import invoke

TICKS=128
OPERATIONS={0:[['mode','ripple']],10:[['mode','ripple']],25:[['mode','magic']],29:[['frame',2]],32:[['frame',2]],36:[['visible',False]],47:[['visible',True]],50:[['player_pause',True]],60:[['position',[10.25,-20.5]]],65:[['player_pause',False]],70:[['tree_pause',True]],80:[['tree_pause',False]],85:[['mode','magic']],90:[['mode','shadow']],100:[['mode','ripple']],103:[['frame',1]],104:[['frame',1]],110:[['visible',False]],112:[['mode','magic']],120:[['visible',True]]}
PROBE=r'''extends Node
var config
var body
var sprite
var rows=[]
var tick=0
var initial
var warmed=false
func bits(value):
    var bytes=StreamPeerBuffer.new()
    bytes.big_endian=false
    bytes.put_float(value)
    bytes.seek(0)
    return bytes.get_u32()
func operations(index):
    for action in config.timeline[index]:
        match action[0]:
            "mode":body.set_shadow(action[1])
            "frame":sprite.frame=int(action[1])
            "visible":sprite.visible=action[1]
            "player_pause":body._paused=action[1]
            "tree_pause":get_tree().paused=action[1]
            "position":body.position=Vector2(action[1][0],action[1][1])
            _:assert(false)
func _ready():
    pause_mode=Node.PAUSE_MODE_PROCESS
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    config=JSON.parse(file.get_as_text()).result
    file.close()
    body=load("res://source.tscn").instance()
    body.pause_mode=Node.PAUSE_MODE_STOP
    body.position=Vector2(128.25,-40.5)
    add_child(body)
    sprite=body.get_node("Shadow")
    initial={"position":[sprite.position.x,sprite.position.y],"offset":[sprite.offset.x,sprite.offset.y],"centered":sprite.centered,"flip_h":sprite.flip_h,"flip_v":sprite.flip_v,"visible":sprite.visible,"behind":sprite.show_behind_parent,"playing":sprite.playing,"speed_scale":sprite.speed_scale,"frame":sprite.frame,"animation":sprite.animation,"gpu_snap":ProjectSettings.get_setting("rendering/2d/snapping/use_gpu_pixel_snap"),"modulate":[sprite.modulate.r,sprite.modulate.g,sprite.modulate.b,sprite.modulate.a],"self_modulate":[sprite.self_modulate.r,sprite.self_modulate.g,sprite.self_modulate.b,sprite.self_modulate.a],"physics_fps":Engine.iterations_per_second,"jitter_fix":Engine.physics_jitter_fix}
func _process(_delta):
    # Start the input sequence after one complete native processing pass. Its
    # mode change resets the timer before the first recorded clock step.
    if not warmed:
        operations(0)
        warmed=true
        return
    var texture=sprite.frames.get_frame(sprite.animation,sprite.frame)
    assert(texture is AtlasTexture)
    rows.append({"tick":tick,"actions":config.timeline[tick],"delta_bits":bits(sprite.get_process_delta_time()),"mode":body.get_shadow(),"frame":sprite.frame,"playing":sprite.playing,"behind":sprite.show_behind_parent,"visible":sprite.is_visible_in_tree(),"process_enabled":sprite.can_process(),"player_pause":body._paused,"tree_pause":get_tree().paused,"body_position":[body.position.x,body.position.y],"shadow_global":[sprite.global_position.x,sprite.global_position.y],"texture":texture.atlas.resource_path,"region":[texture.region.position.x,texture.region.position.y,texture.region.size.x,texture.region.size.y],"drawn_frames":Engine.get_frames_drawn()})
    tick+=1
    if tick==config.timeline.size():
        var file=File.new()
        assert(file.open("res://../reference.json",File.WRITE)==OK)
        file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"fps":config.fps,"no_draw":config.no_draw,"initial":initial,"rows":rows},"  "))
        file.close()
        get_tree().paused=false
        get_tree().quit()
        return
    operations(tick)
'''

def source(fps=60,no_draw=False):
 require(type(fps)is int and fps in(10,60)and type(no_draw)is bool,'Shadow native fixed-rate profile rejected');p,m,_,proof=graphics.build();policy,ex=steps.read();raw=ex.data(policy['party_script']);party,hashes=extract(raw,hashlib.sha256(raw).hexdigest(),['set_shadow','get_shadow']);doc=Document(p['player_scene'],ex.text(p['player_scene']));node=next(n for n in doc.nodes if node_path(n.attrs)==p['node']);text=ex.text(p['player_scene']);starts=list(re.finditer(r'^\[([a-z_]+)([^\n]*)\]\r?$',text,re.M));node_index=0;raw_node=None;raw_ext=None
 for i,match in enumerate(starts):
  block=text[match.start():starts[i+1].start()if i+1<len(starts)else len(text)]
  if match[1]=='node':
   selected=doc.nodes[node_index];node_index+=1
   if node_path(selected.attrs)==p['node']:raw_node=block
  if match[1]=='ext_resource'and re.search(r'\bid='+str(node.attrs['instance'].id)+r'(?:\s|$)',match[2]):raw_ext=block
 require(raw_node and raw_ext,'Shadow verbatim Player child source not found')
 scene='[gd_scene load_steps=3 format=2]\n\n'+raw_ext+'[ext_resource path="res://actor.gd" type="Script" id=1001]\n\n[node name="SourcePlayer" type="KinematicBody2D"]\nscript=ExtResource( 1001 )\n\n'+raw_node
 project='config_version=4\n[application]\nrun/main_scene="res://probe.tscn"\n[logging]\nfile_logging/enable_logging=false\n[physics]\ncommon/physics_fps='+str(fps)+'\ncommon/physics_jitter_fix=0.0\n[rendering]\nquality/driver/driver_name="GLES2"\n2d/snapping/use_gpu_pixel_snap='+('true'if m['flags']&16 else'false')+'\n'
 timeline=[OPERATIONS.get(i,[])for i in range(TICKS)];files={'project.godot':project,'source.tscn':scene,'actor.gd':'extends KinematicBody2D\nvar _paused=false\n'+party,'probe.gd':PROBE,'probe.tscn':'[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Probe" type="Node"]\nscript=ExtResource( 1 )\n','input.json':json.dumps(dict(timeline=timeline,fps=fps,no_draw=no_draw))}
 copied=[policy['shadow_scene'],policy['shadow_script']]+[name for name in policy['sources']if name.startswith('Graphics/Character Sprites/')]
 receipt=dict(source=proof,party_methods=hashes,fps=fps,no_draw=no_draw,timeline=timeline,generated_sha256={name:hashlib.sha256(value.encode()).hexdigest()for name,value in files.items()},copied_source={name:hashlib.sha256(ex.data(name)).hexdigest()for name in copied},adapters=['Fixed native --fixed-fps;eachactual Node delta recorded asIEEEbinary32 bits,no wallclock-derived timestep','One unrecorded processing warmup,then the first mode change resets the timer before the recorded sequence','Verbatim original Player Shadow instance section,complete Shadow scene/script and original image imports;selected original PartyObject set/get methods only','Probe PROCESS/body STOP are explicit tree-pause services;Player paused is an injected boolean,ordinary movement/gameplay not executed','No-draw variant explicitly passes --disable-render-loop;normal variants retain native rendering','Raw native frame/texture/mode and constructor properties,not GPU pixel or complete scene equivalence'])
 controls=json.loads((ROOT/'reports/catacombs-shadow-graphics/harness-engine/sources.json').read_bytes())
 for row in controls:require(row['commit']==p['engine_commit']and hashlib.sha256((ROOT/'reports/catacombs-shadow-graphics/harness-engine'/Path(row['path']).name).read_bytes()).hexdigest()==row['sha256'],'Shadow native clock-control source changed')
 receipt['clock_control_sources']=controls;receipt['adapters'].append('Isolated physics_fps=fixed idle fps andphysics_jitter_fix=0;these controls do not modify the original game project or movement');receipt['adapters'].append('Ordinary game OS update admission remains true even without a render loop;editor UPDATE_VITAL_ONLY is outside this profile')
 return files,copied,receipt

def validate_result(doc,proof):
 require(type(doc)is dict and set(doc)=={'schema','godot','fps','no_draw','initial','rows'}and type(doc['schema'])is int and doc['schema']==1 and type(doc['godot'])is dict and doc['godot'].get('string')=='3.6.2-stable (official)'and type(doc['fps'])is int and doc['fps']==proof['fps']and type(doc['no_draw'])is bool and doc['no_draw']==proof['no_draw'],'Shadow native clock version/profile')
 _,m,_,_=graphics.build();expected=dict(position=m['position'],offset=m['offset'],centered=bool(m['flags']&1),flip_h=bool(m['flags']&2),flip_v=bool(m['flags']&4),visible=bool(m['flags']&8),behind=not m['modes'][m['default']]['front'],playing=m['playing'],speed_scale=1,frame=0,animation=m['modes'][m['default']]['name'],gpu_snap=bool(m['flags']&16),modulate=[1,1,1,1],self_modulate=[1,1,1,1],physics_fps=proof['fps'],jitter_fix=0);require(doc['initial']==expected,'Native Shadow constructor/Player geometry differs');require(type(doc['rows'])is list and len(doc['rows'])==TICKS,'Shadow native clock row count');modes={a['name']:a for a in m['modes']};_,original,_=steps.source();frames={a['name']:a['frames']for a in original['shadows']['animations']}
 for i,row in enumerate(doc['rows']):
  require(type(row)is dict and set(row)==set('tick actions delta_bits mode frame playing behind visible process_enabled player_pause tree_pause body_position shadow_global texture region drawn_frames'.split())and type(row['tick'])is int and row['tick']==i and row['actions']==proof['timeline'][i]and type(row['delta_bits'])is int and 0<row['delta_bits']<0x7f800000 and type(row['mode'])is str and row['mode']in modes and type(row['frame'])is int and 0<=row['frame']<modes[row['mode']]['count'],'Shadow native clock state/action domain')
  require(all(type(row[k])is bool for k in['playing','behind','visible','process_enabled','player_pause','tree_pause'])and type(row['drawn_frames'])is int and row['drawn_frames']>=0,'Shadow native clock gate/counter domain');frame=frames[row['mode']][row['frame']];require(row['texture']==frame['atlas']and row['region']==frame['region'],'Shadow native clock source frame changed')
  require(row['delta_bits']==struct.unpack('<I',struct.pack('<f',1.0/proof['fps']))[0]and row['process_enabled']==(not row['tree_pause']),'Shadow native explicit clock/pause services differ')
  for key in['body_position','shadow_global']:require(type(row[key])is list and len(row[key])==2 and all(type(v)in(int,float)and abs(v)<=1e6 for v in row[key]),'Shadow native transform domain')
 return doc

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);a.add_argument('--fps',type=int,choices=(10,60),default=60);a.add_argument('--no-draw',action='store_true');a.add_argument('--godot',type=Path,default=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe');o=a.parse_args();out=o.out.resolve();engine=o.godot.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh owned Shadow native output required');require(hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd','Original engine SHA changed');files,copied,proof=source(o.fps,o.no_draw);out.mkdir(parents=True);project=out/'temporary';project.mkdir();(project/'.owned-shadow-clock-reference').write_bytes(b'shadow-clock-reference-v1');ex=Extractor(ROOT)
 from tools.run_catacombs_steps_reference import write_files
 for name in copied:
  if name.endswith('.png'):
   target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(name))
 write_files(project,{'project.godot':'config_version=4\n[rendering]\nquality/driver/driver_name="GLES2"\n'});invoke(engine,project,['--video-driver','GLES2','--debug','--editor','--quit'],out/'fresh-import.log')
 for name in copied:
  target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(name))
 expected={Path(name).name+'-'+hashlib.md5(('res://'+name).encode()).hexdigest()+'.md5'for name in copied if name.endswith('.png')};checksums=list((project/'.import').glob('*.md5'));require({p.name for p in checksums}==expected and not(project/'.import').is_symlink(),'Unexpected fresh Shadow import cache');proof['fresh_checksum_retirement']={p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in checksums}
 for path in checksums:require(path.resolve().parent==(project/'.import').resolve()and not path.is_symlink(),'Shadow import checksum escaped');path.unlink()
 invoke(engine,project,['--video-driver','GLES2','--debug','--editor','--quit'],out/'source-settings-import.log');write_files(project,files);args=['--video-driver','GLES2','--debug','--fixed-fps',str(o.fps)]+(['--disable-render-loop']if o.no_draw else[]);invoke(engine,project,args,out/'godot.log');result=validate_result(json.loads((out/'reference.json').read_bytes()),proof);proof.update(producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest(),native_defaults_verified=True,diagnostic_only_fields=['drawn_frames']);(out/'receipt.json').write_bytes((json.dumps(proof,indent=2)+'\n').encode());print('Native source Shadow clock:',len(result['rows']),'observations;fixedfps',o.fps,';no_draw',o.no_draw,';no complete gameplay or GPU pixel claim')
if __name__=='__main__':main()
