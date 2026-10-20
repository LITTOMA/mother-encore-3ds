"""Verify and copy the frozen original-engine property observation projection."""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import require
SOURCE=ROOT/'reports/catacombs-switch-clips/reference-oracle-1'
def build(out):
 out=Path(out).resolve();require(out.is_relative_to((ROOT/'build').resolve()),'Dungeon property reference must stay under build')
 file=SOURCE/'original-2/states.tsv';proof=json.loads((SOURCE/'verification.json').read_bytes());require(hashlib.sha256(file.read_bytes()).hexdigest()==proof['evidence_sha256']['original-2/states.tsv'],'Frozen dungeon property observations changed')
 require(len(file.read_text(encoding='utf8').splitlines())==810,'Frozen dungeon property observation count changed');out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(file.read_bytes());print('Verified/copied809 original property observations; expectations unchanged')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);build(p.parse_args().out)
