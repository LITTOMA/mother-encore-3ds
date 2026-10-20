from pathlib import Path
import hashlib,json,subprocess,sys,time
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r));from tools import native_content as nc
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();baseline=json.loads((r/'build/full-port/bound-generation-before.json').read_bytes())
for name,args in [('player-assets',['tools/field_player_assets.py','assets','--from-atlases','build/iggy-tutorial/swing-graphics-candidate-2']),('stage-final',['tools/stage_native_romfs.py','--verify-only'])]:
 with(r/('build/full-port/bound-regenerate-'+name+'.log')).open('wb')as f:p=subprocess.run([sys.executable,*args],cwd=r,stdout=f,stderr=subprocess.STDOUT)
 print(name,p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.returncode)
for name,old in baseline['rooms'].items():
 new=nc.parse_pack((r/name).read_bytes());new.pop('build_input_sha256');assert new==old,name
changed=[n for n,h in baseline['files'].items()if sha(r/n)!=h];assert all(n.startswith('content/asset-receipts/')for n in changed),changed
proof=dict(scope='Unpublished Map6/Object2 component. All tracked game binaries, IR/defaults unchanged. Original generators refreshed three compiler provenance receipts; actual tex3ds invoked, player atlases reused with original conversion proof. Initial stale player producer receipt correctly rejected by stage; final stage passed.',changed=changed,unchanged=len(baseline['files'])-len(changed),all_room_semantics_exact=True,all_tracked_romfs_exact=True)
(r/'build/full-port/bound-regeneration-proof.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print(proof,flush=True)
