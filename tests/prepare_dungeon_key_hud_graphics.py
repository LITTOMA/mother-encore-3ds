"""Keep original icon geometry/font identity; explicit synthetic decoder payload."""
import argparse,json,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_key_hud_graphics as g

def build(out):
 out=Path(out).resolve();marker=out/'.owned-key-hud-fixture';g.require(out.is_relative_to(ROOT/'build')and(not out.exists()or(marker.is_file()and marker.read_text('ascii')=='explicit-key-hud-fixture-v1\n')),'Owned key HUD fixture under build required');out.mkdir(parents=True,exist_ok=True);marker.write_text('explicit-key-hud-fixture-v1\n',encoding='ascii')
 model,proof=g.source_model();raw=b'EXPLICIT KEY HUD DECODER TEST PAYLOAD; NOT T3X OR GPU PIXELS\n';image=out/model['texture'];image.parent.mkdir(parents=True,exist_ok=True);image.write_bytes(raw);model.update(texture_bytes=len(raw),texture_crc=zlib.crc32(raw));blob=g.encode(model);(out/'source.enckgx').write_bytes(blob)
 p,_=g.read();native=json.loads((ROOT/next(iter(p['native_layouts']))).read_text('utf-8'));lines=[]
 for row in native['rows']:lines.append(json.dumps(row['text'])+' '+' '.join(str(x)for x in row['icon_position']+row['label_position']+row['string_size']))
 (out/'layout.tsv').write_text('\n'.join(lines)+'\n',encoding='utf-8');proof.update(texture_kind='Explicit decoder double payload, not a valid texture or GPU result');(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8');print('Source key HUD fixture;one explicit synthetic payload;no font atlas duplicated');return blob
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);build(p.parse_args().out)
