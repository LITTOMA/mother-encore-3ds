from pathlib import Path
import hashlib,json,subprocess
from tools import native_content as c
r=Path.cwd();out=r/'build/full-port/story-object-generation-before.json';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
names=subprocess.check_output(['git','ls-files','romfs','content'],text=True).splitlines();files={n:sha(r/n)for n in names if(r/n).is_file()}
rooms={n:c.parse_pack((r/n).read_bytes())for n in names if n.startswith('romfs/')and n.endswith('.encroom')}
for v in rooms.values():v.pop('build_input_sha256')
out.write_text(json.dumps(dict(head=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),files=files,rooms=rooms),indent=2)+'\n',encoding='utf8');print('Before source regeneration:',len(files),'tracked hashes;',len(rooms),'decoded room baselines')
