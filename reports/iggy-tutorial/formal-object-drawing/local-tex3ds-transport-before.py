"""Use the exact source-admitted Linux converter from Windows asset generators."""
from pathlib import Path,PureWindowsPath
import subprocess,sys,hashlib,json
root=Path(__file__).resolve().parents[2];tool=root/'build/tooling/pinned-tex3ds/tex3ds';proof=json.loads(tool.with_name('proof.json').read_bytes())
assert hashlib.sha256(tool.read_bytes()).hexdigest()==proof['converter_sha256']
def translated(value):
 p=PureWindowsPath(value)
 if len(p.drive)==2 and p.drive[1]==':'and p.root:return '/mnt/'+p.drive[0].lower()+'/'+ '/'.join(p.parts[1:])
 return value
command=['wsl','-d','Ubuntu-20.04','--cd',translated(str(root)),'--',translated(str(tool))]
command.extend(translated(v)for v in sys.argv[1:])
raise SystemExit(subprocess.run(command).returncode)
