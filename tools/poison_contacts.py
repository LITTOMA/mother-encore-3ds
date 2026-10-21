"""Independent original poison trigger decomposition and Player footprint."""
import argparse,hashlib,json,math,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_poison as poison,catacombs_steps as steps
from tools.run_dungeon_key_contact_reference import player_source
from tools.extract_battle_entry import PIN,require
from tools.dungeon_keys import fields
from tools.dungeon_key_indicator import unique
RECIPE=ROOT/'content/catacombs-poison-contacts.json'
MAGIC=b'ENCPCN01';STRIDES=(1,32,1,16,16,8);LIMIT=1048576

def source(file=RECIPE):
 recipe=json.loads(Path(file).read_bytes(),object_pairs_hook=unique);fields(recipe,'schema kind commit scope poison_recipe poison_sha256 player_scene player_sha256 player_reader_sha256 geometry_producer_sha256 license_review','poison contact recipe')
 require(type(recipe['schema'])is int and recipe['schema']==1 and recipe['kind']=='encore.poison-contacts.reviewed-source'and recipe['commit']==PIN,'Poison contact reviewed identity');poison.safe(recipe['poison_recipe']);dependency=(ROOT/recipe['poison_recipe']).resolve();require(dependency.is_relative_to(ROOT/'content')and not dependency.is_symlink()and hashlib.sha256(dependency.read_bytes()).hexdigest()==recipe['poison_sha256'],'Poison contact dependency recipe changed')
 _,pm,proof=poison.source(dependency);player,player_proof=player_source();require(recipe['player_scene']==player_proof['path']and recipe['player_sha256']==player_proof['sha256']and recipe['player_reader_sha256']==hashlib.sha256((ROOT/'tools/run_dungeon_key_contact_reference.py').read_bytes()).hexdigest()and recipe['geometry_producer_sha256']==hashlib.sha256(Path(steps.geometry.__file__).read_bytes()).hexdigest(),'Poison contact body/geometry source changed');areas=[]
 for area in pm['areas']:
  shape=pm['shapes'][area['first']];require(shape['role']==1,'Poison contact source trigger role');points=[[poison.f.f32(v[k]+shape['position'][k])for k in [0,1]]for v in pm['points'][shape['first']:shape['first']+shape['count']]];parts=steps.geometry.decompose_polygon_in_convex(points);areas.append(dict(id=area['id'],parts=[[[x,y]for x,y in part]for part in parts]))
 body=[[poison.f.f32(p[k]+player['position'][k])for k in [0,1]]for p in player['points']];model=dict(scene=pm['scene'],source=pm['source'],player=player_proof['path'],layer=player['layer'],mask=player['mask'],body=body,areas=areas,poison=poison.encode(pm));validate(model)
 provenance=dict(commit=PIN,recipe_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),poison=proof,player=player_proof,player_reader_sha256=recipe['player_reader_sha256'],geometry_producer_sha256=recipe['geometry_producer_sha256'],scope=recipe['scope']);return recipe,model,provenance

def validate(model):
 fields(model,'scene source player layer mask body areas poison','poison contact model');poison.integer(model['scene'],1,65535);poison.safe(model['source']);poison.safe(model['player']);poison.integer(model['layer'],0,0xfffff);poison.integer(model['mask'],0,0xfffff);steps.convex(model['body']);require(type(model['poison'])is bytes,'Poison contact nested metadata type');nested=poison.parse(model['poison']);require(nested['scene']==model['scene']and nested['source']==model['source'],'Poison contact nested source/scene differs');require(type(model['areas'])is list and len(model['areas'])==len(nested['areas'])and 1<=len(model['areas'])<=128,'Poison contact exact area coverage');parts=0;points=len(model['body'])
 for row,area in zip(model['areas'],nested['areas']):
  fields(row,'id parts','poison contact area');require(type(row['id'])is int and row['id']==area['id']and type(row['parts'])is list and 1<=len(row['parts'])<=64,'Poison contact area identity/parts');shape=nested['shapes'][area['first']];raw=[[poison.f.f32(v[k]+shape['position'][k])for k in [0,1]]for v in nested['points'][shape['first']:shape['first']+shape['count']]];vertices=set(map(tuple,raw));used=set()
  def area2(polygon):return abs(sum(a[0]*b[1]-a[1]*b[0]for a,b in zip(polygon,polygon[1:]+polygon[:1])))
  total=0
  for part in row['parts']:
   steps.convex(part);require(set(map(tuple,part))<=vertices,'Poison contact foreign source vertex');used.update(map(tuple,part));parts+=1;points+=len(part);total+=area2(part)
  require(used==vertices and math.isclose(total,area2(raw),rel_tol=1e-12,abs_tol=1e-6),'Poison contact source geometry coverage')
 require(parts<=4096 and points<=16448,'Poison contact schema capacities');return model

