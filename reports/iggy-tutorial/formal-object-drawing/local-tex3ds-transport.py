"""Use the exact source-admitted Linux converter from Windows asset generators."""
from pathlib import Path,PureWindowsPath
import subprocess,sys,hashlib,json
from tempfile import TemporaryDirectory
root=Path(__file__).resolve().parents[2];tool=root/'build/tooling/pinned-tex3ds/tex3ds';proof=json.loads(tool.with_name('proof.json').read_bytes())
assert hashlib.sha256(tool.read_bytes()).hexdigest()==proof['converter_sha256']
def translated(value):
 p=PureWindowsPath(value)
 if len(p.drive)==2 and p.drive[1]==':'and p.root:return '/mnt/'+p.drive[0].lower()+'/'+ '/'.join(p.parts[1:])
 return value
command=['wsl','-d','Ubuntu-20.04','--cd',translated(str(root)),'--',translated(str(tool))]
args=list(sys.argv[1:])
if '-o'not in args:raise SystemExit(subprocess.run(command+[translated(v)for v in args]).returncode)
index=args.index('-o');assert index+1<len(args);destination=Path(args[index+1]);destination=(root/destination).resolve()if not destination.is_absolute()else destination.resolve();assert destination.is_relative_to(root)and destination!=root
# Existing NTFS files may carry root-owned WSL metadata. Keep the real tool
# unprivileged, writing a fresh private output; publish its exact bytes using
# the Windows writer only after successful conversion. No permission changes.
with TemporaryDirectory(prefix='tex3ds-run-',dir=root/'build/tooling')as folder:
 candidate=Path(folder)/'output.t3x';args[index+1]=str(candidate);result=subprocess.run(command+[translated(v)for v in args])
 if result.returncode:raise SystemExit(result.returncode)
 assert candidate.is_file()and candidate.stat().st_size>0;destination.write_bytes(candidate.read_bytes())
