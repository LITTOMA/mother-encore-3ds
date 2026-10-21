"""Actual original Geyser lifecycle with audited selected map tree ordering."""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_geyser_factory_reference as base,catacombs_geyser_lifecycle as family
from tools.extract_battle_entry import Extractor,require
from tools.run_scene_reference import invoke

def source():
 files,copied,proof=base.source();_,binding,_=family.source();recipe,reader=base.p.read();nodes=base.p.instantiate(reader.doc,recipe['paths']['scene']);order={node.path:index for index,node in enumerate(nodes)};parent=binding['parent'];require(parent in order,'Map factory source parent missing');indices=[order[h['path']]for h in recipe['holes']];require(all(i<order[parent]for i in indices),'Selected map Timer-before-Objects tree ordering changed')
 block='''    var objects=Node2D.new()
    objects.name="Objects"
    objects.pause_mode=Node.PAUSE_MODE_STOP
    add_child(objects)
'''
 text=files['probe.gd'];require(text.count(block)==1 and text.count('        holes.append(hole)\nfunc snapshot():')==1,'Map factory exact observer insertion points');text=text.replace(block,'').replace('        holes.append(hole)\nfunc snapshot():','        holes.append(hole)\n'+block+'func snapshot():');files['probe.gd']=text;config=json.loads(files['input.json']);config['map_parent_order']=dict(parent=parent,parent_index=order[parent],holes=indices,animation_before_timers=False);files['input.json']=json.dumps(config)
 proof['generated_sha256']={name:hashlib.sha256(value.encode()).hexdigest()for name,value in files.items()};proof['map_parent_order']=config['map_parent_order'];proof['base_producer_sha256']=hashlib.sha256(Path(base.__file__).read_bytes()).hexdigest();proof['scope']=proof['scope']+['Selected source-map partial tree order:all hole Timers precede dynamically added Objects descendants;identity holders preserve relative order,not full map instantiation','Only observer holder creation order changes;all original methods,Timer/animation,deferred phases and recorded fields unchanged'];return files,copied,proof

def validate_result(doc,proof):return base.validate_result(doc,proof,expected_source=source()[2])

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);o=a.parse_args();out=o.out.resolve();engine=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe';require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh map factory reference output under build required');require(hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd','Map factory engine SHA');files,copied,proof=source();out.mkdir(parents=True);project=out/'temporary';project.mkdir();(project/'.owned-geyser-map-factory').write_bytes(b'geyser-map-factory-v1');ex=Extractor(ROOT);(out/'inputs.json').write_text(json.dumps(proof,indent=2)+'\n')
 for name in copied:
  if not name.endswith('.import'):target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(name))
 def write():
  for name,value in files.items():target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(value.encode())
 write();args=['--video-driver','GLES2','--debug'];invoke(engine,project,args+['--editor','--quit'],out/'fresh-import.log')
 for name in copied:
  if name.endswith('.import'):(project/name).write_bytes(ex.data(name))
 checks=list((project/'.import').glob('*.md5'));require(len(checks)==4,'Map factory fresh import coverage')
 for path in checks:require(path.resolve().parent==(project/'.import').resolve()and not path.is_symlink(),'Map factory import checksum escaped');path.unlink()
 invoke(engine,project,args+['--editor','--quit'],out/'source-import.log');(out/'editor-project.godot').write_bytes((project/'project.godot').read_bytes());write();invoke(engine,project,args+['--fixed-fps','60'],out/'godot.log');require(all(hashlib.sha256((project/n).read_bytes()).hexdigest()==h for n,h in proof['generated_sha256'].items()),'Map factory generated input changed');require(all(hashlib.sha256((project/n).read_bytes()).hexdigest()==h for n,h in proof['copied_source'].items()),'Map factory copied source changed');doc=validate_result(json.loads((out/'reference.json').read_bytes()),proof);proof.update(producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest());(out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n');print('Original Geyser selected map order:',len(doc['rows']),'frames,',doc['spawn_count'],'actual instances;not full map/hardware')
if __name__=='__main__':main()
