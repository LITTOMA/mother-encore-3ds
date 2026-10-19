"""Checked static-waypoint plans for normal-input emulator regression drivers."""
import hashlib,json,math,re
from pathlib import Path
FIELDS=set('scope expanded grid waypoints schema source_commit map_path map_sha256 planner_source_sha256 destination_door route'.split())
def require(ok,why):
 if not ok:raise ValueError(why)
def validate(plan,map_bytes):
 require(type(plan)is dict and set(plan)==FIELDS,'Unknown/missing walk-plan fields')
 require(type(plan['schema'])is int and plan['schema']==1,'Unsupported walk-plan schema')
 require(type(plan['expanded'])is int and 0<=plan['expanded']<=250000 and type(plan['grid'])is int and plan['grid']in(4,6,8),'Walk-plan bounds')
 require(type(plan['route'])is int and 0<plan['route']<2**32,'Walk-plan route identity')
 require(type(plan['scope'])is str and 0<len(plan['scope'])<=256,'Walk-plan scope')
 for key,n in [('source_commit',40),('map_sha256',64),('planner_source_sha256',64)]:require(type(plan[key])is str and re.fullmatch('[0-9a-f]{'+str(n)+'}',plan[key]),'Walk-plan fingerprint '+key)
 for key in ('map_path','destination_door'):
  value=plan[key];require(type(value)is str and 0<len(value)<=256 and all(32<=ord(c)<127 and c not in '\\:?#'for c in value)and all(p not in ('','.','..')for p in value.split('/')),'Walk-plan canonical path')
 require(plan['map_path'].startswith('romfs/world/')and plan['map_path'].endswith('.encmap'),'Walk-plan map category')
 require(isinstance(map_bytes,bytes)and 64<=len(map_bytes)<=4*1024*1024 and map_bytes[:8]==b'ENCMAP01','Walk-plan requires checked map bytes')
 require(plan['map_sha256']==hashlib.sha256(map_bytes).hexdigest()and plan['source_commit']==map_bytes[32:52].hex(),'Walk-plan source/map mismatch')
 points=plan['waypoints'];require(type(points)is list and 2<=len(points)<=256 and all(type(p)is list and len(p)==2 and all(type(v)in(int,float)and math.isfinite(v)and abs(v)<=100000 for v in p)for p in points),'Walk-plan points rejected')
 for a,b in zip(points,points[1:]):
  require((a[0]==b[0])!=(a[1]==b[1]),'Walk-plan requires nonempty cardinal segments')
  distance=abs(b[0]-a[0])+abs(b[1]-a[1]);require(abs(distance/plan['grid']-round(distance/plan['grid']))<=.0001,'Walk-plan grid mismatch')
 return plan

def load(path,map_bytes):
 path=Path(path);require(path.is_file()and 0<path.stat().st_size<=65536,'Bounded walk-plan file required')
 def unique(pairs):
  result={}
  for key,value in pairs:require(key not in result,'Duplicate walk-plan field');result[key]=value
  return result
 return validate(json.loads(path.read_text(encoding='utf8'),object_pairs_hook=unique),map_bytes)
