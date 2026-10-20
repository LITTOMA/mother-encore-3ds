"""Project frozen original method observations; never rewrite expectations."""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import require
SOURCE=ROOT/'reports/catacombs-switches/metadata-oracle-1'
ANIMATION={'':0,'TurnOff':1,'TurnOn':2,'Off':3,'On':4,'StayOff':5,'StayOn':6}
def packed(values):
 result=1
 for value in values:result=result*4+value
 return result
def build(out):
 out=Path(out).resolve();require(out.is_relative_to((ROOT/'build').resolve()),'Switch reference must stay under build')
 file=SOURCE/'original-method-5/observations.json';proof=json.loads((SOURCE/'verification.json').read_bytes())
 require(hashlib.sha256(file.read_bytes()).hexdigest()==proof['evidence_sha256']['original-method-5/observations.json'],'Frozen original switch reference changed')
 data=json.loads(file.read_bytes());require(len(data['rows'])==28,'Frozen switch observation count changed');lines=['index action followed_by_71_state_and_service_values']
 for index,row in enumerate(data['rows']):
  values=[]
  for n in row['switches']:values.extend([int(n['on']),int(n['disabled']),int(n['ready']),ANIMATION[n['animation']],int(n['audio'])])
  for n in row['objects']:values.extend([int(n['on']),ANIMATION[n['animation']],n['order'],n['registrations']])
  values.extend([int(row['paused']),int(row['cutscene']),packed([{'pause':1,'unpause':2}[n]for n in row['player_events']]),packed([2 if n else 1 for n in row['ui_events']]),packed([{'fade':1,'resume':2,'sfx':3}[n['kind']]for n in row['audio_events']])])
  camera=row['camera_events'];require(len(camera)<=1,'Frozen source camera request cardinality changed')
  if camera:
   n=camera[0];require(n['kind']in('move','return'),'Frozen camera method unsupported');values.extend([1,1 if n['kind']=='move'else 2,n.get('x',0),n.get('y',0),n['duration']])
  else:values.extend([0,0,0,0,0])
  require(len(values)==71,'Frozen switch projection width changed');lines.append(' '.join(map(str,[index,row['action']]+values)))
 out.parent.mkdir(parents=True,exist_ok=True);out.write_text('\n'.join(lines)+'\n',encoding='utf8');print('Projected28 frozen original switch/control observations with71 state/service fields each')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);build(p.parse_args().out)
