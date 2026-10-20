"""Actual candidate plus explicitly artificial alignment fixture; build only."""
import argparse,copy,hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_keys as d
def build(out):
 out=Path(out).resolve();d.require(out.is_relative_to((ROOT/'build').resolve())and out!=(ROOT/'build').resolve(),'Dungeon key fixtures stay under build')
 data,proof=d.extract();actual=d.encode(data);boundary=copy.deepcopy(data);boundary['region']+='X';padded=d.encode(boundary)
 strings=struct.unpack_from('<4I',padded,64);policy=struct.unpack_from('<4I',padded,80);d.require(policy[1]>strings[1]+strings[2],'Dungeon key synthetic fixture requires actual padding')
 out.mkdir(parents=True,exist_ok=True)
 for name,blob in [('catacombs.encdkeys',actual),('alignment-fixture.encdkeys',padded)]:(out/name).write_bytes(blob)
 proof.update(actual_bytes=len(actual),actual_sha256=hashlib.sha256(actual).hexdigest(),alignment_fixture='Synthetic region name suffix only, exercising canonical zero padding; not source dungeon/gameplay data.',alignment_sha256=hashlib.sha256(padded).hexdigest())
 (out/'fixture-source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Actual key metadata and artificial alignment fixture:',len(actual),len(padded))
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--out',type=Path,required=True);build(parser.parse_args().out)
