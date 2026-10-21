"""Checked original Sprite observations and an explicit image API-double fixture."""
import argparse,hashlib,json,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_geyser_graphics as graphics,run_geyser_graphics_reference as native
REVIEW=ROOT/'compatibility/reviews/geyser-graphics-v0410.json'

def build(out):
 out=Path(out).resolve();marker=out/'.owned-geyser-graphics-fixture';graphics.require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and(not out.exists()or marker.is_file()and not out.is_symlink()and marker.read_bytes()==b'geyser-graphics-fixture-v1'),'Owned Geyser graphics fixture required');p,model,_,source=graphics.source();review=json.loads(REVIEW.read_bytes(),object_pairs_hook=graphics.timeline.unique);graphics.timeline.fields(review,'schema source producer_sha256 native_producer_sha256 results mapping unverified','Geyser graphics semantic review');producer=hashlib.sha256(Path(graphics.__file__).read_bytes()).hexdigest();nproducer=hashlib.sha256(Path(native.__file__).read_bytes()).hexdigest();graphics.require(type(review['schema'])is int and review['schema']==1 and review['source']==source and review['producer_sha256']==producer and review['native_producer_sha256']==nproducer and type(review['results'])is list and len(review['results'])==2,'Geyser graphics review source/producer changed');docs=[]
 for row in review['results']:
  graphics.timeline.fields(row,'path sha256','Geyser graphics original result');graphics.timeline.safe(row['path']);folder=(ROOT/row['path']).resolve();graphics.require(folder.is_relative_to(ROOT/'reports/geyser-graphics')and not folder.is_symlink(),'Geyser graphics frozen result escaped');raw=(folder/'reference.json').read_bytes();proof=json.loads((folder/'receipt.json').read_bytes(),object_pairs_hook=graphics.timeline.unique);graphics.require(hashlib.sha256(raw).hexdigest()==row['sha256']and proof['result_sha256']==row['sha256']and proof['producer_sha256']==nproducer,'Geyser graphics frozen bytes changed');docs.append(native.validate_result(json.loads(raw),proof))
 graphics.require(docs[0]==docs[1],'Original Geyser Sprite runs differ');doc=docs[0];lines=[str(len(doc['frames']))]
 for row in doc['frames']:lines.append(' '.join(map(str,[row['frame'],*row['rect']])))
 lines.append(str(len(doc['positions'])))
 for row in doc['positions']:lines.append(' '.join(map(str,row['root']+row['top_left'])))
 fake=b'EXPLICIT-GEYSER-TEXTURE-API-DOUBLE-v1';model['texture_bytes']=len(fake);model['texture_crc']=zlib.crc32(fake);blob=graphics.encode(model);graphics.require(graphics.parse(blob)==model,'Geyser graphics fixture roundtrip');outputs={p['output']:blob,p['texture']:fake,'geyser.encgeyser':model['timeline'],'native.tsv':('\n'.join(lines)+'\n').encode()};out.mkdir(parents=True,exist_ok=True);marker.write_bytes(b'geyser-graphics-fixture-v1')
 for name,data in outputs.items():
  target=out/name;graphics.require(not target.exists()or target.is_file()and not target.is_symlink()and target.read_bytes()==data,'Geyser cached graphics fixture changed');target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
 print('Checked32 original Sprite frames/3positions;explicit fake image for API checks,not T3X/GPU execution');return outputs
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);build(a.parse_args().out)
