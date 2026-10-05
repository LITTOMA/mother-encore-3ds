"""Manual-only producer negative checks; does not run during ordinary builds."""
import copy,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_item_definitions as tool
def rejects(d):
 try:tool.validate(d)
 except(ValueError,KeyError,TypeError):return
 raise AssertionError('Unsupported definition accepted')
def main():
 d=tool.load();assert tool.encode(d)==tool.PACK.read_bytes()
 assert len(d['definitions'])==17 and len(d['bindings'])==36
 for change in ('schema','pending','dose','transform','uid','operation','coverage','definition','duplicate'):
  x=copy.deepcopy(d)
  if change=='schema':x['schema']=2
  elif change=='pending':next(a for r in x['definitions']for a in r['actions'])['pending']=False
  elif change=='dose':x['definitions'][0]['doses']=0
  elif change=='transform':x['definitions'][0]['transform']='AsthmaSpray'
  elif change=='uid':x['policy']['uid_protocol']=0
  elif change=='operation':x['bindings'][0]['operation']=5
  elif change=='coverage':x['bindings'].pop()
  elif change=='definition':x['bindings'][0]['definition']=0
  else:x['definitions'][1]['id']=x['definitions'][0]['id']
  rejects(x)
 assert not any(b['definition']for b in d['bindings']if b['kind']==3), 'Actual town doors currently require no item key'
if __name__=='__main__':main()
