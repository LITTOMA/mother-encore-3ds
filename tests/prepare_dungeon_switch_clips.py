"""Source property clips fixture, using the unchanged existing binary format."""
import argparse,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_switch_clips
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);dungeon_switch_clips.build(p.parse_args().out)