def encode(model):
 validate(model);pool=bytearray();spans=[]
 for key in ['source','player']:value=model[key].encode('ascii');spans.extend([len(pool),len(value)]);pool.extend(value)
 policy=struct.pack('<8I',*spans,model['layer'],model['mask'],0,len(model['body']));areas=bytearray();parts=bytearray();points=bytearray()
 for point in model['body']:points.extend(struct.pack('<2f',*point))
 for index,row in enumerate(model['areas']):
  areas.extend(struct.pack('<4I',row['id'],len(parts)//16,len(row['parts']),0))
  for part in row['parts']:
   parts.extend(struct.pack('<4I',index,len(points)//8,len(part),0))
   for point in part:points.extend(struct.pack('<2f',*point))
 return poison.f.pack_container(MAGIC,model['scene'],[bytes(pool),policy,model['poison'],bytes(areas),bytes(parts),bytes(points)],STRIDES,LIMIT)

def parse(blob):
 scene,sections=poison.f.unpack_container(blob,MAGIC,STRIDES,LIMIT);pool,policy,nested,areas,parts,points=sections;require(1<=scene<=65535 and 0<len(pool)<=1536 and len(policy)==32 and 0<len(nested)<=262144 and 1<=len(areas)//16<=128 and 1<=len(parts)//16<=4096 and 3<=len(points)//8<=16448,'Poison contact section domains');values=struct.unpack('<8I',policy);used=bytearray(len(pool));texts=[]
 for first,length in zip(values[:4:2],values[1:4:2]):
  require(0<length<=512 and first<=len(pool)and length<=len(pool)-first and not any(used[first:first+length]),'Poison contact text ownership');value=pool[first:first+length].decode('ascii');poison.safe(value);texts.append(value);used[first:first+length]=b'\1'*length
 require(all(used)and values[6]==0 and 3<=values[7]<=64 and values[7]<=len(points)//8,'Poison contact exact text/body ownership');vectors=[list(struct.unpack_from('<2f',points,at))for at in range(0,len(points),8)];part_cursor=0;point_cursor=values[7];rows=[]
 for at in range(0,len(areas),16):
  identifier,first,count,reserved=struct.unpack_from('<4I',areas,at);require(first==part_cursor and 1<=count<=64 and count<=len(parts)//16-first and not reserved,'Poison contact area part coverage');polygons=[]
  for n in range(count):
   owner,first,count,reserved=struct.unpack_from('<4I',parts,part_cursor*16);require(owner==len(rows)and first==point_cursor and 3<=count<=64 and count<=len(vectors)-first and not reserved,'Poison contact convex point ownership');polygons.append(vectors[first:first+count]);part_cursor+=1;point_cursor+=count
  rows.append(dict(id=identifier,parts=polygons))
 require(part_cursor==len(parts)//16 and point_cursor==len(vectors),'Poison contact orphan geometry');return validate(dict(scene=scene,source=texts[0],player=texts[1],layer=values[4],mask=values[5],body=vectors[:values[7]],areas=rows,poison=nested))

if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);out=a.parse_args().out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh poison contact candidate under build required');_,model,_=source();blob=encode(model);require(parse(blob)==model,'Poison contact resource roundtrip');out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(blob);print('Original poison contact geometry candidate',len(blob),'bytes;not runtime/scene admission')
