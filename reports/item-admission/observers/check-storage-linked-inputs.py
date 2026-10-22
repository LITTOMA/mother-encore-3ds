from pathlib import Path
import hashlib,json,re,subprocess,sys
root=Path(__file__).resolve().parents[2]
out=(root/sys.argv[1]).resolve();assert root/'build'in out.parents
inputs=json.loads((root/'build/full-port/shadow-core-host-2/inputs.json').read_bytes())
members=set()
for map_file in out.glob('*.map'):
    members.update(re.findall(r'libencore-shadow\.a\(([^)]+)\)',map_file.read_text()))
assert members
record={}
for member in sorted(members):
    candidates=[path for path in inputs if path.startswith('runtime/')and path.endswith('.cpp')
                and member in (path.replace('/','_')+'.o',Path(path).name+'.o',Path(path).stem+'.o')]
    assert len(candidates)==1,(member,candidates)
    source=candidates[0]
    raw=subprocess.check_output(['g++','-MM','-std=c++17','-Iinclude',source],cwd=root,text=True)
    dependencies=raw.replace('\\\n',' ').split(':',1)[1].split()
    assert all(path in inputs for path in dependencies),dependencies
    changed=[path for path in dependencies if hashlib.sha256((root/path).read_bytes()).hexdigest()!=inputs[path]]
    assert not changed,(member,changed)
    record[member]={path:inputs[path]for path in dependencies}
(out/'linked-baseline-inputs.json').write_text(json.dumps(record,indent=2)+'\n')
print('Validated',len(members),'reused archive members including every current project header dependency')
