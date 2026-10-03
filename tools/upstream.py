#!/usr/bin/env python3
"""Explicitly fetch/pin upstream, inventory bytes, compare snapshots and fail closed.
No GDScript execution. Function-name discovery is diagnostic only: full-file SHA-256
and declared dependency hashes decide whether native bindings require review.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.content_compiler import compile_content
SKIP={'.git','__pycache__'}

def digest(data: bytes)->str:return hashlib.sha256(data).hexdigest()
def read_json(path: Path):return json.loads(path.read_text(encoding='utf-8'))
def write_json(path: Path,value)->None:
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(value,indent=2,sort_keys=True,ensure_ascii=False)+'\n',encoding='utf-8')
def git(root: Path,*args)->str:
    return subprocess.check_output(['git','-C',str(root),*args],text=True,stderr=subprocess.PIPE).strip()
def safe_path(root: Path,name: str)->Path:
    p=Path(name)
    if p.is_absolute() or '..' in p.parts:raise ValueError(f'Unsafe relative path: {name}')
    target=root/p
    if not target.resolve().is_relative_to(root.resolve()):raise ValueError(f'Path escapes root: {name}')
    return target

def classify(path: str)->str:
    name=Path(path).name.lower()
    if name in ('license','license.txt'):return 'license'
    if name=='.gdignore':return 'editor_ignore'
    if name=='.gitignore':return 'source_control_metadata'
    if path.startswith('Fonts/fontsource/') and name.endswith('.txt'):return 'font_source'
    if path.startswith('Translations/Tools/m1 dumps/'):return 'translation_source_requires_rights_review'
    extension=Path(path).suffix.lower()
    return {'.gd':'gdscript','.ecs':'encorescript','.tscn':'scene','.scn':'scene',
      '.tres':'resource','.res':'resource','.shader':'shader','.gdshader':'shader',
      '.yaml':'data','.yml':'data','.json':'data','.csv':'data','.png':'texture','.jpg':'texture',
      '.ogg':'audio','.wav':'audio','.mp3':'audio','.m4a':'audio',
      '.ttf':'font','.otf':'font','.import':'import_metadata','.translation':'translation',
      '.bbg':'distortionator','.mbg':'battle_background','.godot':'project',
      '.cfg':'configuration','.exe':'development_binary',
      '.aseprite':'art_source','.kra':'art_source','.bmp':'texture','.gif':'texture',
      '.tsx':'tiled_source','.tmx':'tiled_source','.txt':'documentation','.md':'documentation',
      '.py':'development_script','.sh':'development_script','.dat':'credits_data',
      '.vs':'visual_script_requires_review','.sson':'editor_data_requires_review',
      '.icns':'icon','.ico':'icon'}.get(extension,'other')

def tracked_files(root: Path):
    """Inventory every Git blob, including tracked editor/cache directories.

    A source pin must describe pristine bytes. Non-Git fixtures still work, but
    cannot claim a commit. Refuse special Git modes and files outside the index.
    """
    try:
        top=Path(git(root,'rev-parse','--show-toplevel')).resolve()
    except (OSError,subprocess.CalledProcessError):
        return None,None,None
    if top!=root:return None,None,None
    if git(root,'status','--porcelain'):
        raise ValueError('Dirty upstream checkout cannot claim a source commit')
    extra=subprocess.check_output(['git','-C',str(root),'ls-files','--others','-z'])
    if extra:raise ValueError('Untracked/ignored files require explicit handling in upstream checkout')
    raw=subprocess.check_output(['git','-C',str(root),'ls-tree','-rz','--full-tree','HEAD'])
    paths=[]
    for record in raw.split(b'\0'):
        if not record:continue
        meta,name=record.split(b'\t',1)
        mode,kind,_=meta.decode('ascii').split()
        if mode not in ('100644','100755') or kind!='blob':
            raise ValueError('Unsupported Git entry: '+name.decode('utf-8'))
        paths.append(safe_path(root,name.decode('utf-8')))
    return paths,git(root,'rev-parse','HEAD'),git(root,'rev-parse','HEAD^{tree}')

def snapshot(root: Path)->dict:
    root=root.resolve()
    if not root.is_dir():raise ValueError(f'Not a directory: {root}')
    paths,commit,tree=tracked_files(root)
    tracked=paths is not None
    if paths is None:paths=sorted(root.rglob('*'))
    files={}
    for path in sorted(paths):
        rel=path.relative_to(root)
        if not tracked and any(part in SKIP for part in rel.parts):continue
        if path.is_symlink():raise ValueError(f'Symlink requires explicit handling: {rel}')
        if not path.is_file():continue
        raw=path.read_bytes();entry={'sha256':digest(raw),'bytes':len(raw),'kind':classify(rel.as_posix())}
        if raw.startswith(b'version https://git-lfs.github.com/spec/v1\n'):entry['kind']='lfs_pointer_requires_fetch'
        if entry['kind']=='other' and raw.startswith(b'\x89PNG\r\n\x1a\n'):
            entry['kind']='texture';entry['classification_note']='PNG signature; extensionless source requires explicit path binding'
        if path.suffix.lower() in ('.gd','.ecs','.tscn','.tres','.shader','.gdshader'):
            text=raw.decode('utf-8')
            # Quoted references may contain spaces. This is a static diagnostic
            # lower bound, not a GDScript/resource parser or dependency closure.
            entry['res_references']=sorted(set(re.findall(r'["\'](res://[^"\'\r\n]*)["\']',text)))
            if entry['kind']=='gdscript':entry['functions']=re.findall(r'^\s*(?:static\s+)?func\s+(\w+)\s*\(',text,re.M)
        files[rel.as_posix()]=entry
    return {'schema':1,'commit':commit,'tree':tree,'coverage':'all_tracked_git_blobs' if tracked else 'directory_without_commit',
            'reference_coverage':'quoted literal diagnostic only; dynamic references require review','files':files}

def pin_existing(ref: str,checkout: Path,lock_path: Path,version: str)->None:
    """Pin an already acquired pristine checkout; never infer release identity."""
    if not re.fullmatch('[0-9a-f]{40}',ref):raise ValueError('Pin requires a full commit ID')
    lock=read_json(lock_path)
    if git(checkout,'remote','get-url','origin').rstrip('/')!=lock['repository'].rstrip('/'):
        raise ValueError('Existing upstream remote differs from lock file')
    inventory=snapshot(checkout)
    if inventory['commit']!=ref:raise ValueError('Requested pin differs from checkout HEAD')
    source=(checkout/'Scripts/global/global.gd').read_text(encoding='utf-8')
    versions=re.findall(r'^const GAME_VERSION := "([0-9.]+)"$',source,re.M)
    if versions!=[version]:raise ValueError('Release version differs from upstream GAME_VERSION')
    lock.update({'commit':ref,'tree':inventory['tree'],'requested_ref':ref,'game_version':version,
                 'status':'pinned; compatibility NOT yet established'})
    write_json(lock_path,lock)
    print(f'Pinned pristine source {ref}, declared game version {version}; no compatibility approval.')

def diff_snapshots(base: dict,candidate: dict)->dict:
    old,new=base['files'],candidate['files']
    added=sorted(set(new)-set(old));removed=sorted(set(old)-set(new))
    changed=sorted(p for p in set(old)&set(new) if old[p]['sha256']!=new[p]['sha256'])
    return {'base_commit':base.get('commit'),'candidate_commit':candidate.get('commit'),
            'added':added,'changed':changed,'removed':removed,
            'changed_by_kind':{kind:[p for p in added+changed if new[p]['kind']==kind]
                for kind in sorted(set(new[p]['kind'] for p in added+changed))}}

def gate(base: dict,candidate: dict,registry: dict,candidate_root: Path,project_root: Path=ROOT)->dict:
    if registry.get('schema')!=1:raise ValueError('Unsupported bindings schema')
    changes=diff_snapshots(base,candidate);problems=[];automatic=[]
    bindings=registry.get('bindings',{});files=candidate['files']
    for path,record in files.items():
        binding=bindings.get(path)
        if not binding:
            problems.append({'path':path,'reason':'unmapped_source','kind':record['kind']});continue
        actual=safe_path(candidate_root,path)
        if not actual.is_file() or digest(actual.read_bytes())!=record['sha256']:
            problems.append({'path':path,'reason':'snapshot_does_not_match_checkout'});continue
        kind=binding.get('kind')
        if kind=='native':
            if binding.get('reviewed_sha256')!=record['sha256']:
                problems.append({'path':path,'reason':'native_source_changed_or_unreviewed'})
            implementations=binding.get('native_files',[])
            if not implementations or any(not safe_path(project_root,p).is_file() for p in implementations):
                problems.append({'path':path,'reason':'missing_native_implementation'})
            if not binding.get('tests'):
                problems.append({'path':path,'reason':'missing_test_mapping'})
            for dependency,expected in binding.get('dependencies',{}).items():
                if dependency not in files or files[dependency]['sha256']!=expected:
                    problems.append({'path':path,'reason':'native_dependency_changed','dependency':dependency})
        elif kind=='exported':
            # Only the fixture adapter is implemented in M0. Never guess a real game adapter.
            if binding.get('exporter')!='sandbox-json-v1':
                problems.append({'path':path,'reason':'exporter_not_implemented'});continue
            try:compile_content(read_json(actual))
            except (ValueError,TypeError,KeyError) as e:
                problems.append({'path':path,'reason':'import_validation_failed','detail':str(e)});continue
            if path in changes['added'] or path in changes['changed']:automatic.append(path)
        elif kind=='ignored':
            if not binding.get('reason') or binding.get('reviewed_sha256')!=record['sha256']:
                problems.append({'path':path,'reason':'ignored_source_requires_review'})
        else:problems.append({'path':path,'reason':'invalid_binding_kind'})
    removals=registry.get('reviewed_removals',{})
    for path in changes['removed']:
        approval=removals.get(path,{})
        if approval.get('old_sha256')!=base['files'][path]['sha256'] or not approval.get('reason'):
            problems.append({'path':path,'reason':'removed_source_requires_migration_review'})
    return {'schema':1,'passed':not problems,'claim':'static import/review gate only; behavioral tests still required',
            'changes':changes,'automatic_content_updates':automatic,'problems':problems}

def fetch(ref: str,checkout: Path,lock_path: Path)->None:
    lock=read_json(lock_path);url=lock['repository']
    if checkout.exists():
        if not (checkout/'.git').exists():raise ValueError('Existing checkout path is not a Git repository')
        if git(checkout,'status','--porcelain'):raise ValueError('Upstream checkout is dirty; refusing to overwrite work')
        if git(checkout,'remote','get-url','origin').rstrip('/')!=url.rstrip('/'):
            raise ValueError('Existing upstream remote differs from lock file')
    else:
        checkout.parent.mkdir(parents=True,exist_ok=True)
        subprocess.run(['git','clone','--no-checkout','--filter=blob:none',url,str(checkout)],check=True)
    subprocess.run(['git','-C',str(checkout),'fetch','--depth=1','origin',ref],check=True)
    resolved=git(checkout,'rev-parse','FETCH_HEAD')
    if not re.fullmatch('[0-9a-f]{40}',resolved):raise ValueError('Cannot resolve full commit ID')
    subprocess.run(['git','-C',str(checkout),'checkout','--detach',resolved],check=True)
    # A different ref must not inherit a previously observed release identity.
    for key in ('game_version','release_evidence'):lock.pop(key,None)
    lock.update({'commit':resolved,'tree':git(checkout,'rev-parse','HEAD^{tree}'),
                 'requested_ref':ref,'status':'pinned; compatibility NOT yet established'})
    write_json(lock_path,lock);print(f'Pinned {resolved}. This does not approve any game content.')

def main()->int:
    ap=argparse.ArgumentParser(description=__doc__);sub=ap.add_subparsers(dest='action',required=True)
    p=sub.add_parser('fetch');p.add_argument('--ref',required=True);p.add_argument('--checkout',type=Path,default=ROOT/'upstream/MOTHER-Encore');p.add_argument('--lock',type=Path,default=ROOT/'upstream.lock')
    p=sub.add_parser('pin-existing');p.add_argument('--ref',required=True);p.add_argument('--version',required=True);p.add_argument('--checkout',type=Path,required=True);p.add_argument('--lock',type=Path,default=ROOT/'upstream.lock')
    p=sub.add_parser('snapshot');p.add_argument('--root',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p=sub.add_parser('diff');p.add_argument('--base',type=Path,required=True);p.add_argument('--candidate',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p=sub.add_parser('gate');p.add_argument('--base',type=Path,required=True);p.add_argument('--candidate',type=Path,required=True);p.add_argument('--bindings',type=Path,default=ROOT/'compatibility/bindings.json');p.add_argument('--candidate-root',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    args=ap.parse_args()
    try:
        if args.action=='fetch':fetch(args.ref,args.checkout,args.lock);return 0
        if args.action=='pin-existing':pin_existing(args.ref,args.checkout,args.lock,args.version);return 0
        if args.action=='snapshot':write_json(args.out,snapshot(args.root));return 0
        if args.action=='diff':write_json(args.out,diff_snapshots(read_json(args.base),read_json(args.candidate)));return 0
        report=gate(read_json(args.base),read_json(args.candidate),read_json(args.bindings),args.candidate_root)
        write_json(args.out,report)
        print(f"Gate {'PASS' if report['passed'] else 'BLOCKED'}: {len(report['problems'])} review items")
        return 0 if report['passed'] else 2
    except (OSError,ValueError,KeyError,subprocess.CalledProcessError) as error:
        print(f'UPSTREAM ERROR: {error}',file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
