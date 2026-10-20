from pathlib import Path
import hashlib,json,subprocess,sys,time
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r))
from tools import native_content as nc,native_session,native_restore
out=r/'build/full-port/root-formal-generation-1';assert not out.exists();out.mkdir()
tracked=subprocess.check_output(['git','ls-files','content','romfs'],cwd=r,text=True).splitlines();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
before={n:sha(r/n)for n in tracked if(r/n).is_file()};rooms={n:nc.parse_pack((r/n).read_bytes())for n in tracked if n.endswith('.encroom')};(out/'before.json').write_text(json.dumps(dict(head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=r,text=True).strip(),files=before,rooms=rooms),indent=2)+'\n',encoding='utf8')
steps=[]
def run(name,args):
 start=time.monotonic()
 with(out/(name+'.log')).open('wb')as log:p=subprocess.run([sys.executable,*args],cwd=r,stdout=log,stderr=subprocess.STDOUT)
 steps.append(dict(step=name,args=args,returncode=p.returncode,seconds=time.monotonic()-start));(out/'steps.json').write_text(json.dumps(steps,indent=2)+'\n');print(name,p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.returncode)
tool=['--tex3ds','build/tooling/pinned-tex3ds/tex3ds','--tex3ds-command','py -3.11 build/tooling/run_tex3ds_wsl.py']
run('props',['tools/field_props.py','assets',*tool]);run('objects',['tools/field_objects_assets.py','assets',*tool]);run('player',['tools/field_player_assets.py','assets','--from-atlases','build/iggy-tutorial/swing-graphics-candidate-2']);run('field',['tools/podunk_field.py','compile',*tool]);run('story',['tools/field_story.py','compile'])
for name in ('romfs/world/house/opening.encroom','romfs/world/mayor/mayor.encroom'):
 assert sha(r/name)==before[name],('Unrelated Room changed',name)
for name in ('romfs/world/podunk/podunk.encmap','romfs/world/podunk/podunk.encobjects','romfs/graphics/world/podunk/objects/atlas-00.t3x','romfs/graphics/world/podunk/props/roots.t3x'):
 assert sha(r/name)==before[name],('Unrelated map/ready/texture changed',name)
old=rooms['romfs/world/podunk/podunk.encroom'];new=nc.parse_pack((r/'romfs/world/podunk/podunk.encroom').read_bytes());assert old['capabilities']==13 and new['capabilities']==14 and len(old['sections']['Program'])==7 and len(new['sections']['Program'])==8
assert new['strings'][:len(old['strings'])]==old['strings']
for key in old['sections']:
 if key in ('Program','Command'):continue
 assert new['sections'][key][:len(old['sections'][key])]==old['sections'][key],('Changed previous Room record',key)
assert new['sections']['Program'][:6]==old['sections']['Program'][:6]
stop=old['sections']['Program'][6]['first_command'];assert new['sections']['Command'][:stop]==old['sections']['Command'][:stop]
changes={}
for label,module,name in [('session',native_session,'content/native-session.json'),('restore',native_restore,'content/native-restore.json')]:
 old=json.loads((r/name).read_bytes());new=module.extract();changes[label]={k:dict(before=old.get(k),after=new.get(k))for k in old.keys()|new.keys()if old.get(k)!=new.get(k)}
(out/'dependent-candidates.json').write_text(json.dumps(changes,indent=2)+'\n',encoding='utf8')
(out/'proof.json').write_text(json.dumps(dict(scope='Actual source generators; Room14 getshovel append and four tutorial idle yields. House/Mayor Room bytes, full field map, ready object packet and both object/Root textures unchanged. Session/restore candidates only, pending semantic review.',changed=[n for n,v in before.items()if sha(r/n)!=v],dependent_keys={k:list(v)for k,v in changes.items()}),indent=2)+'\n',encoding='utf8');print('Real generators completed; dependent candidates:',{k:list(v)for k,v in changes.items()},flush=True)
