"""Freeze independent family metadata and two actual original lifecycle runs."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_geyser_lifecycle as g,run_geyser_factory_reference as n,run_geyser_map_factory_reference as mapped
REVIEW=ROOT/'compatibility/reviews/geyser-lifecycle-v0410.json'

def build(out):
 out=Path(out).resolve();marker=out/'.owned-geyser-lifecycle-fixture';g.require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and(not out.exists()or marker.is_file()and not marker.is_symlink()and marker.read_bytes()==b'geyser-lifecycle-fixture-v1'),'Owned lifecycle fixture under build required');review=json.loads(REVIEW.read_bytes(),object_pairs_hook=g.unique);g.fields(review,'schema source producer_sha256 native_producer_sha256 map_native_producer_sha256 results mapping unverified','lifecycle review');_,model,source=g.source();producer=hashlib.sha256(Path(g.__file__).read_bytes()).hexdigest();native=hashlib.sha256(Path(n.__file__).read_bytes()).hexdigest();map_native=hashlib.sha256(Path(mapped.__file__).read_bytes()).hexdigest();g.require(type(review['schema'])is int and review['schema']==1 and review['source']==source and review['producer_sha256']==producer and review['native_producer_sha256']==native and review['map_native_producer_sha256']==map_native and type(review['results'])is list and len(review['results'])==4,'Lifecycle review/source/producer changed');docs=[]
 for row in review['results']:
  g.fields(row,'path sha256 profile','lifecycle result');g.require(row['profile']in['objects_first','map_order'],'Unknown lifecycle observation profile');observer=n if row['profile']=='objects_first'else mapped;expected_native=native if row['profile']=='objects_first'else map_native;g.g.safe(row['path']);folder=(ROOT/row['path']).resolve();g.require(folder.is_relative_to(ROOT/'reports/geyser-lifecycle')and not folder.is_symlink(),'Lifecycle frozen result escaped');raw=(folder/'reference.json').read_bytes();receipt=json.loads((folder/'receipt.json').read_bytes(),object_pairs_hook=g.unique);g.require(hashlib.sha256(raw).hexdigest()==row['sha256']and receipt['result_sha256']==row['sha256']and receipt['producer_sha256']==expected_native,'Lifecycle frozen result/producer differs')
  for name,h in receipt['generated_sha256'].items():path=(folder/'observer'/name).resolve();g.require(path.is_relative_to(folder/'observer')and not path.is_symlink()and hashlib.sha256(path.read_bytes()).hexdigest()==h,'Lifecycle frozen observer changed')
  docs.append(observer.validate_result(json.loads(raw,object_pairs_hook=g.unique),receipt))
 g.require([r['profile']for r in review['results']]==['objects_first','objects_first','map_order','map_order']and docs[0]==docs[1]and docs[2]==docs[3],'Lifecycle original runs/profile ownership differ');_,gm,_=g.g.source();ids={'res://'+s['source']:s['id']for s in gm['sounds']};streams={};lines=['2']
 def phase(state):
  lines.append(' '.join(map(str,[int(state['player_paused']),int(state['tree_paused']),*[int(v)for v in state['ui']]])))
  for clock in state['clocks']:lines.append(' '.join(map(str,[clock[0],clock[1],int(clock[2])])))
  lines.append(str(len(state['actors'])));commands=[]
  for actor in state['actors']:
   values=[actor['ticket'],actor['hole'],*actor['position'],int(actor['ready']),actor['time'],int(actor['playing']),actor['frame'],int(actor['monitoring']),int(actor['disabled']),int(actor['active']),int(actor['paused']),actor['gain'],int(actor['queued']),int(actor['on_screen'])];lines.append(' '.join(map(str,values)))
   for command in actor['commands']:
    if command[0]=='stream':streams[actor['ticket']]=ids[command[1]];value=[1,streams[actor['ticket']],0]
    elif command[0]=='play':g.require(actor['ticket']in streams,'Lifecycle play before source stream');value=[2,streams[actor['ticket']],command[1]]
    elif command[0]=='stop':value=[3,0,0]
    else:raise ValueError('Lifecycle unknown native command')
    commands.append([actor['ticket'],*value])
  # This source-map placement is entirely offscreen. Positive named-voice
  # delivery is separately tested with explicit contexts; do not invent an
  # owner for a native manager command that has no actor ticket in its log.
  g.require(not state['global_calls']and all(not a['on_screen']for a in state['actors']),'Lifecycle selected offscreen observation profile changed');lines.append(str(len(commands)))
  for command in commands:lines.append(' '.join(map(str,command)))
 for profile,doc in [(1,docs[0]),(0,docs[2])]:
  streams.clear();retired=sum(event[0]=='exit'for row in doc['rows']for state in[row['before'],row['after']]for event in state['events']);lines.append(' '.join(map(str,[len(doc['rows']),doc['spawn_count'],retired,profile])))
  for row in doc['rows']:
   lines.append(str(row['delta']));phase(row['before']);lines.append(str(len(row['actions'])))
   for action in row['actions']:
    g.require(action[0]in['player_pause','tree_pause','ui'],'Lifecycle action opcode');lines.append(str({'player_pause':1,'tree_pause':2,'ui':3}[action[0]])+' '+str(int(action[1])))
   phase(row['after'])
 outputs={'family.encglf':g.encode(model),'native.tsv':('\n'.join(lines)+'\n').encode()}
 for name,raw in outputs.items():path=out/name;g.require(not path.exists()or path.is_file()and not path.is_symlink()and path.read_bytes()==raw,'Lifecycle cached fixture changed')
 out.mkdir(parents=True,exist_ok=True);marker.write_bytes(b'geyser-lifecycle-fixture-v1')
 for name,raw in outputs.items():(out/name).write_bytes(raw)
 print('Checked lifecycle family and two tree-order profiles with two720-frame originals each;not new native execution');return outputs
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);build(a.parse_args().out)
