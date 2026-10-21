"""Portable explicit texture-service fixture, never a fabricated GPU image.

Keep the source clocks/geometry; replace only the three media byte identities
with labelled test payloads consumed by a declared Tex3DS API double. Local
checks may instead pass the genuine prepared T3X directory and original blob.
"""
import argparse,hashlib,json,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tests.prepare_dungeon_key_media import build as source_fixture
from tools import dungeon_key_media as media

def build(out):
 out=Path(out).resolve();marker=out/'.owned-key-renderer-fixture';media.require(out.is_relative_to(ROOT/'build')and(not out.exists()or(marker.is_file()and marker.read_text('ascii')=='explicit-key-renderer-fixture-v1\n')),'Owned renderer fixture under build required');out.mkdir(parents=True,exist_ok=True);marker.write_text('explicit-key-renderer-fixture-v1\n',encoding='ascii')
 source=source_fixture(out/'original');data=media.parse(source)
 for profile in data['profiles']:
  raw=('EXPLICIT TEXTURE SERVICE TEST PAYLOAD; NOT T3X; profile='+str(profile['id'])+'\n').encode('ascii')
  path=out/profile['texture'];path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(raw)
  profile.update(texture_bytes=len(raw),texture_crc=zlib.crc32(raw),texture_sha256=hashlib.sha256(raw).hexdigest())
 blob=media.encode(data,data['bank']);media.require(media.parse(blob)['clocks']==media.parse(source)['clocks'],'Renderer fixture changed source clocks');(out/'source.enckmp').write_bytes(blob)
 (out/'fixture.json').write_text(json.dumps(dict(kind='explicit-texture-api-double-not-gpu',source_sha256=hashlib.sha256(source).hexdigest(),fixture_sha256=hashlib.sha256(blob).hexdigest(),changed_fields=['profile texture byte length/CRC/SHA only']),indent=2)+'\n',encoding='utf8')
 print('Key renderer fixture:source clocks and geometry;3 explicit synthetic payloads,not GPU files')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);build(p.parse_args().out)
