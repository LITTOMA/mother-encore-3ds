"""Build concrete factory resources and strict original composition inputs."""
import argparse,copy,hashlib,json,math,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import coin_scene as c,run_coin_scene_reference as native
from tools.coin_scene_protocol import validate
from tests.prepare_coins import scalar,state
REVIEW=ROOT/'compatibility/reviews/catacombs-coin-scene-v0410.json'

def protocol(doc,config,coin_model):
 validate(doc,config);roles={v['name']:v['id']for v in coin_model['clips']};lines=['ENCCOIN_SCENE02 '+str(len(doc['cases']))+' '+str(len(config['bindings']))]
 def row(values):lines.append(' '.join(map(str,values)))
 for case in doc['cases']:
  row([case['mode'],config['seed'],len(case['rows']),len(case['births']),case['rng_tail'],case['cash'],case['cash_opens'],len(case['collect_audio'])])
  for birth in case['births']:row([birth['ordinal'],birth['factory'],birth['physics'],birth['idle'],*birth['position'],scalar(birth['speed']),*birth['direction'],scalar(birth['air_time']),birth['sparkle']])
  for record in case['rows']:
   row([record['frame'],record['physics'],record['idle'],record['born'],int('live'in record)])
   for factory in record['factories']:
    row([factory['id'],scalar(factory['timer']),factory['remaining'],int(factory['spawning']),*factory['position'],json.dumps(factory['parent']),int(factory['flag']),int(factory['body_disabled'])])
   if 'live'in record:
    row([len(record['live'])])
    for live in record['live']:row([live['ordinal'],*state(live['state'],roles),len(live['drops']),*live['drops']])
 return ('\n'.join(lines)+'\n').encode()

def build(out):
 review=json.loads(REVIEW.read_bytes(),object_pairs_hook=c.unique);c.fields(review,'schema source native_producer_sha256 fixture_protocol_sha256 observation_protocol_sha256 results mapping unverified','Coin scene reviewed runs');_,model,source=c.source();files,copied,expected=native.source();producer=hashlib.sha256(Path(native.__file__).read_bytes()).hexdigest();c.require(type(review['schema'])is int and review['schema']==1 and review['source']==source and review['native_producer_sha256']==producer and review['fixture_protocol_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest()and review['observation_protocol_sha256']==expected['protocol_sha256']and type(review['results'])is list and len(review['results'])==2 and type(review['mapping'])is str and bool(review['mapping'])and type(review['unverified'])is list and all(type(v)is str and v for v in review['unverified']),'Coin scene reviewed source/producer/protocol');docs=[];folders=set();config=json.loads(files['input.json'])
 for run in review['results']:
  c.fields(run,'path sha256','Coin scene frozen run');c.b.text(run['path']);folder=(ROOT/run['path']).resolve();c.require(folder.is_relative_to(ROOT/'reports/coin-scene')and folder not in folders and not folder.is_symlink(),'Coin scene frozen path');folders.add(folder);raw=(folder/'reference.json').read_bytes();receipt=json.loads((folder/'receipt.json').read_bytes(),object_pairs_hook=c.unique);c.fields(receipt,' '.join(expected)+' result_sha256 engine_binary_sha256 producer_sha256 return_code elapsed_seconds','Coin scene original receipt');c.require(all(receipt.get(k)==v for k,v in expected.items())and receipt['result_sha256']==run['sha256']==hashlib.sha256(raw).hexdigest()and receipt['producer_sha256']==producer and type(receipt['return_code'])is int and receipt['return_code']==0 and receipt['engine_binary_sha256']=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd'and type(receipt['elapsed_seconds'])in[int,float]and math.isfinite(receipt['elapsed_seconds'])and receipt['elapsed_seconds']>0,'Coin scene frozen execution changed');c.require(json.loads((folder/'inputs.json').read_bytes(),object_pairs_hook=c.unique)==expected and hashlib.sha256((folder/'producer.py').read_bytes()).hexdigest()==producer,'Coin scene frozen inputs/retained observer changed');docs.append(validate(json.loads(raw,object_pairs_hook=c.unique),config))
 c.require(docs[0]==docs[1],'Coin scene original composition runs differ');_,coin_model,_=c.coins.source();_,break_model,_=c.b.source();partial=copy.deepcopy(model);partial['bindings']=partial['bindings'][:1];outputs={'scene.enccf':c.encode(model),'partial.enccf':c.encode(partial),'coins.encoin':c.coins.encode(coin_model),'breakables.encbrk':c.b.encode(break_model),'native.tsv':protocol(docs[0],config,coin_model)};out=Path(out).resolve();marker=out/'.owned-coin-scene-fixture';c.require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and(not out.exists()or marker.is_file()and not marker.is_symlink()and marker.read_bytes()==b'coin-scene-fixture-v1'),'Coin scene owned fixture required')
 for name,raw in outputs.items():p=out/name;c.require(not p.exists()or p.is_file()and not p.is_symlink()and p.read_bytes()==raw,'Coin scene fixture cache changed')
 out.mkdir(parents=True,exist_ok=True);marker.write_bytes(b'coin-scene-fixture-v1')
 for name,raw in outputs.items():(out/name).write_bytes(raw)
 print('Checked original49-factory composition in three modes, twice; not new native execution');return outputs

if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--out',type=Path,required=True);build(parser.parse_args().out)
