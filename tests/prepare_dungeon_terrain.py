"""Original detached terrain, frozen native logical-cell oracle and geometry."""
import argparse,copy,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_terrain as t

def build(out):
 out=Path(out).resolve();marker=out/'.owned-terrain-fixture';t.require(out.is_relative_to(ROOT/'build')and(not out.exists()or marker.is_file()and marker.read_text('ascii')=='terrain-fixture-v1\n'),'Owned terrain fixture under build');out.mkdir(parents=True,exist_ok=True);marker.write_text('terrain-fixture-v1\n',encoding='ascii')
 model,atlas,graphics,native,proof=t.source_model();policy,_=t.read();blob=t.encode_source(model,graphics,native,policy);(out/'source.encterr').write_bytes(blob);(out/'graphics.enctmap').write_bytes(graphics)
 # Synthetic codec-only optional-navigation path,explicitly not original data.
 no_nav=copy.deepcopy(native)
 for tile in no_nav['tiles']:tile['navigation']=None
 alternate=t.encode_source(model,graphics,no_nav,policy);(out/'synthetic-no-navigation.encterr').write_bytes(alternate)
 lines=[]
 for layer in native['layers']:
  ident=next(row['id']for row in policy['layers']if row['path']==layer['name'])
  for cell in layer['cells']:lines.append(' '.join(str(int(v))for v in [ident,*cell]))
 (out/'cells.tsv').write_text('\n'.join(lines)+'\n',encoding='utf-8');proof.update(bytes=len(blob),native_cells=len(lines));(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8');print('Original terrain fixture:',len(blob),'bytes;',len(lines),'native logical cells;no root admission or navigation solver');return blob
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);build(p.parse_args().out)
