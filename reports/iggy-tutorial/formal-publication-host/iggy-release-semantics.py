from pathlib import Path
import json,hashlib,struct,subprocess,sys
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r))
from tools import native_content as nc,podunk_field as f
base='7d2e552f86c9ef0c995c85c301e3a2f26a78f32a';old=lambda name:subprocess.check_output(['git','show',base+':'+name],cwd=r)
name='romfs/world/podunk/podunk.encroom';a=nc.parse_pack(old(name));b=nc.parse_pack((r/name).read_bytes());assert(a['rules'],a['capabilities'],b['rules'],b['capabilities'])==(7,12,7,13)
assert b['strings'][:len(a['strings'])]==a['strings']
for key,rows in a['sections'].items():assert b['sections'][key][:len(rows)]==rows,key
def story(blob):
 _,ss=f.unpack_container(blob,b'ENCFST01',(1,24,36,40,12,32,1),128*1024,capability=4);pool=ss[0]
 def text(v):return pool[v[0]:v[0]+v[1]].decode()
 out=[]
 for section,fmt,refs in [(1,'<6I',(1,3)),(2,'<9I',(1,3,5,7)),(3,'<8IfI',(2,4)),(4,'<3I',(1,)),(5,'<8I',(2,6))]:
  rows=[]
  for v in struct.iter_unpack(fmt,ss[section]):
   v=list(v)
   for i in refs:v[i]=text(v[i:i+2]);v[i+1]=None
   rows.append(v)
  out.append(rows)
 return out
name='romfs/world/podunk/story.encstory';sa=story(old(name));sb=story((r/name).read_bytes())
for oldrows,newrows in zip(sa,sb):assert newrows[:len(oldrows)]==oldrows
assert len(sb[0])==7 and len(sb[1])==56
before=json.loads(old('content/native-session.json'));after=json.loads((r/'content/native-session.json').read_bytes());assert before['defaults']==after['defaults'];assert before['compatibility']==after['compatibility']
assert set(after['mutable_flags'])-set(before['mutable_flags'])=={'bat'}
assert after['key_acquisitions'][:-1]==before['key_acquisitions'];assert after['key_acquisitions'][-1]==dict(item_id='BatOld',doses=1,flag_id='bat',consumed_flag_id='',flag_policy=1)
assert all((r/path).read_bytes()==old(path)for path in ['romfs/world/podunk/podunk.encmap','romfs/world/podunk/podunk.encprops','romfs/ui/opening.encnewgame','romfs/session/opening.encrestore'])
proof=dict(base=base,room='All old table rows and strings preserved as prefixes; rules7 unchanged, cap12->13 only for actual GrantKeyItem48',story='Old six programmes,28 locale rows,encounters,overrides,NPC interactions semantically unchanged; append programme6 and28locale rows',session='Resource v11/save schema and defaults unchanged; only bat added to mutable flags and BatOld completion policy appended',unchanged='Actual map, props, naming and restore bytes equal baseline',outputs={str(p.relative_to(r)):dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())for p in [r/'romfs/world/podunk/podunk.encroom',r/'romfs/world/podunk/story.encstory',r/'romfs/session/opening.encsession']})
(r/'build/full-port/iggy-release-semantics.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print(json.dumps(proof,ensure_ascii=False,indent=2))
