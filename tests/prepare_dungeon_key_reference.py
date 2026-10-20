"""Project original-method observations, retaining the two signal-only negatives."""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import require
SOURCE=ROOT/'reports/catacombs-keys/metadata-oracle-1'
def build(out):
 out=Path(out).resolve();require(out.is_relative_to((ROOT/'build').resolve()),'Dungeon reference output must stay under build')
 file=SOURCE/'original-method-7/observations.json';proof=json.loads((SOURCE/'verification.json').read_bytes())
 require(hashlib.sha256(file.read_bytes()).hexdigest()==proof['evidence_sha256']['original-method-7/observations.json'],'Frozen original-method evidence changed')
 observations=json.loads(file.read_bytes());require(len(observations['rows'])==18,'Original-method row count changed')
 lines=['index action counter key_flag lock_flag alive queued visible disabled prompt opened animation audio notifications notice_visible notice_disabled notice_opened notice_audio indicators indicator_value locked_text']
 negatives=[]
 for index,row in enumerate(observations['rows']):
  if row['action']in('negative_missing_handler_connected','negative_missing_handler_after_emit'):
   negatives.append(row);continue
  pickup=index<10;node=row['key'if pickup else'lock'];alive=node['alive'];notices=row['notifications'];require(len(notices)<=1,'Original-method notification domain changed')
  notice=notices[0]['key'if pickup else'lock']if notices else{}
  indicators=[e['value']for e in row['ui']if e['kind']=='indicator'];texts=[e['path']for e in row['ui']if e['kind']=='text']
  require(len(indicators)<=1 and len(texts)<=1 and (not texts or texts==['Reusable/locklocked']),'Original-method UI projection changed')
  values=[index,row['action'],row['count'],int(row['flags'].get('Catacombs/Key1',False)),int(row['flags'].get('Catacombs/Locked Door3',False)),int(alive)]
  values +=[int(node.get(k,False))if alive else 0 for k in ('queued','visible','disabled','prompt','opened')]
  values +=[{'':0,'Open':1,'Opened':2}.get(node.get('animation',''),-1)if alive else 0,int(node.get('audio',False))if alive else 0,len(notices)]
  values +=[int(notice.get(k,False))for k in ('visible','disabled','opened','audio')]
  values +=[len(indicators),indicators[0]if indicators else -1,len(texts)]
  lines.append(' '.join(map(str,values)))
 require(len(lines)==17 and len(negatives)==2,'Reference projection lost a valid method or negative boundary')
 a,b=(dict(n)for n in negatives);a.pop('action');b.pop('action');require(a==b,'Original negative signal changed state')
 out.parent.mkdir(parents=True,exist_ok=True);out.write_text('\n'.join(lines)+'\n',encoding='utf8');print('Projected16 valid-method states; two original signal-dispatch negatives remain separately verified, not implemented by the key-state kernel')
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--out',type=Path,required=True);build(parser.parse_args().out)
