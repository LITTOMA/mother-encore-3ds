"""Manual source IR positive/negative checks; no ordinary build execution."""
from pathlib import Path
import copy,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_player_transitions as tool
def rejects(d):
 try:tool.validate(d)
 except(ValueError,KeyError,TypeError):return
 raise AssertionError('Unsupported transition admitted')
def main():
 d=tool.load();assert tool.encode(d)==tool.PACK.read_bytes()
 assert sum(r['kind']==1 for r in d['records'])==14 and sum(r['kind']==2 for r in d['records'])==2
 for change in ('schema','height','shape','unknown','timing','coverage','sync','ray'):
  x=copy.deepcopy(d)
  if change=='schema':x['schema']=2
  elif change=='height':next(r for r in x['records']if r['kind']==1)['height']=-1
  elif change=='shape':x['areas'][0]['kind']=99
  elif change=='unknown':x['anything']=True
  elif change=='timing':x['params'][3]=0
  elif change=='coverage':x['records'].pop()
  elif change=='sync':next(r for r in x['records']if r['kind']==1)['flags']=8
  else:next(r for r in x['records']if r['kind']==1)['ray']=[]
  rejects(x)
if __name__=='__main__':main()
