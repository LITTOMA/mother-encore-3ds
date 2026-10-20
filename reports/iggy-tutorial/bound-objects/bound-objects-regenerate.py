from pathlib import Path
import hashlib,json,subprocess,sys,time
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r));from tools import native_content as nc,native_session,native_restore,podunk_field as field
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
tracked=subprocess.check_output(['git','ls-files','content','romfs'],cwd=r,text=True).splitlines();before={n:sha(r/n)for n in tracked if(r/n).is_file()};rooms={n:nc.parse_pack((r/n).read_bytes())for n in tracked if n.endswith('.encroom')};steps=[]
for row in rooms.values():row.pop('build_input_sha256')
(r/'build/full-port/bound-generation-before.json').write_text(json.dumps(dict(head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=r,text=True).strip(),files=before,rooms=rooms),indent=2)+'\n',encoding='utf8')
# The optional owned-object adapter is not active in the published recipe.
# Check the original generator still produces the exact map and texture pages.
model,atlas,paths,outputs=field.build_outputs(field.load_recipe())
for path,blob in outputs.items():
 if path.endswith('.encmap'):assert blob==(r/'romfs'/path).read_bytes(),path
assert not model.object_bindings
def step(name,args):
 begin=time.time();log=r/('build/full-port/bound-regenerate-'+name+'.log')
 with log.open('wb')as f:p=subprocess.run([sys.executable,*args],cwd=r,stdout=f,stderr=subprocess.STDOUT)
 steps.append(dict(step=name,returncode=p.returncode,seconds=time.time()-begin));(r/'build/full-port/bound-regeneration.json').write_text(json.dumps(steps,indent=2)+'\n',encoding='utf8');print(name,p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.returncode)
step('props-assets',['tools/field_props.py','assets','--tex3ds','build/tooling/pinned-tex3ds/tex3ds','--tex3ds-command','py -3.11 build/tooling/run_tex3ds_wsl.py'])
for n in ['romfs/world/podunk/podunk.encprops','romfs/graphics/world/podunk/props/roots.t3x']:assert sha(r/n)==before[n],('Unexpected prop gameplay/texture change',n)
step('field',['tools/podunk_field.py','compile'])
for n,old in rooms.items():
 new=nc.parse_pack((r/n).read_bytes());new.pop('build_input_sha256');assert new==old,('Unexpected Room semantic change',n)
comparisons={}
for label,module,path in [('session',native_session,'content/native-session.json'),('restore',native_restore,'content/native-restore.json')]:
 old=json.loads((r/path).read_bytes());new=module.extract();changed={k:dict(before=old.get(k),after=new.get(k))for k in old.keys()|new.keys()if old.get(k)!=new.get(k)};comparisons[label]=changed;(r/'build/full-port/bound-rebind-comparison.json').write_text(json.dumps(comparisons,indent=2)+'\n',encoding='utf8');assert set(changed)<=({'dependencies'}if label=='session'else{'dependencies','fingerprints'}),('Unmapped semantic change',label)
 step(label+'-extract',['tools/native_'+label+'.py','extract']);step(label+'-compile',['tools/native_'+label+'.py','compile'])
step('new-game',['tools/new_game_assets.py','reuse-session','--base','a5b6600da1a8fa21a325a60d8f2553f57c350df8'])
step('stage',['tools/stage_native_romfs.py','--verify-only'])
changed=[n for n,digest in before.items()if sha(r/n)!=digest];proof=dict(scope='Original generators rebound only reviewed compiler identities; no formal map/object or quest activation. Actual tex3ds invoked; no AST readmission weakened.',changed=changed,unchanged=len(before)-len(changed),all_room_decoded_semantics_exact=True,published_map_exact=True,props_pack_and_texture_exact=True)
(r/'build/full-port/bound-regeneration-proof.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print(proof,flush=True)
