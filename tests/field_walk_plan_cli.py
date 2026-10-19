"""Exercise the offline planner's source joins and fail-closed CLI admission."""
from pathlib import Path
import json,subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.native_content import parse_pack
exe=Path(sys.argv[1]).resolve();room=ROOT/'romfs/world/mayor/mayor.encroom';map_path=ROOT/'romfs/world/mayor/mayor.encmap'
with tempfile.TemporaryDirectory(prefix='walk-cli-',dir=ROOT/'build')as temporary:
 owner=Path(temporary);flags=owner/'flags';flags.write_bytes(b'');base=[str(room),str(map_path),str(flags),'8','-519','8','-560','4']
 def run(args):return subprocess.run([str(exe),*args],capture_output=True,timeout=20,cwd=ROOT)
 p=run(base);assert p.returncode==0,p.stderr.decode(errors='replace');plan=json.loads(p.stdout);assert plan['waypoints'][0]==[8,-519]and plan['waypoints'][-1]==[8,-559]and plan['grid']==4
 cases=[[],base[:-1]]
 for at,value in [(3,''),(3,'bad'),(3,'nan'),(3,'inf'),(3,'100001'),(7,'0'),(7,'5'),(7,'4.5'),(1,str(ROOT/'romfs/world/podunk/podunk.encmap')),(2,str(owner/'absent'))]:
  args=list(base);args[at]=value;cases.append(args)
 for args in cases:
  p=run(args);assert p.returncode==2,(args,p.returncode,p.stderr.decode(errors='replace'))
 parsed=parse_pack(room.read_bytes());name=parsed['strings'][parsed['sections']['Flag'][0]['name_string']]
 for text in ['unknown_test_flag\n',name+'\n'+name+'\n']:
  flags.write_text(text);p=run(base);assert p.returncode==2,(text,p.returncode)
 print('Walk planner CLI:',1+len(cases)+2,'checks; actual source room/map, invalid numbers/grids, wrong scene, missing/unknown/duplicate flag input; offline static scope only')
