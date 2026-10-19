from pathlib import Path
import hashlib, json, shutil, subprocess, sys
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r))
from tools import native_content as nc
folder=r/'reports/iggy-tutorial/formal-object-drawing';folder.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
base=json.loads((r/'build/full-port/object-activation-before.json').read_bytes())
changed={n:dict(before=old,after=sha(r/n))for n,old in base['files'].items()if sha(r/n)!=old}
rooms={}
for n,old in base['rooms'].items():
    new=nc.parse_pack((r/n).read_bytes());identity=new.pop('build_input_sha256');assert new==old,n
    canonical=json.dumps(new,sort_keys=True,separators=(',',':')).encode()
    rooms[n]=dict(all_decoded_content_unchanged=True,canonical_sha256=hashlib.sha256(canonical).hexdigest(),new_build_input_sha256=identity)
sources={}
for line in subprocess.check_output(['git','status','--porcelain','-uall'],cwd=r).decode('utf8').splitlines():
    n=line[3:]
    if n.startswith(('reports/','content/asset-receipts/graphics/world/mick/','content/asset-receipts/graphics/world/presents/','romfs/graphics/world/mick/','romfs/graphics/world/presents/')):continue
    p=r/n
    if p.is_file():sources[n]=sha(p)
files=[]
for pattern in ('object-*.log','scene-compile-*.log','scene-result-*.log'):
    files.extend((r/'build/full-port').glob(pattern))
for name in ('object-scene-host.py','object-art-host.py','object-links-host.py','object-integrated-host.py','object-integrated-host-final.py','object-integrated-host-final-3.py','object-scene-compile-core.py','object-links-compile-core.py','generate-field-object-release.py','finalize-field-object-release.py','regenerate-object-source-endings.py','check-field-diagnostic.py','object-lf-generation.json','object-methods-final.json','object-release-dependent-candidates.json','object-final-restore-comparison.json'):
    p=r/'build/full-port'/name
    if p.is_file():files.append(p)
for p in files:shutil.copyfile(p,folder/p.name)
shutil.copyfile(r/'build/podunk-routes/host/source.json',folder/'host-core-source.json')
shutil.copyfile(r/'build/tooling/run_tex3ds_wsl.py',folder/'local-tex3ds-transport.py')
shutil.copyfile(r/'build/full-port/object-converter-wrapper-before.py',folder/'local-tex3ds-transport-before.py')
older=folder/'older-ci';older.mkdir(exist_ok=True)
for n in ('aaff-gcc.log','a5b-gcc.api.log','3b-clang.api.log','3b-console.api.log'):
    p=r/'build/full-port/older-ci-terminal'/n
    assert p.stat().st_size>10000,n
    shutil.copyfile(p,older/n)
proof=dict(baseline_head=base['head'],scope='Formal source ready/drawing/deletion resources only; Root contact, audio callback, full getshovel and manual pickup remain unavailable',changed_preexisting_tracked_resources=changed,unchanged_preexisting_tracked_resources=sum(1 for n,v in base['files'].items()if sha(r/n)==v),rooms=rooms,current_source_sha256=sources,actual_texture=dict(bytes=(r/'romfs/graphics/world/podunk/objects/atlas-00.t3x').stat().st_size,sha256=sha(r/'romfs/graphics/world/podunk/objects/atlas-00.t3x')),limits=['Host core logs are actual GCC9.4 ASan/UBSan with leak checking; Windows configure/collection only','Scene and loader test payloads are explicitly synthetic, not GPU images','Actual art test uses real converted T3X, not a GPU/emulator/hardware result','No current-slice cross-build, emulator, natural-opening, full-game or hardware acceptance yet','Initial map-converter stderr was overwritten by retry; driver returncode and observed error only, not retained original stderr'])
(folder/'source-proof.json').write_text(json.dumps(proof,indent=2,ensure_ascii=False)+'\n',encoding='utf8')
retired=[]
for p in sorted((r/'build/full-port').glob('scene-regression-*')):
    assert p.is_file() and p.resolve().parent==(r/'build/full-port').resolve() and p.read_bytes()[:4]==b'\x7fELF',p
    result=folder/('scene-result-'+p.name.removeprefix('scene-regression-')+'.log')
    assert result.is_file() and result.stat().st_size>0,result
    retired.append(dict(path=p.relative_to(r).as_posix(),bytes=p.stat().st_size,sha256=sha(p),retained_result=result.relative_to(r).as_posix()))
(folder/'owned-elf-retirement.json').write_text(json.dumps(dict(owned_files=retired,total_bytes=sum(v['bytes']for v in retired),keep='Exact core object cache, accepted f029 game, recorded save, converters and source/reference evidence remain'),indent=2)+'\n',encoding='utf8')
for row in retired:(r/row['path']).unlink()
manifest={str(p.relative_to(folder)):dict(bytes=p.stat().st_size,sha256=sha(p))for p in folder.rglob('*')if p.is_file()and p.name!='retained-files.json'}
(folder/'retained-files.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf8')
print('Retained',len(manifest),'files;',len(changed),'changed /',proof['unchanged_preexisting_tracked_resources'],'unchanged prior tracked resources; Room content identical; retired',sum(v['bytes']for v in retired),'owned ELF bytes',flush=True)
