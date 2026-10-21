"""Translate retained original observations into checked native test inputs."""
import argparse,hashlib,json,math,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import coins as b,run_coin_reference as native,run_circle_motion_reference as circle_native
from tools.coin_reference_protocol import validate,vector
REVIEW=ROOT/'compatibility/reviews/catacombs-coins-v0410.json'

def scalar(h):return int.from_bytes(bytes.fromhex(h),'little')
def state(s,roles):
 return [*s['position'],scalar(s['speed']),*s['direction'],scalar(s['air_time']),scalar(s['start_air_time']),int(s['grounded']),int(s['physics']),int(s['body_disabled']),int(s['pickup_disabled']),*s['offset'],s['sprite_frame'],s['sparkle_frame'],scalar(s['glow']),roles[s['animation']],int(s['playing']),s['time'],int(s['queued'])]

def actors(config,native,model,target=None):
 roles={c['name']:c['id']for c in model['clips']};lines=['ENCCOIN_NATIVE03 '+str(len(config['cases']))]
 for ci,c in enumerate(config['cases']):
  rows=[v for v in native['rows']if v['case']==ci];contacts=[e for e in native['contacts']if e['case']==ci];solids=c.get('obstacles',[]);peek=rows[-1]['peek_after_ready'];lines.append(' '.join(map(str,[ci,c['name'],c.get('seed',config['seed']),int(c['peek']),peek or 0,rows[0]['cash'],len(solids),len(rows),len(contacts),int(c['area'])])))
  for solid in solids:
   kind={'rectangle':3,'convex':1,'segment':2}[solid['kind']]
   if kind==3:
    x,y=solid['center'];w,h=solid['extents'];points=[[x-w,y-h],[x+w,y-h],[x+w,y+h],[x-w,y+h]]
   else:points=solid['points']
   lines.append(' '.join(map(str,[kind,len(points),*(v for p in points for v in p)])))
  for e in contacts:lines.append(' '.join(map(str,[e['frame'],1 if e['kind']=='body'else 2,e['identity'],e.get('layer',0)])))
  for row in rows:
   values=[row['frame'],int(row['alive']),row['cash'],row['cash_opens'],len(row['drop_audio']),*row['drop_audio'],len(row['collect_audio']),*(e[0]for e in row['collect_audio'])]
   if row['alive']:values.extend(state(row['state'],roles))
   lines.append(' '.join(map(str,values)))
  lines.append('TAIL '+str(rows[-1]['rng_tail'])+' '+str(int(rows[-1]['duplicate_collect_called'])))
 raw=('\n'.join(lines)+'\n').encode()
 if target is not None:Path(target).write_bytes(raw)
 return raw

def factories(config,native,model,target=None):
 roles={c['name']:c['id']for c in model['clips']};lines=['ENCCOIN_FACTORY04 '+str(len(config['spawner_cases']))]
 for ci,c in enumerate(config['spawner_cases']):
  rows=[v for v in native['spawners']if v['case']==ci];lines.append(' '.join(map(str,[ci,c['name'],c['seed'],int(c['use_default']),c['amount'],c['restart_frame'],c['restart_amount'],*c['position'],len(rows)])))
  for row in rows:
   lines.append(' '.join(map(str,[row['frame'],scalar(row['timer']),int(row['spawning']),row['remaining'],row['cash'],row['cash_opens'],row['collect_calls'],len(row['coins'])])))
   for birth in row['coins']:lines.append(' '.join(map(str,[birth['ordinal'],birth['physics'],birth['idle']]))+' '+json.dumps(birth['parent'])+' '+' '.join(map(str,[*birth['position'],scalar(birth['speed']),*birth['direction'],scalar(birth['air_time']),birth['sparkle']])))
   lines.append(str(len(row['live'])))
   for item in row['live']:lines.append(' '.join(map(str,[item['ordinal'],*state(item['state'],roles)])))
  lines.append('TAIL '+str(rows[-1]['rng_tail']))
 raw=('\n'.join(lines)+'\n').encode()
 if target is not None:Path(target).write_bytes(raw)
 return raw

def circles(config,doc):
 b.fields(doc,'engine cases','Circle original result');engine=doc['engine'];b.require(type(engine)is dict and all(type(engine.get(k))is int for k in ['major','minor','patch'])and(engine['major'],engine['minor'],engine['patch'])==(3,6,2)and engine.get('hash')==circle_native.ENGINE_COMMIT and type(doc['cases'])is list and len(doc['cases'])==len(config),'Circle original coverage/engine');lines=['ENCCIRCLE_NATIVE01 '+str(len(config))]
 for definition,row in zip(config,doc['cases']):
  b.fields(row,'name frames','Circle original case');b.require(row['name']==definition['name']and type(row['frames'])is list and len(row['frames'])==len(definition['velocities']),'Circle original identity/frames');solids=definition['obstacles'];lines.append(' '.join(map(str,[row['name'],*definition['position'],definition['radius'],definition['margin'],*definition['offset'],len(solids),len(row['frames'])])))
  for kind,points in zip(definition['obstacle_kinds'],solids):
   if kind=='RectangleShape2D':
    xs=[p[0]for p in points];ys=[p[1]for p in points];center=[(min(xs)+max(xs))/2,(min(ys)+max(ys))/2];extents=[(max(xs)-min(xs))/2,(max(ys)-min(ys))/2];points=[[center[0]-extents[0],center[1]-extents[1]],[center[0]+extents[0],center[1]-extents[1]],[center[0]+extents[0],center[1]+extents[1]],[center[0]-extents[0],center[1]+extents[1]]]
   lines.append(' '.join(map(str,[{'ConvexPolygonShape2D':1,'ConcavePolygonShape2D':2,'RectangleShape2D':3}[kind],len(points),*(v for point in points for v in point)])))
  for velocity,frame in zip(definition['velocities'],row['frames']):
   b.fields(frame,'position velocity','Circle original frame');vector(frame['position']);vector(frame['velocity']);lines.append(' '.join(map(str,[*velocity,*frame['position'],*frame['velocity']])))
 return ('\n'.join(lines)+'\n').encode()

