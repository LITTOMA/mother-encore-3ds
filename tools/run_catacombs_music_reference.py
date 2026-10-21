"""Original music methods and verbatim Catacombs area geometry in Godot3.6.2."""
import argparse,copy,hashlib,json,re,sys
from pathlib import Path
from unittest import mock
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_music as music,run_music_scene_reference as graph
from tools.extract_battle_entry import Extractor,require
from tools.godot_text import Document,node_path
from tools.run_scene_reference import invoke
GEOMETRY=r'''extends Node
func _ready():call_deferred("run")
func run():
 var input=File.new()
 assert(input.open("res://areas.json",File.READ)==OK)
 var config=JSON.parse(input.get_as_text()).result
 input.close()
 var source=load("res://source.tscn").instance()
 add_child(source)
 var rows=[]
 for path in config:
  var area=source.get_node(path)
  var shapes=[]
  for child in area.get_children():
   var e=child.shape.extents
   var corners=[]
   for local in [Vector2(-e.x,-e.y),Vector2(e.x,-e.y),Vector2(e.x,e.y),Vector2(-e.x,e.y)]:
    var point=child.global_transform.xform(local)
    corners.append([point.x,point.y])
   shapes.append({"path":path+"/"+child.name,"disabled":child.disabled,"points":corners})
  rows.append({"path":path,"layer":area.collision_layer,"mask":area.collision_mask,"monitoring":area.monitoring,"monitorable":area.monitorable,"visible_in_tree":area.is_visible_in_tree(),"music":area.music,"loop":area.loop,"gain":area.volume_db,"fadein":area.fadein_length,"fadeout":area.fadeout_length,"appear":area.appear_flag,"disappear":area.disappear_flag,"disabled":area.disabled,"diegetic":area.diegetic,"shapes":shapes})
 var output=File.new()
 assert(output.open("res://../geometry.json",File.WRITE)==OK)
 output.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows},"  "))
 output.close()
 source.free()
 get_tree().quit()
'''
def source():
 p,metadata,polygons,source_proof=music.source();files,base=graph.source()
 require(base==json.loads((ROOT/'compatibility/reviews/music-scene-graph-v0410.json').read_bytes()),'Existing original music-method review changed')
 # New source bindings for ten existing callback families;no old trace changes.
 mapping={'Music/MusicArea':'Music/MusicArea2','Music/MusicArea3':'Music/MusicArea3','Music/MusicArea5':'Music/MusicArea4','Music/MusicArea6':'Music/MusicArea','Music/MusicArea2':'Music/MusicArea','Music/MusicArea10':'Music/MusicArea4','Music/MusicArea11':'Music/MusicArea6','MusicArea':'Music/MusicArea2'}
 cases=copy.deepcopy(base['cases'])
 for case in cases:
  case['name']='catacombs_'+case['name']
  for action in case['actions']:
   if action[0]in ['enter','exit','stop']:action[1]=mapping[action[1]]
   if action[0]=='flag':action[1]='hidden_entrance_opened'
 tracks={t['stable_id']:t for t in metadata['tracks']}
 bindings=[dict(path=r['source_path'],music='',loop=tracks[r['track_id']]['source_path'].split('/')[-1],volume_db=r['volume_db'],fadein_length=r['fadein_seconds'],fadeout_length=r['fadeout_seconds'],disabled=r['disabled'],appear_flag=r['appear_flag'],disappear_flag=r['disappear_flag'])for r in metadata['regions']]
 imports=source_proof['imports'];scenes=[bindings,copy.deepcopy(bindings)];files['input.json']=json.dumps(dict(imports=imports,scenes=scenes,cases=cases))
 proof=dict(source_commit=source_proof['source_commit'],source=source_proof,upstream_method_review_sha256=hashlib.sha256((ROOT/'compatibility/reviews/music-scene-graph-v0410.json').read_bytes()).hexdigest(),manager_methods=base['manager_methods'],flag_methods=base['flag_methods'],parameters=base['parameters'],imports=imports,scenes=scenes,cases=cases,generated_sha256={k:hashlib.sha256(v.encode()).hexdigest()for k,v in files.items()},scope='Original callbacks with Catacombs bindings;explicit singleton/player/UI/flag services,manual callbacks,frozen Tween/event completion;no bank routing,NDSP,audible output or complete root')
 return files,proof
def validate_result(doc,proof):
 # Temporary scenario input only;no historical fixture or source is changed.
 with mock.patch.object(graph,'CASES',proof['cases']):return graph.validate_result(doc,proof)
def fixture(doc,proof):
 with mock.patch.object(graph,'CASES',proof['cases']):return graph.fixture(doc,proof)
