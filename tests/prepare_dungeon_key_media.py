"""Rebuild source clocks/PCM, join retained genuine tex3ds metadata and native geometry.

The portable fixture does not fabricate GPU texture files. Actual T3X bytes
are checked separately by passing a genuine prepared graphics directory.
"""
import argparse,hashlib,json,shutil,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_key_media as d
REFERENCE=ROOT/'reports/dungeon-key-media'

def build(out,reference=REFERENCE):
 out=Path(out).resolve();d.require(out.is_relative_to(ROOT/'build'),'Key media fixture stays under build');out.mkdir(parents=True,exist_ok=True)
 observed=json.loads((reference/'native-8/geometry.json').read_text('utf8'));second=json.loads((reference/'native-9/geometry.json').read_text('utf8'));d.require(observed==second,'Key media native repetitions differ');m=d.validate_native(observed);_,_,proof=d.source_model();receipt=json.loads((reference/'graphics/source.json').read_text('utf8'));d.require(receipt['review_sha256']==proof['review_sha256']and receipt['clock_sha256']==proof['clock_sha256'],'Key media retained graphics recipe differs')
 atlas={p['id']:p for p in receipt['profiles']}
 for p in m['profiles']:
  r=atlas[p['id']];d.require(all(r[k]==p[k]for k in p),'Key media retained source atlas differs');p.update({k:r[k]for k in['texture_bytes','texture_crc','texture_sha256']})
 with tempfile.TemporaryDirectory(prefix='key-media-audio-',dir=ROOT/'build')as folder:
  a=Path(folder)/'audio';d.sound_assets(a);bank=(a/'sound/banks/catacombs-keys.encaudio').read_bytes();blob=d.encode(m,bank);(out/'source.enckmp').write_bytes(blob)
  for asset in d.audio.parse_bank(bank)['assets']:
   path=asset['pcm_path'];target=out/path;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(a/path,target)
 rows=[]
 for r in observed['sprites']:
  xy=[float(r['global_position'][a])+float(r['rect']['position'][a])for a in range(2)]
  rows.append(' '.join(map(str,['S',r['owner'],r['profile'],r['frame'],*xy,*r['rect']['size']])))
 for r in observed['shapes']:
  ex=[float(a)*abs(float(b))for a,b in zip(r['extents'],r['scale'])];bits=int(r['disabled'])|int(r['monitoring'])<<1|int(r['monitorable'])<<2
  rows.append(' '.join(map(str,['R',r['owner'],r['role'],*r['center'],*ex,r['layer'],r['mask'],bits])))
 for r in observed['sounds']:rows.append(' '.join(map(str,['A',r['owner'],r['length']])))
 (out/'reference.tsv').write_text('\n'.join(rows)+'\n',encoding='utf8');print('Key media fixture:',len(blob),'bytes;69 native frame rectangles,9 shapes,6 stream lengths;2 real decoded PCM,retained T3X metadata only')
 return blob
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args();build(a.out)
