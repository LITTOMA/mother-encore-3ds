#!/usr/bin/env python3
"""Original distortionator data-resource importer quarantine, no game Ready."""
from pathlib import Path
import json,sys,shutil
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,sha,read,write,require
from tools.upstream import git

def prepare(out):
 inv=read(ROOT/'compatibility/upstream-inventory.json');source=ROOT/'upstream/MOTHER-Encore';require(inv['commit']==PIN and git(source,'rev-parse','HEAD')==PIN and not git(source,'status','--porcelain'),'Pristine fixed source required');out=Path(out).resolve();require(out.is_relative_to((ROOT/'build').resolve())and not out.exists(),'Fresh source BG quarantine project required');files={}
 wanted=[f for f in inv['files']if f.startswith(('Graphics/Battle BGS/','addons/distortionator_integration/'))];require(sum(f.endswith('.bbg')for f in wanted)==50,'Original fifty background resources required');out.mkdir(parents=True)
 for name in wanted:
  raw=(source/name).read_bytes();require(sha(source/name)==inv['files'][name]['sha256'],'Changed original BG dependency '+name);dest=out/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(raw);files[name]=dict(sha256=inv['files'][name]['sha256'],bytes=len(raw))
 for name in ['tools/godot_exporter/scene_data.gd','tools/godot_exporter/field_battle_bg_resources.gd']:
  shutil.copyfile(ROOT/name,out/Path(name).name)
 p=out/'addons/encore_import';p.mkdir(parents=True);(p/'plugin.cfg').write_text('[plugin]\nname="Encore resource import completion"\ndescription="Bounded source resource scan"\nauthor="Encore Native"\nversion="1"\nscript="import_plugin.gd"\n');shutil.copyfile(ROOT/'tools/godot_exporter/field_battle_bg_import.gd',p/'import_plugin.gd')
 (out/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Original battle resource reference"\n[logging]\nfile_logging/enable_logging=false\n[editor_plugins]\nenabled=PoolStringArray("res://addons/encore_import/plugin.cfg")\n')
 write(out/'source.json',dict(schema=1,commit=PIN,files=files,scope='Original fixed resource importer executes only ConfigFile/resource construction and PackedScene.pack; no scene enters Tree, no game scripts/Ready/process/animations execute',tools={name:sha(ROOT/name)for name in ['tools/field_battle_bg_reference.py','tools/godot_exporter/scene_data.gd','tools/godot_exporter/field_battle_bg_resources.gd','tools/godot_exporter/field_battle_bg_import.gd']}));return files
if __name__=='__main__':
 require(len(sys.argv)==2,'OUT under build required');print('Original BG source resource reference:',len(prepare(Path(sys.argv[1]))),'files')
