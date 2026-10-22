"""Compile source owners and replay the retained original prepared-host runs."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_breakables as b,run_breakable_reference as native
REVIEW=ROOT/'compatibility/reviews/catacombs-breakables-v0410.json'

def validate(doc,config):
 b.fields(doc,'schema godot rows','Breakable native result');b.require(type(doc['schema'])is int and doc['schema']==1 and type(doc['godot'])is dict and doc['godot'].get('hash','').startswith('3cd3caab')and type(doc['rows'])is list and len(doc['rows'])==47*len(config['cases']),'Breakable native identity/count')
 for ci,case in enumerate(config['cases']):
  has=bool(config['owners'][case['owner']]['content'])
  for ri,frame in enumerate(range(-1,46)):
   row=doc['rows'][ci*47+ri];expected=set('case frame root_visible parent position frame_index sprite_visible assigned playing time body_disabled body_layer hit_layer hit_mask flag paused rumble audio coins has_content'.split())|({'content'}if has else set())|({'pending'}if frame==45 else set());b.require(type(row)is dict and set(row)==expected and type(row['case'])is int and row['case']==ci and type(row['frame'])is int and row['frame']==frame,'Breakable native row identity/fields')
   for key in ['root_visible','sprite_visible','playing','body_disabled','flag','paused','has_content']+(['pending']if frame==45 else[]):b.boolean(row[key])
   b.require(row['has_content']==has and row['assigned']in['','Break']and((row['time']is None)==(row['assigned']=='')),'Breakable native content/animation')
   if row['time']is not None:b.integer(row['time'])
   b.text(row['parent']);b.integer(row['frame_index'],0,65535)
   for key in ['body_layer','hit_layer','hit_mask']:b.integer(row[key],0,(1<<20)-1)
   def point(value):b.require(type(value)is list and len(value)==2,'Breakable native vector');[b.integer(v)for v in value]
   point(row['position'])
   for key,columns in [('rumble',5),('audio',2),('coins',2)]:
    b.require(type(row[key])is list and len(row[key])<=64,'Breakable native request count');previous=-1
    for request in row[key]:
     b.require(type(request)is list and len(request)==columns and type(request[0])is int and previous<=request[0]<=max(frame,0),'Breakable native request order');previous=request[0]
     if key=='audio':b.boolean(request[1])
     elif key=='coins':b.integer(request[1],1,1024)
     else:b.integer(request[1],0,15);[b.number(v)for v in request[2:]]
   if has:
    content=row['content'];b.fields(content,'parent position visible physics changing start interactions shapes','Breakable native content');b.text(content['parent']);point(content['position']);point(content['start']);b.integer(content['interactions'],0,1024)
    for key in ['visible','physics','changing']:b.boolean(content[key])
    b.require(type(content['shapes'])is list and len(content['shapes'])==2,'Breakable native shape count');[b.boolean(v)for v in content['shapes']]
 return doc

def protocol(doc,config):
 validate(doc,config);lines=[str(len(config['cases']))]
 def add(values):lines.append(' '.join(str(int(v))if type(v)is bool else str(v)for v in values))
 for ci,case in enumerate(config['cases']):
  owner=config['owners'][case['owner']];add([owner['id'],case['distance'],case['flag']])
  for row in doc['rows'][ci*47:(ci+1)*47]:
   add([row['frame'],row['root_visible'],row['parent'],*row['position'],row['frame_index'],row['sprite_visible'],bool(row['assigned']),row['playing'],row['time']if row['time']is not None else 0xffffffff,row['body_disabled'],row['body_layer'],row['hit_layer'],row['hit_mask'],row['flag'],row['paused'],row['has_content']])
   if row['has_content']:
    c=row['content'];add([c['parent'],*c['position'],c['visible'],c['physics'],c['changing'],*c['start'],c['interactions'],*c['shapes']])
   for key in ['rumble','audio','coins']:
    add([len(row[key])])
    for request in row[key]:add(request)
   if row['frame']==45:add([row['pending']])
 return ('\n'.join(lines)+'\n').encode()

def build(out):
 review=json.loads(REVIEW.read_bytes(),object_pairs_hook=b.unique);b.fields(review,'schema source producer_sha256 native_producer_sha256 fixture_protocol_sha256 results mapping unverified','Breakable review');_,model,source=b.source();files,copied,expected=native.source();b.require(type(review['schema'])is int and review['schema']==1 and review['source']==source and review['producer_sha256']==hashlib.sha256(Path(b.__file__).read_bytes()).hexdigest()and review['native_producer_sha256']==hashlib.sha256(Path(native.__file__).read_bytes()).hexdigest()and review['fixture_protocol_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest()and type(review['results'])is list and len(review['results'])==2,'Breakable reviewed source/producers');docs=[]
 for row in review['results']:
  b.fields(row,'path sha256','Breakable frozen run');b.text(row['path']);folder=(ROOT/row['path']).resolve();b.require(folder.is_relative_to(ROOT/'reports/breakables')and not folder.is_symlink(),'Breakable frozen path escaped');raw=(folder/'reference.json').read_bytes();receipt=json.loads((folder/'receipt.json').read_bytes(),object_pairs_hook=b.unique);b.require(hashlib.sha256(raw).hexdigest()==row['sha256']and receipt['result_sha256']==row['sha256']and receipt['producer_sha256']==review['native_producer_sha256']and all(receipt.get(k)==v for k,v in expected.items()),'Breakable frozen source/observer/bytes');docs.append(validate(json.loads(raw,object_pairs_hook=b.unique),json.loads(files['input.json'])))
 b.require(docs[0]==docs[1],'Breakable original runs differ');outputs={'breakables.encbrk':b.encode(model),'native.tsv':protocol(docs[0],json.loads(files['input.json']))};out=Path(out).resolve();marker=out/'.owned-breakable-fixture';b.require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and(not out.exists()or marker.is_file()and not marker.is_symlink()and marker.read_bytes()==b'breakable-fixture-v1'),'Owned Breakable fixture required')
 for name,raw in outputs.items():p=out/name;b.require(not p.exists()or p.is_file()and not p.is_symlink()and p.read_bytes()==raw,'Breakable fixture cache changed')
 out.mkdir(parents=True,exist_ok=True);marker.write_bytes(b'breakable-fixture-v1')
 for name,raw in outputs.items():(out/name).write_bytes(raw)
 print('Checked65 Breakable owners and two282-state original provider runs;not new native execution');return outputs

if __name__=='__main__':
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--out',type=Path,required=True);build(ap.parse_args().out)
