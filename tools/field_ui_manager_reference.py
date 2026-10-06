#!/usr/bin/env python3
"""Original UiManager UI data-only reference; unique PNG case aliases recorded."""
from pathlib import Path
import json,shutil,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.scene_reference import quarantine,unquote,sha,prepare
from tools.upstream import git,safe_path,read_json,write_json

def prepare_ui(out,scene):
 inv=read_json(ROOT/'compatibility/upstream-inventory.json');lock=read_json(ROOT/'upstream.lock');source=ROOT/'upstream/MOTHER-Encore';indexed=inv['files'];out=Path(out).resolve()
 if not out.is_relative_to((ROOT/'build').resolve())or out.exists():raise ValueError('Fresh UI reference under build required')
 if git(source,'rev-parse','HEAD')!=lock['commit']or git(source,'status','--porcelain')or inv['commit']!=lock['commit']:raise ValueError('Pristine fixed source required')
 folded={}
 for name in indexed:folded.setdefault(name.casefold(),[]).append(name)
 files={};payloads={};aliases={};attachments=[];connections=[];pending=[scene]
 while pending:
  requested=pending.pop()
  canonical=requested
  if requested not in indexed:
   matches=folded.get(requested.casefold(),[])
   if len(matches)!=1 or Path(requested).suffix!='.png' or Path(matches[0]).suffix!='.png':raise ValueError('Unreviewed non-PNG/missing/ambiguous UI dependency '+requested)
   canonical=matches[0];aliases[requested]={'canonical':canonical,'sha256':indexed[canonical]['sha256'],'reason':'Original Windows case-insensitive PNG path resolves this single inventory file'}
  if canonical in files:
   if requested!=canonical and canonical in payloads:payloads[requested]=payloads[canonical]
   continue
  raw=safe_path(source,canonical).read_bytes()
  if sha(raw)!=indexed[canonical]['sha256']:raise ValueError('Changed UI source '+canonical)
  record={'sha256':sha(raw),'bytes':len(raw)};suffix=Path(canonical).suffix
  if suffix in ('.tscn','.tres'):
   transformed,refs,scripts,signals=quarantine(raw,canonical);record['external_resources']=refs;record['reference_sha256']=sha(transformed);attachments+=scripts;connections+=signals;pending.extend(r['path']for r in refs)
   fonts=[]
   for number,line in enumerate(transformed.decode().splitlines(),1):
    if line.startswith('font_path = '):
     value=unquote(line[len('font_path = '):])
     if not value.startswith('res://'):raise ValueError('Unknown UI font path')
     fonts.append({'path':value[6:],'line':number});pending.append(value[6:])
   record['native_font_payloads']=fonts;payloads[canonical]=transformed
  elif suffix=='.gd':record['quarantined']='not executed; source constructors/Ready need typed owner'
  elif suffix.lower()in('.png','.shader','.wav','.mp3','.ogg','.ttf','.otf'):payloads[canonical]=raw
  else:raise ValueError('Unknown UI source codec '+canonical)
  files[canonical]=record
  if requested!=canonical and canonical in payloads:payloads[requested]=payloads[canonical]
 document={'schema':1,'commit':lock['commit'],'game_version':lock['game_version'],'scene':scene,'scope':'Original full native UI subtree, no script execution/Ready','native_compatible':False,'files':files,'case_aliases':aliases,'script_attachments':attachments,'signal_connections':connections,'tools':{f:sha((ROOT/f).read_bytes())for f in ['tools/field_ui_manager_reference.py','tools/scene_reference.py','tools/godot_exporter/scene_data.gd','tools/godot_exporter/field_ui_manager.gd']}}
 out.mkdir(parents=True)
 for name,raw in payloads.items():
  p=safe_path(out,name);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(raw)
 (out/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Source-only UiManager reference"\n[logging]\nfile_logging/enable_logging=false\n[editor_plugins]\nenabled=PoolStringArray("res://addons/encore_import/plugin.cfg")\n')
 plugin=out/'addons/encore_import';plugin.mkdir(parents=True);(plugin/'plugin.cfg').write_text('[plugin]\nname="Encore import completion"\ndescription="Bounded resource scan"\nauthor="Encore Native"\nversion="1"\nscript="import_plugin.gd"\n')
 shutil.copyfile(ROOT/'tools/godot_exporter/import_plugin.gd',plugin/'import_plugin.gd');shutil.copyfile(ROOT/'tools/godot_exporter/scene_data.gd',out/'scene_data.gd');shutil.copyfile(ROOT/'tools/godot_exporter/field_ui_manager.gd',out/'field_ui_manager.gd');write_json(out/'source.json',document);return document
if __name__=='__main__':
 if len(sys.argv)!=3:raise SystemExit('usage: field_ui_manager_reference.py OUT SCENE')
 d=prepare_ui(Path(sys.argv[1]),sys.argv[2]);print(len(d['files']),'full original source files;',len(d['case_aliases']),'explicit PNG aliases')