def frozen(rows,expected,producer,kind):
 b.require(type(rows)is list and len(rows)==2,'Coin two independent original runs required');docs=[];paths=set()
 for row in rows:
  b.fields(row,'path sha256','Coin frozen run');b.text(row['path']);folder=(ROOT/row['path']).resolve();b.require(folder.is_relative_to(ROOT/'reports/coins')and folder not in paths and not folder.is_symlink(),'Coin frozen path');paths.add(folder);raw=(folder/'reference.json').read_bytes();receipt=json.loads((folder/'receipt.json').read_bytes(),object_pairs_hook=b.unique);digest=hashlib.sha256(raw).hexdigest();extra='engine_binary_sha256 return_code result_sha256 elapsed_seconds producer_sha256'if kind=='actor'else'cases frames result_sha256 return_code';b.fields(receipt,' '.join(expected)+' '+extra,'Coin original receipt')
  b.require(digest==row['sha256']==receipt['result_sha256']and all(receipt.get(k)==v for k,v in expected.items())and type(receipt['return_code'])is int and receipt['return_code']==0 and receipt['producer_sha256']==producer,'Coin original source/observer/result changed')
  b.require(json.loads((folder/'inputs.json').read_bytes(),object_pairs_hook=b.unique)==expected,'Coin original input receipt changed')
  if kind=='actor':b.require(receipt['engine_binary_sha256']==circle_native.ENGINE_SHA and type(receipt['elapsed_seconds'])in[int,float]and math.isfinite(receipt['elapsed_seconds'])and receipt['elapsed_seconds']>0,'Coin actual engine/time')
  docs.append(json.loads(raw,object_pairs_hook=b.unique))
 b.require(docs[0]==docs[1],'Coin original repeated runs differ')
 return docs[0]

def build(out):
 review=json.loads(REVIEW.read_bytes(),object_pairs_hook=b.unique);b.fields(review,'schema source native_producer_sha256 circle_producer_sha256 fixture_protocol_sha256 observation_protocol_sha256 coin_results circle_results mapping unverified','Coin reviewed mapping');_,model,source=b.source();files,copied,expected=native.source();circle_files,circle_expected=circle_native.source();native_sha=hashlib.sha256(Path(native.__file__).read_bytes()).hexdigest();circle_sha=hashlib.sha256(Path(circle_native.__file__).read_bytes()).hexdigest()
 b.require(type(review['schema'])is int and review['schema']==1 and review['source']==source and review['native_producer_sha256']==native_sha and review['circle_producer_sha256']==circle_sha and review['fixture_protocol_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest()and review['observation_protocol_sha256']==expected['protocol_sha256']and type(review['mapping'])is str and bool(review['mapping'])and type(review['unverified'])is list and all(type(v)is str and v for v in review['unverified']),'Coin reviewed source/producers')
 config=json.loads(files['input.json']);doc=validate(frozen(review['coin_results'],expected,native_sha,'actor'),config,{c['name']for c in model['clips']});circle_config=json.loads(circle_files['cases.json']);circle_doc=frozen(review['circle_results'],circle_expected,circle_sha,'circle')
 for row in review['circle_results']:
  receipt=json.loads((ROOT/row['path']/'receipt.json').read_bytes(),object_pairs_hook=b.unique);b.require(type(receipt['cases'])is int and receipt['cases']==len(circle_config)and type(receipt['frames'])is int and receipt['frames']==sum(len(c['velocities'])for c in circle_config),'Circle recorded case/frame coverage')
 outputs={'coins.encoin':b.encode(model),'native.tsv':actors(config,doc,model),'factory.tsv':factories(config,doc,model),'circle.tsv':circles(circle_config,circle_doc)};out=Path(out).resolve();marker=out/'.owned-coin-fixture';b.require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and(not out.exists()or marker.is_file()and not marker.is_symlink()and marker.read_bytes()==b'coin-fixture-v1'),'Owned Coin fixture required')
 for name,raw in outputs.items():p=out/name;b.require(not p.exists()or p.is_file()and not p.is_symlink()and p.read_bytes()==raw,'Coin fixture cache changed')
 out.mkdir(parents=True,exist_ok=True);marker.write_bytes(b'coin-fixture-v1')
 for name,raw in outputs.items():(out/name).write_bytes(raw)
 print('Checked two original 1573-state/33-factory runs and two 246-state circle runs; not new native execution');return outputs

if __name__=='__main__':
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--out',type=Path,required=True);build(ap.parse_args().out)
