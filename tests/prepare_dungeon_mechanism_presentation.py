"""Compile checked geometry and project two immutable native source observations."""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_mechanism_presentation as d
REPORT=ROOT/'reports/dungeon-mechanism-presentation/source-media-1'

def build(out):
 out=Path(out).resolve();d.require(out.is_relative_to((ROOT/'build').resolve())and out!=ROOT/'build','Geometry fixtures stay under build')
 audit=json.loads((REPORT/'verification.json').read_bytes());observations=[]
 for i in(2,3):
  folder=REPORT/('geometry-original-'+str(i))
  for name in('geometry.json','geometry-input.json','geometry-receipt.json'):
   path=folder/name;d.require(hashlib.sha256(path.read_bytes()).hexdigest()==audit['evidence_sha256'].get(path.relative_to(REPORT).as_posix()),'Frozen mechanism geometry changed')
  proof=json.loads((folder/'geometry-receipt.json').read_bytes());d.require(proof['commit']==d.PIN and proof['producer_sha256']==hashlib.sha256((ROOT/'tools/run_dungeon_geometry_reference.py').read_bytes()).hexdigest()and proof['bootstrap_producer_sha256']==hashlib.sha256((ROOT/'tools/run_dungeon_mechanism_timeline_reference.py').read_bytes()).hexdigest(),'Geometry producer/pin changed')
  obs=json.loads((folder/'geometry.json').read_bytes());v=obs['engine'];d.require(tuple(v[k]for k in('major','minor','patch','hash'))==(3,6,2,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'),'Geometry engine changed');observations.append(obs)
 d.require(observations[0]==observations[1],'Geometry repeats differ');model=d.extract()[0];by_path={r['path']:r for r in model['owners']};shapes={(r['owner'],r['path']):i for i,r in enumerate(model['shapes'])};lines=['# Native actual source geometry, repeated two runs; no terrain/contact.']
 for run,obs in enumerate(observations):
  d.require(obs['defaults']==d.read()['engine_defaults']and len(obs['records'])==len(model['owners']),'Geometry defaults/owner count differs')
  for row in obs['records']:
   d.require(row['path']in by_path,'Unknown native geometry owner');owner=by_path[row['path']];sprite=row['sprite'];x,y=map(float,sprite['global_position']);rx,ry=map(float,sprite['rect']['position']);w,h=map(float,sprite['rect']['size']);pos=list(map(float,row['position']));d.require(row['parent_type']=='TileMap'and pos==owner['position'],'Native geometry parent/position differs')
   flags=sum(int(sprite[key])<<i for i,key in enumerate(('centered','visible','flip_h','flip_v','z_relative')))
   values=['O',run,owner['owner'],owner['profile'],owner['order'],int(row['parent_sort']),*pos,x+rx,y+ry,int(w),int(h),flags,sprite['z_index']];lines.append('\t'.join(map(str,values)))
   for shape in row['shapes']:
    d.require((owner['owner'],shape['path'])in shapes,'Unknown native geometry shape');index=shapes[(owner['owner'],shape['path'])];checked=model['shapes'][index];bits=int(shape['disabled'])|int(shape.get('monitoring',False))<<1|int(shape.get('monitorable',False))<<2|int(shape['one_way'])<<3
    values=['S',run,index,owner['owner'],checked['role'],shape['layer'],shape['mask'],bits,*shape['body_position'],*shape['shape_position'],*shape['extents'],*shape['global_center'],*shape['corners'][0],*shape['corners'][2],shape.get('safe_margin',0),shape['one_way_margin']];lines.append('\t'.join(map(str,values)))
 d.build(out);(out/'reference.tsv').write_text('\n'.join(lines)+'\n',encoding='ascii');print('Native geometry fixture:2 runs,14 owners/17 rectangles each; immutable expectations')

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args();build(a.out)
