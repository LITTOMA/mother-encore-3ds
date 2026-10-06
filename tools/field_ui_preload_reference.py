#!/usr/bin/env python3
"""Complete original UiManager preload resource-only quarantine references."""
from pathlib import Path
import argparse,json,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.field_ui_manager_reference import prepare_ui
from tools.podunk_scene import read,require,PIN,write,sha

def roster():
 d=read(ROOT/'content/field-ui-manager.json');have={r['scene']for r in read(ROOT/'content/field-ui-manager-recipes.json')['recipes']};require(d['commit']==PIN,'UI source pin changed')
 return [p for p in d['preloads']if p['native']=='PackedScene'and p['path']not in have]
def prepare(out):
 out=Path(out).resolve();require(out.is_relative_to((ROOT/'build').resolve())and not out.exists(),'Fresh preload quarantine under build required');out.mkdir(parents=True);entries=[]
 for index,p in enumerate(roster()):
  if p['path']=='Nodes/Ui/DialogueBox.tscn':entries.append(dict(index=index,source=p['path'],borrowed='content/dialogue-node-recipe.json'));continue
  root=out/('scene-'+str(index));d=prepare_ui(root,p['path']);extra='tools/godot_exporter/field_ui_preload_native.gd';(root/'field_ui_preload_native.gd').write_bytes((ROOT/extra).read_bytes());d['tools'][extra]=sha(ROOT/extra);write(root/'source.json',d);entries.append(dict(index=index,source=p['path'],files=len(d['files']),aliases=len(d['case_aliases'])))
 write(out/'preload-reference.json',dict(schema=1,commit=PIN,entries=entries,scene_admitted=False));return entries
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('out',type=Path);a=p.parse_args();print(json.dumps(prepare(a.out),indent=2))
