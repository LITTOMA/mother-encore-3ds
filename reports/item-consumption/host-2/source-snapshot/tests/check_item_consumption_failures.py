"""Checked fixture corruption failures, after a successful real baseline."""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

executable,romfs,fixture,output=map(Path,sys.argv[1:])
root=Path(__file__).resolve().parents[1]
output=output.resolve()
if root/'build'not in output.parents:
    raise ValueError('Owned failure-check output under build required')
marker=output/'.owned-item-consumption-failures'
if output.exists():
    if not marker.is_file()or marker.read_bytes()!=b'1\n':
        raise ValueError('Unowned failure-check output')
else:
    output.mkdir(parents=True);marker.write_bytes(b'1\n')
output=Path(tempfile.mkdtemp(prefix='run-',dir=output))
records=[]
count=int((fixture/'negative-reference-count.txt').read_text())
for name,args,expected in [('positive',[],0)]+[('negative-%d'%i,[str(fixture/('bad-reference-%d.bin'%i))],1)for i in range(count)]:
    command=[str(executable),str(romfs),str(fixture)]+args
    with(output/(name+'.stdout.log')).open('wb')as stdout,(output/(name+'.stderr.log')).open('wb')as stderr:
        result=subprocess.run(command,stdout=stdout,stderr=stderr,timeout=120)
    text=(output/(name+'.stderr.log')).read_text(errors='replace')
    if result.returncode!=expected or any(tag in text for tag in ['runtime error:','ERROR: AddressSanitizer','ERROR: LeakSanitizer']):
        raise ValueError('Unexpected actual fixture check: '+name)
    records.append(dict(command=command,returncode=result.returncode))
(output/'receipt.json').write_text(json.dumps(records,indent=2)+'\n')
print('Checked positive baseline and',count,'rejected corrupt observation fixtures')
