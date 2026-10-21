"""Rebuild small original-music metadata;PCM stays in the private converter run."""
import argparse,hashlib,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_music as music,run_catacombs_music_reference as native,audio_asset
def fixture():
 files,proof=native.source();music.require(proof==json.loads((ROOT/'compatibility/reviews/catacombs-music-v0410.json').read_bytes()),'Catacombs music semantic review changed')
 a=ROOT/'reports/catacombs-music/native-2';b=ROOT/'reports/catacombs-music/native-3'
 for name in ['reference.json','geometry.json','scenes.tsv']:music.require((a/name).read_bytes()==(b/name).read_bytes(),'Catacombs repeated native result differs')
 for folder in [a,b]:
  receipt=json.loads((folder/'receipt.json').read_bytes());music.require(receipt['source']==proof and receipt['engine_sha256']=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd','Catacombs native receipt/source changed')
  for name,key in [('reference.json','result_sha256'),('geometry.json','geometry_sha256')]:music.require(hashlib.sha256((folder/name).read_bytes()).hexdigest()==receipt[key],'Catacombs native result fingerprint')
  native.validate_result(json.loads((folder/'reference.json').read_bytes()),proof);native.validate_geometry(json.loads((folder/'geometry.json').read_bytes()),proof['source'])
 data=native.fixture(json.loads((a/'reference.json').read_bytes()),proof).encode('utf-8');music.require(data==(a/'scenes.tsv').read_bytes(),'Catacombs native TSV changed');return proof,data,json.loads((a/'geometry.json').read_bytes())
def audio_bank(metadata):
 actual=json.loads((ROOT/'reports/catacombs-music/component-1/actual-conversion.json').read_bytes());p,current,polygons,source=music.source()
 music.require(actual['metadata']==metadata==current and actual['review_sha256']==source['review_sha256']and actual['sources']==source['sources'],'Catacombs actual audio conversion source changed')
 recipe=json.loads((ROOT/'content/native-audio.json').read_bytes());ex=music.Extractor(ROOT);buses=ex.text(recipe['bus_source']);floor=re.search(r'^const SILENT_SOUND_THRESHOLD = (-?[0-9.]+)$',ex.text(recipe['manager_source']),re.M);master=re.search(r'^bus/0/volume_db = (-?[0-9.]+)$',buses,re.M);music.require(floor and master,'Actual audio source tuning changed')
 bank=audio_asset.build_bank(actual['audio']['assets'],float(master[1]),float(floor[1]));expected=actual['outputs'][p['output']['audio']]
 music.require(len(bank)==expected['bytes']and hashlib.sha256(bank).hexdigest()==expected['sha256'],'Rebuilt metadata differs from actual FFmpeg bank');return bank
def build(out):
 out=Path(out).resolve();marker=out/'.owned-catacombs-music-fixture';music.require(out.is_relative_to(ROOT/'build')and(not out.exists()or marker.is_file()and marker.read_bytes()==b'catacombs-music-fixture-v1'),'Owned Catacombs fixture under build required');proof,tsv,geometry=fixture();p,metadata,polygons,source=music.source();binding,zones=music.codec.encode(metadata,polygons);bank=audio_bank(metadata);out.mkdir(parents=True,exist_ok=True);marker.write_bytes(b'catacombs-music-fixture-v1')
 for name,data in [('source.encmusic',binding),('source.enczones',zones),('source.encaudio',bank),('scenes.tsv',tsv)]: (out/name).write_bytes(data)
 lines=[]
 for row in geometry['rows']:
  values=[row['path'],row['gain'],row['fadein'],row['fadeout'],row['appear']or'-',row['disappear']or'-',len(row['shapes'])]
  for shape in row['shapes']:values.extend([len(shape['points']),*[v for point in shape['points']for v in point]])
  lines.append(' '.join(str(v)for v in values))
 (out/'geometry.tsv').write_bytes(('\n'.join(lines)+'\n').encode('utf-8'));print('Small actual Catacombs metadata:',len(binding),len(zones),len(bank),'bytes;92 native method observations/5 geometry records;PCM not copied');return binding,zones,bank
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);build(p.parse_args().out)