def geometry_files():
 p,metadata,polygons,proof=music.source();ex=Extractor(ROOT);text=ex.text(p['scene']);doc=Document(p['scene'],text);nodes={node_path(n.attrs):n for n in doc.nodes};wanted={'Music'};ids=set();ext=set()
 for region in metadata['regions']:
  wanted.add(region['source_path']);wanted.update(region['shape_paths']);ext.add(nodes[region['source_path']].attrs['instance'].id)
  for path in region['shape_paths']:ids.add(nodes[path].props['shape'].id)
 starts=list(re.finditer(r'^\[([a-z_]+)([^\n]*)\]\r?$',text,re.M));sections=[];node_index=0;ext_blocks=[];sub_blocks=[]
 for i,match in enumerate(starts):
  raw=text[match.start():starts[i+1].start()if i+1<len(starts)else len(text)]
  if match[1]=='node':
   node=doc.nodes[node_index];node_index+=1
   if node_path(node.attrs)in wanted:sections.append(raw)
  elif match[1]in ['ext_resource','sub_resource']:
   ident=re.search(r'\bid=([0-9]+)',match[2]);require(ident,'Original geometry resource identity')
   if match[1]=='ext_resource'and int(ident[1])in ext:ext_blocks.append(raw)
   if match[1]=='sub_resource'and int(ident[1])in ids:sub_blocks.append(raw)
 require(len(ext_blocks)==1 and len(sub_blocks)==len(ids)and len(sections)==len(wanted),'Original geometry section coverage')
 raw='[gd_scene load_steps='+str(1+len(ext_blocks)+len(sub_blocks))+' format=2]\n\n'+''.join(ext_blocks+sub_blocks)+'[node name="'+doc.nodes[0].attrs['name']+'" type="Node2D"]\n\n'+''.join(sections)
 project='config_version=4\n[application]\nrun/main_scene="res://probe.tscn"\n[autoload]\nglobal="*res://global.gd"\nuiManager="*res://ui.gd"\naudioManager="*res://audio.gd"\nglobaldata="*res://flags.gd"\n[rendering]\nquality/driver/driver_name="GLES2"\n'
 files={'project.godot':project,'source.tscn':raw,'probe.gd':GEOMETRY,'probe.tscn':'[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Probe" type="Node"]\nscript=ExtResource( 1 )\n','areas.json':json.dumps([r['source_path']for r in metadata['regions']]),'global.gd':'extends Node\nfunc get_player():return null\n','ui.gd':'extends Node\nfunc is_in_cutscene():return false\nfunc is_in_battle():return false\n','audio.gd':'extends Node\nvar musicChangers=[]\n','flags.gd':'extends Node\nfunc check_appear_disappear_flags(a,_b):return a==""\n'}
 return files,proof
def validate_geometry(result,proof):
 require(type(result)is dict and set(result)=={'schema','godot','rows'}and result['schema']==1 and result['godot'].get('string')=='3.6.2-stable (official)','Catacombs native geometry schema/engine');metadata=proof['metadata'];tracks={t['stable_id']:t for t in metadata['tracks']}
 require(type(result['rows'])is list and len(result['rows'])==len(metadata['regions']),'Catacombs native geometry count')
 for row,region,geometry in zip(result['rows'],metadata['regions'],proof['polygons']):
  expected=dict(path=region['source_path'],layer=4096,mask=0,monitoring=True,monitorable=False,visible_in_tree=False,music='',loop=tracks[region['track_id']]['source_path'].split('/')[-1],gain=region['volume_db'],fadein=region['fadein_seconds'],fadeout=region['fadeout_seconds'],appear=region['appear_flag'],disappear=region['disappear_flag'],disabled=region['disabled'],diegetic=False,shapes=[dict(path=path,disabled=False,points=points)for path,points in zip(region['shape_paths'],geometry['polygons'])])
  require(row==expected,'Catacombs native geometry/properties differ:'+region['source_path'])
 return result
def write_files(project,files):
 for relative,value in files.items():
  target=project/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(value.encode('utf-8'))
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--godot',type=Path,default=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe');args=ap.parse_args();out=args.out.resolve()
 require(out.is_relative_to(ROOT/'build')and not out.exists(),'Fresh owned music reference under build required');engine=args.godot.resolve();require(hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd','Original engine SHA changed')
 out.mkdir(parents=True);files,proof=source();geometry,gproof=geometry_files();ex=Extractor(ROOT)
 project=out/'geometry-project';project.mkdir();(project/'.owned-catacombs-music-reference').write_bytes(b'catacombs-music-reference-v1');write_files(project,geometry)
 target=project/music.CHANGER;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(music.CHANGER));invoke(engine,project,['--video-driver','GLES2','--debug'],out/'geometry.log');validate_geometry(json.loads((out/'geometry.json').read_bytes()),gproof)
 project=out/'project';project.mkdir();(project/'.owned-catacombs-music-reference').write_bytes(b'catacombs-music-reference-v1')
 # Import source audio before installing test scripts/scenes.
 for imported in proof['imports']:
  target=project/imported['source'][6:];target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(imported['source'][6:]))
 write_files(project,{'project.godot':'config_version=4\n[application]\nconfig/name="Original Catacombs audio import"\n[rendering]\nquality/driver/driver_name="GLES2"\n'})
 invoke(engine,project,['--video-driver','GLES2','--debug','--editor','--quit'],out/'import.log');write_files(project,files)
 invoke(engine,project,['--video-driver','GLES2','--debug','--script',str(project/'probe.gd')],out/'godot.log');result=validate_result(json.loads((out/'reference.json').read_bytes()),proof);(out/'scenes.tsv').write_bytes(fixture(result,proof).encode('utf-8'))
 receipt=dict(source=proof,geometry=gproof,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest(),geometry_sha256=hashlib.sha256((out/'geometry.json').read_bytes()).hexdigest());(out/'receipt.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode('utf-8'));print('Original Catacombs music:',len(result['rows']),'callback sequences,5 verbatim source areas;no audible/full scene claim')
if __name__=='__main__':main()