"""Execute official Godot circle motion; no game scripts/assets are executed.

The radius comes from the pinned Coin prefab. Solids/velocities are explicit
test fixtures. This does not establish arbitrary full-scene broadphase order.
"""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import podunk_field as f
from tools.godot_text import instantiate,owner_of
from tools.run_collision_reference import cases
from tools.run_scene_reference import invoke

ENGINE_SHA='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd'
ENGINE_COMMIT='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'

def inputs():
 s=f.Sources();by={n.path:n for n in instantiate(s.doc,'Nodes/Reusables/Coin.tscn')}
 node=by['CollisionShape2D'];shape=owner_of(node,'shape').sub_resource(node.props['shape'],'CircleShape2D')
 assert set(shape)=={'radius'} and type(shape['radius'])is float and shape['radius']>0
 result=[]
 for definition in cases([]):
  for kind in ['ConvexPolygonShape2D','RectangleShape2D']:
   # A non-rectangle polygon remains a convex-only fixture.
   if kind=='RectangleShape2D'and any(len(p)!=4 or len({q[0]for q in p})!=2 or len({q[1]for q in p})!=2 for p in definition['obstacles']):continue
   row={k:v for k,v in definition.items()if k!='actor'}
   row.update(name=definition['name']+'_'+kind,radius=shape['radius'],margin=.08,offset=[0,0],obstacle_kinds=[kind]*len(row['obstacles']))
   result.append(row)
 for name,points,start,velocities in [('segment_side',[[12,-100],[12,100]],[0,0],[[600,200]]*12),('segment_endpoint',[[12,-5],[12,5]],[0,4],[[600,100]]*12),('segment_recovery',[[12,-100],[12,100]],[11.8,0],[[0,0]]*5),('segment_long_sweep',[[12,-100],[12,100]],[0,0],[[30000,0]]),('segment_horizontal',[[-100,16],[100,16]],[0,0],[[100,600]]*10)]:
  result.append(dict(name=name,position=start,obstacles=[points],obstacle_kinds=['ConcavePolygonShape2D'],velocities=velocities,radius=shape['radius'],margin=.08,offset=[0,0]))
 result.append(dict(name='shape_offset',position=[0,0],obstacles=[[[12,-100],[32,-100],[32,100],[12,100]]],obstacle_kinds=['RectangleShape2D'],velocities=[[600,240]]*10,radius=shape['radius'],margin=.08,offset=[3,-1]))
 return result,s.digests()

def source():
 definitions,sources=inputs();files={'project.godot':'config_version=4\n[physics]\ncommon/physics_fps=60\n','cases.json':json.dumps(definitions,indent=2)+'\n','probe.gd':(ROOT/'tools/godot_exporter/circle_motion_probe.gd').read_text('utf8')}
 proof=dict(sources=sources,engine_binary_sha256=ENGINE_SHA,engine_commit=ENGINE_COMMIT,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),inputs={n:hashlib.sha256(v.encode()).hexdigest()for n,v in files.items()},scope='Actual CircleShape2D static shape subset; explicit fixture solids/velocities; no game script, complete room, GPU or hardware claim')
 return files,proof

def main():
 p=argparse.ArgumentParser();p.add_argument('--work',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();work=a.work.resolve();engine=a.godot.resolve()
 assert work.is_relative_to(ROOT/'build')and not work.exists()and hashlib.sha256(engine.read_bytes()).hexdigest()==ENGINE_SHA
 work.mkdir(parents=True);(work/'.owned-circle-reference').write_bytes(b'circle-reference-v1');files,proof=source();definitions=json.loads(files['cases.json'])
 for n,v in files.items():(work/n).write_text(v,encoding='utf8',newline='\n')
 (work/'inputs.json').write_text(json.dumps(proof,indent=2)+'\n')
 invoke(engine,work,['--video-driver','GLES2','--debug','--fixed-fps','60','--script','res://probe.gd','--encore-out='+str(work/'reference.json')],work/'godot.log')
 assert all(hashlib.sha256((work/n).read_bytes()).hexdigest()==v for n,v in proof['inputs'].items())
 raw=(work/'reference.json').read_bytes();doc=json.loads(raw);assert doc['engine']['hash']==ENGINE_COMMIT and len(doc['cases'])==len(definitions)
 for definition,row in zip(definitions,doc['cases']):
  assert row['name']==definition['name']and len(row['frames'])==len(definition['velocities'])
  assert all(set(frame)=={'position','velocity'}and all(type(v)is int and 0<=v<=0xffffffff for field in frame.values()for v in field)for frame in row['frames'])
 proof.update(cases=len(definitions),frames=sum(len(v['velocities'])for v in definitions),result_sha256=hashlib.sha256(raw).hexdigest(),return_code=0)
 (work/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n');print(proof['cases'],proof['frames'],proof['result_sha256'])
if __name__=='__main__':main()
