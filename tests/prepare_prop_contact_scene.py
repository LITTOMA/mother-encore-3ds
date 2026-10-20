"""Private complete scene and contact metadata; no audio playback claim."""
import argparse,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tests.prepare_shovel_scene_programme import build as scene
from tests.prepare_prop_contact_data import build as contacts
from tools import resource_catalog as catalog

def build(out):
 out=Path(out).resolve();scene(out);contacts(out/'contact-metadata')
 (out/'world/podunk/podunk.encprops').write_bytes((out/'contact-metadata/source.encprops').read_bytes())
 # Regenerate the private catalog through its real encoder after replacing
 # the complete packet; never mutate a catalog fingerprint by hand.
 (out/'system/native.encresources').write_bytes(catalog.encode(catalog.load_catalog(),out))
 print('Complete contact scene fixture; exact source Room14 and contact3; prepared sound remains a test double')

if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);build(ap.parse_args().out)
