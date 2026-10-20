"""Compile the complete source getshovel graph as an unpublished core fixture."""
from pathlib import Path
import argparse,hashlib,json,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import graveyard_shovel_programme as c,native_content as nc
from tools.extract_battle_entry import require
def build():
 blob,p,review,ex=c.candidate();text=c.texts(p,review,ex);room=nc.parse_pack(blob)
 return blob,dict(scope=__doc__,programme=len(room['sections']['Program'])-1,commands=p['commands'],actors=p['actors'],labels=p['labels'],terminal=p['terminal'],texts=text,sources=ex.sources,room_sha256=hashlib.sha256(blob).hexdigest())
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--out',type=Path,required=True);v=a.parse_args();out=v.out.resolve();require(out!=ROOT/'build'and(ROOT/'build').resolve()in out.parents,'Owned shovel fixture directory required');out.mkdir(parents=True,exist_ok=True);blob,proof=build();(out/'source.encroom').write_bytes(blob);(out/'source.json').write_text(json.dumps(proof,indent=2,ensure_ascii=False)+'\n',encoding='utf8');print('Unpublished full shovel fixture:',len(proof['commands']),'commands;',len(proof['texts']),'locale rows')
