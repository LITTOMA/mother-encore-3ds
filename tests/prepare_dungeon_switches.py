"""Build-only source graph fixture; no release registration."""
import argparse,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_switches as d
def build(out):
 out=Path(out).resolve();d.require(out.is_relative_to((ROOT/'build').resolve())and out!=(ROOT/'build').resolve(),'Dungeon switch fixture must stay under build')
 data,proof=d.extract();blob=d.encode(data);out.mkdir(parents=True,exist_ok=True);(out/'source.encdsw').write_bytes(blob)
 (out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Actual dungeon switch graph',len(blob),'bytes')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);build(p.parse_args().out)
