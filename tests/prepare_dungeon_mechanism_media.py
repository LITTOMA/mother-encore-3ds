"""Compile the joined source sound ownership; preserve capability1 geometry."""
import argparse,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_mechanism_media as media,dungeon_mechanism_presentation as presentation
def build(out):
 proof=media.build(out);_,geometry,_=presentation.extract();(Path(out)/'geometry.encdmp').write_bytes(geometry);return proof
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args();build(a.out)
