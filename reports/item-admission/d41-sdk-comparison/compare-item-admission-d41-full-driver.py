from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[2];source=root/'build/full-port/compare-item-admission-d41-official.py';raw=source.read_text()
assert raw.count("item-admission-d41-official")==1 and raw.count('run=39771293469;artifact=12424100129')==1
raw=raw.replace('item-admission-d41-official','item-admission-d41-full-official').replace('run=39771293469;artifact=12424100129','run=39771295093;artifact=12422724217')
target=root/'build/full-port/compare-item-admission-d41-full.py';assert not target.exists();target.write_bytes(raw.encode())
command=[sys.executable,'-X','utf8',str(target)];result=subprocess.run(command,cwd=root);raise SystemExit(result.returncode)
