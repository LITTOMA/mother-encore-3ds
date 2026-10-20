"""Compile source bindings and project the frozen combined-engine observations."""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_mechanism_bindings as d
REPORT=ROOT/'reports/dungeon-mechanism-timelines/source-integration-1'

def build(out):
 out=Path(out).resolve();require=d.require;require(out.is_relative_to((ROOT/'build').resolve())and out!=ROOT/'build','Mechanism fixtures stay under build')
 first=REPORT/'original-5';second=REPORT/'original-6'
 audit=json.loads((REPORT/'verification.json').read_bytes())
 for folder in (first,second):
  for name in ('observations.json','input.json','receipt.json'):
   path=folder/name;relative=path.relative_to(REPORT).as_posix();require(hashlib.sha256(path.read_bytes()).hexdigest()==audit['evidence_sha256'].get(relative),'Combined frozen source fingerprint changed')
 require((first/'observations.json').read_bytes()==(second/'observations.json').read_bytes(),'Combined source repeats differ')
 proof=json.loads((second/'receipt.json').read_bytes());require(proof['commit']==d.PIN and proof['producer_sha256']==hashlib.sha256((ROOT/'tools/run_dungeon_mechanism_timeline_reference.py').read_bytes()).hexdigest(),'Combined source producer/pin changed')
 observed=json.loads((second/'observations.json').read_bytes());config=json.loads((second/'input.json').read_bytes());require(len(observed['rows'])==232,'Combined source observations incomplete')
 version=observed['engine'];require(tuple(version.get(k)for k in('major','minor','patch','hash'))==(3,6,2,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'),'Combined source engine version changed')
 d.build(out);review=d.read();model=d.parse((out/'source.encdma').read_bytes());byid={p['id']:p for p in review['profiles']};byowner={r['owner']:r for r in model['bindings']};bypath={r['path']:r for r in review['bindings']}
 roles={r['owner']:{name:ident for name,ident in byid[r['profile']]['roles'].items()}for r in model['bindings']}
 def ident(path):require(path in bypath,'Unknown source node');return bypath[path]['owner']
 def clip(owner,name):require(name in roles[owner],'Unknown source assigned clip');return roles[owner][name]
 def scalar(value):return format(value,'.12g')
 main=ident(config['main']);other=ident('Bottom2/switch1');lines=['# Combined source graph and native animation observations; manual batch, separate native idle/timer boundary.']
 for row in observed['rows']:
  label=row['action'];argument=0
  if label=='ready':op='ready'
  elif label.split('/',1)[0]in('initial','focus_pending','opening','other_switch_animations')and len(label.split('/'))==2 and label.split('/')[1].isdigit():op='frame'
  elif label=='ready_timers_complete':op='ready_timers'
  elif label in('main_interact','one_way_repeat'):op='interact';argument=main
  elif label=='other_switch_on':op='interact';argument=other
  elif label in('focus_complete','camera_return_complete'):op='camera'
  elif label=='wall_timer_pending':op='noop'
  elif label=='wall_timer_complete_before_camera_return':op='idle_wall'
  elif label=='leave_resets_flag_only':op='leave'
  else:raise ValueError('Unknown combined source action')
  players=','.join('1'if v=='pause'else'0'if v=='unpause'else'unknown'for v in row['player_events']);require('unknown'not in players,'Unknown source player request');ui=','.join(str(int(v))for v in row['ui_events'])
  music=[]
  for v in row['audio_events']:
   if v['kind']=='fade':music.append('fade:'+scalar(v['value']))
   elif v['kind']=='resume':music.append('resume')
   else:raise ValueError('Unknown combined music request')
  cameras=[]
  for v in row['camera_events']:
   if v['kind']=='move':cameras.append('move:'+':'.join(scalar(v[k])for k in('x','y','duration')))
   elif v['kind']=='return':cameras.append('return:'+scalar(v['duration']))
   elif v['kind']=='shake':cameras.append('shake:'+':'.join(scalar(float(a))for a in v['arguments']))
   else:raise ValueError('Unknown combined camera request')
  timeline=[]
  for v in row['timeline_events']:
   if v['kind']=='audio':timeline.append('audio:'+str(int(v['playing'])))
   elif v['kind']=='finished':
    owner=ident(v['path']);timeline.append('finished:'+str(owner)+':'+str(clip(owner,v['clip'])))
   else:raise ValueError('Unknown combined timeline event')
  groups=[players,ui,','.join(music),','.join(cameras),','.join(timeline)]
  lines.append(' '.join(['step',label,op,str(argument),str(int(row['paused'])),str(int(row['cutscene'])),*[v or'-'for v in groups]]))
  require([n['path']for n in row['nodes']]==[r['path']for r in review['bindings']],'Combined observation source order mismatch')
  for n in row['nodes']:
   owner=ident(n['path']);require(owner in byowner,'Combined source owner missing');lines.append(' '.join(map(str,['node',owner,n['frame'],int(n['body']),clip(owner,n['assigned']),int(n['playing']),n['position'],int(n['on'])])))
 (out/'reference.tsv').write_text('\n'.join(lines)+'\n',encoding='utf8');print('Combined reference projected:232 steps,14 source owners,explicit native idle/timer boundary; no synthesized animation completion.')

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);build(p.parse_args().out)
