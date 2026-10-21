from pathlib import Path,PurePosixPath
import hashlib,json,os,shutil,subprocess
r=Path.cwd();source=r/'upstream/MOTHER-Encore';out=r/'build/full-port/full-original-oracle-1';pin='7d9246600fffe518408f5830d4848635019005a3';env=dict(os.environ,GIT_OPTIONAL_LOCKS='0');git=lambda *a:subprocess.check_output(['git','-C',str(source),*a],env=env)
assert git('rev-parse','HEAD').decode().strip()==pin
rows=[]
for record in git('ls-tree','-rzl','HEAD').split(b'\0'):
 if not record:continue
 header,name=record.split(b'\t',1);mode,kind,blob,size=header.split();path=PurePosixPath(name.decode());assert mode in[b'100644',b'100755']and kind==b'blob'and not path.is_absolute()and all(p not in['.','..']for p in path.parts);rows.append((path,int(size),blob.decode()))
assert len(rows)==5839 and sum(n for _,n,_ in rows)==267289095 and not out.exists();out.mkdir();(out/'.owned-original-full-oracle').write_bytes(b'original-full-oracle-v1');project=out/'project';project.mkdir();manifest=[]
for path,size,blob in rows:
 original=source/str(path);assert not original.is_symlink();raw=original.read_bytes();assert len(raw)==size and hashlib.sha1(b'blob '+str(len(raw)).encode()+b'\0'+raw).hexdigest()==blob,(str(path),'source changed');dest=project/str(path);assert dest.resolve().is_relative_to(project.resolve());dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(raw);manifest.append(dict(path=str(path),bytes=size,git_blob=blob,sha256=hashlib.sha256(raw).hexdigest()))
receipt=dict(source=pin,files=len(rows),bytes=sum(row['bytes']for row in manifest),scope='Exact private tracked source copy for original-scene oracle;no source/upstream mutation;not release assets or port implementation',manifest=manifest);(out/'source-inputs.json').write_text(json.dumps(receipt,indent=2)+'\n');print('Private original oracle staged',receipt['files'],'files',receipt['bytes'],'bytes;source all verified')
