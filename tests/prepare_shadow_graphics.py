"""Source Shadow metadata,explicit texture API double,and actual clock oracle."""
import argparse,hashlib,json,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_shadow_graphics as g,catacombs_steps as steps,run_shadow_clock_reference as native
REPORT=ROOT/'reports/catacombs-shadow-graphics'
REVIEW=ROOT/'compatibility/reviews/shadow-graphics-v0410.json'

def build(out):
 out=Path(out).resolve();marker=out/'.owned-shadow-fixture';g.require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and(not out.exists()or marker.is_file()and marker.read_bytes()==b'shadow-fixture-v1'),'Owned Shadow fixture under build required');out.mkdir(parents=True,exist_ok=True);marker.write_bytes(b'shadow-fixture-v1');_,model,_,proof=g.build();review=json.loads(REVIEW.read_bytes());g.require(review['source']==proof and review['native_producer_sha256']==hashlib.sha256(Path(native.__file__).read_bytes()).hexdigest(),'Shadow source/method review changed')
 # CPU schema/API submission only;this payload is deliberately not a T3X.
 # Local real-conversion run substitutes the actual candidate separately.
 fake=b'EXPLICIT-SHADOW-TEXTURE-API-DOUBLE-v1';target=out/model['texture'];target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(fake);model['texture_bytes']=len(fake);model['texture_crc']=zlib.crc32(fake);blob=g.encode(model);g.require(g.parse(blob)==model,'Shadow fixture roundtrip');(out/'shadow.encshadow').write_bytes(blob);_,source,_=steps.source();(out/'catacombs.encsteps').write_bytes(steps.encode(steps.binary_model(source)));mode_ids={m['name']:i for i,m in enumerate(model['modes'])};profiles=[];operations={'mode':0,'frame':1,'visible':2,'player_pause':3,'tree_pause':4,'position':5}
 for label,fps,no_draw in[('60',60,False),('10',10,False),('no-draw',60,True)]:
  path=REPORT/('native-'+label+'-1');raw=(path/'reference.json').read_bytes();receipt=json.loads((path/'receipt.json').read_bytes());expected=next(r for r in review['results']if r['profile']==label and r['run']==1);g.require(hashlib.sha256(raw).hexdigest()==expected['sha256']and receipt['producer_sha256']==review['native_producer_sha256'],'Shadow frozen native result/producer changed');doc=native.validate_result(json.loads(raw),receipt);rows=[]
  for row in doc['rows']:
   values=[row['tick'],len(row['actions'])]
   for action in row['actions']:
    op=operations[action[0]];a=b=0
    if op==0:a=mode_ids[action[1]]
    elif op==5:a,b=action[1]
    else:a=int(action[1])
    values +=[op,a,b]
   values +=[row['delta_bits'],int(row['process_enabled']),mode_ids[row['mode']],row['frame'],int(row['playing']),int(row['behind']),int(row['visible']),int(row['player_pause']),int(row['tree_pause']),*row['body_position']];rows.append(' '.join(map(str,values)))
  filename='clock-'+label+'.tsv';(out/filename).write_bytes(('\n'.join(rows)+'\n').encode());profiles.append(filename+' '+str(fps)+' '+str(int(no_draw)))
 (out/'profiles.tsv').write_bytes(('\n'.join(profiles)+'\n').encode());(out/'source.json').write_bytes((json.dumps(dict(source=proof,metadata=model,bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),texture_is_explicit_api_double=True,native_clock_observations=384,scope='Frozen actual native input/output;fixture creation isnot new native/GPU execution'),indent=2)+'\n').encode());print('Shadow fixture:',len(blob),'metadata bytes;384 original clock states;explicit fake texture for API tests');return blob
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);build(a.parse_args().out)
