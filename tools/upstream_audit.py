#!/usr/bin/env python3
"""Complete byte inventory and static LOWER BOUND of Encore's required capabilities.

This diagnostic scanner neither parses Godot semantics nor approves native bindings.
Each finding retains a source location; dynamic sites remain unresolved review work.
"""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
from pathlib import Path
import re
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.upstream import snapshot, read_json, write_json, safe_path

TEXT_SUFFIXES={'.gd','.ecs','.tscn','.tres','.shader','.gdshader','.godot','.cfg','.import','.bbg','.mbg','.yaml','.yml'}
CAPABILITIES={
    'dynamic_resource_loading':r'\b(?:load|preload|ResourceLoader\.load|load_interactive)\s*\(',
    'reflection_or_dynamic_call':r'\b(?:call|call_deferred|callv|funcref|set|get|has_method)\s*\(',
    'signals':r'\b(?:signal\s+\w+|connect\s*\(|emit_signal\s*\()',
    'suspended_execution':r'\b(?:yield|create_timer)\s*\(',
    'tween':r'\b(?:Tween|create_tween|tween_property|interpolate_property)\b',
    'animation_tracks':r'\b(?:AnimationPlayer|AnimationTree)\b|^tracks/\d+/',
    'tile_maps_and_scene_tiles':r'\b(?:TileMap|TileSet|SceneTileMap|SceneTileset)\b',
    'save_and_file_io':r'\b(?:File|Directory|ConfigFile|SaveManager)\b|user://',
    'input':r'\b(?:Input|InputMap|InputEvent\w*|is_action_\w+)\b',
    'audio':r'\b(?:AudioStream\w*|AudioServer|audioManager)\b',
    'shader_material':r'\b(?:ShaderMaterial|Shader|shader_type|shader_param)\b',
    'randomness':r'\b(?:randf|randi|randomize|seed|rand_range)\s*\(',
    'scene_instancing':r'\b(?:instance|PackedScene)\b|^\[node .*instance=',
}

def scan_text(path: str,text: str)->dict:
    """Line diagnostics only. Comments/string hits may overcount; no execution claims."""
    nodes=[];resources=[];classes=[];extends=[];connections=[];findings=defaultdict(list);references=[]
    for line_no,line in enumerate(text.splitlines(),1):
        location={'path':path,'line':line_no}
        if line.startswith('[node '):
            attrs=dict(re.findall(r'(name|type|parent)="([^"\r\n]*)"',line))
            nodes.append(dict(location,attributes=attrs,instance='instance=' in line))
        if line.startswith('[sub_resource '):
            found=re.search(r' type="([^"\r\n]+)"',line)
            if found:resources.append(dict(location,type=found[1]))
        if line.startswith('[connection '):connections.append(location)
        if path.endswith('.gd'):
            match=re.match(r'^class_name\s+(\w+)',line)
            if match:classes.append(dict(location,name=match[1]))
            match=re.match(r'^extends\s+(.+)',line)
            if match:extends.append(dict(location,target=match[1]))
        for ref in re.findall(r'["\'](res://[^"\'\r\n]*)["\']',line):
            references.append(dict(location,reference=ref))
        for capability,pattern in CAPABILITIES.items():
            if re.search(pattern,line):findings[capability].append(dict(location,excerpt=line.strip()[:240]))
    return {'nodes':nodes,'resources':resources,'classes':classes,'extends':extends,
            'connections':connections,'references':references,'capabilities':dict(findings)}

def audit(root: Path,lock: dict)->dict:
    if lock.get('schema')!=1:raise ValueError('Unsupported upstream lock schema')
    inv=snapshot(root)
    if not lock.get('commit') or inv['commit']!=lock['commit'] or inv['tree']!=lock.get('tree'):
        raise ValueError('Audit requires the pristine pinned commit and tree')
    nodes=[];resources=[];classes=[];extends=[];connections=[];references=[];caps=defaultdict(list)
    text_errors=[]
    for path in inv['files']:
        if Path(path).suffix.lower() not in TEXT_SUFFIXES:continue
        try:text=safe_path(root,path).read_text(encoding='utf-8')
        except UnicodeError:
            text_errors.append({'path':path,'reason':'non_utf8_text_requires_review'});continue
        result=scan_text(path,text)
        nodes.extend(result['nodes']);resources.extend(result['resources']);classes.extend(result['classes'])
        extends.extend(result['extends']);connections.extend(result['connections']);references.extend(result['references'])
        for kind,locations in result['capabilities'].items():caps[kind].extend(locations)
    unresolved=[]
    for ref in references:
        name=ref['reference'][6:]
        if name in inv['files']:continue
        # Directory prefixes and interpolated strings are not missing files;
        # record uncertainty instead of deleting them from the dependency graph.
        try:target=safe_path(root,name)
        except ValueError:
            unresolved.append(dict(ref,reason='unsafe_or_external_resource_reference'));continue
        reason=('generated_import_reference' if name.startswith(('.import/','.godot/imported/')) or name.endswith('.translation')
                else 'directory_or_constructed_reference' if target.is_dir() or '%' in name
                else 'missing_or_constructed_reference')
        unresolved.append(dict(ref,reason=reason))
    licences={path:record['sha256'] for path,record in inv['files'].items()
              if re.search(r'(license|copying|notice)',Path(path).name,re.I)}
    fonts=[path for path,record in inv['files'].items() if record['kind']=='font']
    plugins=[path for path in inv['files'] if path.startswith('addons/') and path.endswith('/plugin.cfg')]
    return {'schema':1,'commit':inv['commit'],'tree':inv['tree'],'game_version':lock.get('game_version'),
        'claim':'static diagnostic lower bound, NOT complete semantics or compatibility approval',
        'native_compatible':False,'inventory_coverage':inv['coverage'],
        'file_count':len(inv['files']),'total_bytes':sum(r['bytes'] for r in inv['files'].values()),
        'files_by_kind':dict(sorted(Counter(r['kind'] for r in inv['files'].values()).items())),
        'files_by_suffix':dict(sorted(Counter(Path(p).suffix.lower() for p in inv['files']).items())),
        'unclassified_files':[p for p,r in inv['files'].items() if r['kind']=='other'],
        'node_type_counts':dict(Counter(n['attributes'].get('type','inherited_or_instance') for n in nodes)),
        'node_locations':nodes,'subresource_locations':resources,'script_classes':classes,'script_extends':extends,
        'serialized_connections':connections,'capabilities':dict(caps),'unresolved_references':unresolved,
        'text_errors':text_errors,'licence_files':licences,'font_files_requiring_individual_review':fonts,
        'plugin_manifests':plugins,'external_dependencies':'not yet proven complete; inspect dynamic sites and reference runtime',
        'review_status':'all files require explicit binding; empty binding registry remains blocked'}

def main()->int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--root',type=Path,default=ROOT/'upstream/MOTHER-Encore')
    ap.add_argument('--lock',type=Path,default=ROOT/'upstream.lock')
    ap.add_argument('--out',type=Path,default=ROOT/'reports/upstream-audit.json')
    args=ap.parse_args()
    try:
        result=audit(args.root,read_json(args.lock));write_json(args.out,result)
        print(f"Inventoried {result['file_count']} blobs / {result['total_bytes']} bytes; "
              f"{len(result['unresolved_references'])} unresolved literal sites. Compatibility remains blocked.")
        return 0
    except (OSError,ValueError,KeyError) as error:
        print(f'AUDIT ERROR: {error}',file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
