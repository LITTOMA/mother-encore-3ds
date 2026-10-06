"""Manual source projection checks; receives the actual full exports explicitly."""
import argparse,json,sys,tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.field_script_bindings import actual_scripts
from tools.podunk_scene import ROOT,read,require

def run(native,source):
 bindings,nulls,_=actual_scripts(native,source)
 for path,proof in nulls.items():
  require(path not in bindings and proof['previous_script'] and proof['source_sha256'],'Null script assignment retained an inherited callback')
  scene=read(ROOT/'content/podunk-scene-lifecycle.json')
  require(all(r['node']!=path for r in scene['roster']),'Null source entered script Ready roster')
  geometry=read(ROOT/'content/podunk-scene-geometry.json')
  rows=[r for r in geometry['nodes']if r['path']==path]
  require(len(rows)==1 and rows[0]['script']=='' and rows[0]['script_sha256']=='00'*32,'Null geometry remained scripted')
 with tempfile.TemporaryDirectory()as temp:
  bad=read(source);name=next(iter(bad['files']));bad['files'][name]['sha256']='00'*32
  target=Path(temp)/'changed-receipt.json';target.write_bytes(json.dumps(bad).encode())
  try:actual_scripts(native,target)
  except ValueError:pass
  else:raise AssertionError('Changed actual source receipt accepted')

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--native',type=Path,required=True);p.add_argument('--source',type=Path,required=True);a=p.parse_args();run(a.native,a.source)
