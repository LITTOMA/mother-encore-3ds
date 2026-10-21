"""Compile independent damage data and source-observed command/state fixture."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import world_damage as g
REVIEW=ROOT/'compatibility/reviews/world-damage-v0410.json'

def build(out):
 out=Path(out).resolve();marker=out/'.owned-world-damage-fixture';g.require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and(not out.exists()or marker.is_file()and not marker.is_symlink()and marker.read_bytes()==b'world-damage-fixture-v1'),'Owned world damage fixture under build required');review=json.loads(REVIEW.read_bytes(),object_pairs_hook=g.unique);g.fields(review,'schema source producer_sha256 native_producer_sha256 results mapping unverified','world damage semantic review');_,model,source=g.source();producer=hashlib.sha256(Path(g.__file__).read_bytes()).hexdigest();native=hashlib.sha256((ROOT/'tools/run_world_damage_reference.py').read_bytes()).hexdigest();g.require(type(review['schema'])is int and review['schema']==1 and review['source']==source and review['producer_sha256']==producer and review['native_producer_sha256']==native and type(review['results'])is list and len(review['results'])==2,'World damage review/source/producer changed');docs=[];folders=[]
 for row in review['results']:
  g.fields(row,'path sha256','world damage result');g.text(row['path']);folder=(ROOT/row['path']).resolve();g.require(folder.is_relative_to(ROOT/'reports/world-damage')and not folder.is_symlink(),'World damage frozen result escaped');raw=(folder/'reference.json').read_bytes();receipt=json.loads((folder/'receipt.json').read_bytes(),object_pairs_hook=g.unique);g.require(hashlib.sha256(raw).hexdigest()==row['sha256']and receipt['result_sha256']==row['sha256']and receipt['producer_sha256']==native and receipt['source']['checked_source']==source,'World damage frozen receipt differs');proof=receipt['source']
  for name,h in proof['generated_sha256'].items():p=(folder/'observer'/name).resolve();g.require(p.is_relative_to(folder/'observer')and not p.is_symlink()and hashlib.sha256(p.read_bytes()).hexdigest()==h,'World damage frozen observer differs')
  doc=json.loads(raw,object_pairs_hook=g.unique);g.require(set(doc)=={'schema','godot','rows'}and type(doc['schema'])is int and doc['schema']==1 and doc['godot'].get('string')=='3.6.2-stable (official)'and len(doc['rows'])==361,'World damage source result schema/coverage');docs.append(doc);folders.append(folder)
 g.require(docs[0]==docs[1],'World damage original observations differ');rows=docs[0]['rows'];cases=json.loads((folders[0]/'observer/cases.json').read_bytes());g.require(len(cases)==7,'World damage native direct case coverage');ids={c['name']:c['id']for c in model['clips']};lines=['12'];seen=set();events={'camera':1,'vibration':2,'hurt':4,'stat':5,'show':6,'update':7,'number':8,'status':9,'game_over':10}
 def observed(row):
  expected={'label','hp','status','current','assigned','playing','time','invulnerable','main_alpha','special_alpha','main_modifier','main_color','events','properties'};g.require(set(row)in[expected,expected|{'rng_next'}]and type(row['hp'])is int and type(row['playing'])is bool and type(row['invulnerable'])is bool and row['assigned']in ids and row['current']in['',row['assigned']]and row['current']==(row['assigned']if row['playing']else''),'World damage typed observed state');g.require(row['label']not in seen,'World damage duplicate observed label');seen.add(row['label']);g.require(type(row['time'])is int and 0<=row['time']<=0xffffffff and len(row['properties'])==9 and all(type(v)is list and len(v)==4 and all(type(n)is int and 0<=n<=0xffffffff for n in v)for v in row['properties']),'World damage property words');lines.append(' '.join(map(str,[row['hp'],ids[row['assigned']],int(row['playing']),row['time'],int(row['invulnerable']),row.get('rng_next',-1)]+[v for prop in row['properties']for v in prop])));lines.append(str(len(row['status'])))
  for s in row['status']:g.require(set(s)<=set(['status','passive_healing_turns'])and s['status']in{a['id']for a in model['statuses']},'World damage observed status');lines.append(s['status']+' '+str(s.get('passive_healing_turns',0)))
  commands=[]
  for v in row['events']:
   g.require(type(v)is list and v and v[0]in events,'World damage unknown observed command');kind=events[v[0]]
   if kind==1:g.require(len(v)==4 and len(v[3])==2,'World damage camera command');values=[v[1],v[2],*v[3]]
   elif kind==2:g.require(len(v)==5,'World damage vibration command');values=v[1:]
   elif kind==4:g.require(v==['hurt','res://'+model['strings'][1],model['strings'][2]],'World damage hurt binding');commands.append([3,model['integers'][0]]);values=[]
   elif kind==5:g.require(len(v)==4 and v[1]=='hp','World damage HP signal');values=v[2:]
   elif kind in[6,7]:values=[int(a)for a in v[1:]]
   elif kind==8:g.require(len(v)==4 and len(v[2])==2 and type(v[3])is list and len(v[3])==2 and all(type(w)is int and 0<=w<=0xffffffff for w in v[3]),'World damage original flying-number words');distance=struct.unpack('<d',struct.pack('<2I',*v[3]))[0];values=[v[1],*v[2],distance]
   else:g.require(len(v)==1,'World damage marker command');values=[]
   commands.append([kind,*values])
  lines.append(str(len(commands)))
  for command in commands:lines.append(' '.join(map(str,command+[0.]*(9-len(command)))))
 def block(hp,maxhp,status,ops):
  lines.append(' '.join(map(str,[hp,maxhp,status,1,len(ops)])))
  for operation,row in ops:lines.append(operation);observed(row)
 for case,row in zip(cases,rows[:7]):g.require(row['label']==case['name'],'World damage direct case order');block(case['hp'],case['maxhp'],case.get('status','-'),[('1 %s %s %s'%(case['damage'],case['variance'],int(not case['kill'])),row)])
 for scenario in ['complete','pause_zero','pause_positive','restart']:
  ops=[]
  for row in rows:
   if not row['label'].startswith(scenario+':'):continue
   label=row['label'].split(':')[1]
   if label in['play','replay']:op='2 '+str(model['integers'][0])
   elif label=='pause':op='4'
   elif label=='resume':op='5'
   elif label=='manual_paused':op='3 0.1'
   else:g.require(label.isdigit(),'World damage clock operation');op='3 '+repr(1/60)
   ops.append((op,row))
  block(100,100,'-',ops)
 ops=[];paused=False
 for row in rows:
  if not row['label'].startswith('continuous:'):continue
  label=row['label'].split(':')[1]
  if label=='start':op='6 4 2 '+next(s['id']for s in model['statuses']if s['flags']&2)
  elif label=='pause':op='4';paused=True
  elif label=='resume':op='5';paused=False
  elif label=='stop':op='8'
  else:g.require(label.isdigit(),'World damage continuous operation');op='7 '+str(int(paused))+' '+repr(1/60)
  ops.append((op,row))
 block(15,20,'-',ops);g.require(len(seen)==len(rows),'World damage unowned native observations');outputs={'world-damage.encwdm':g.encode(model),'native.tsv':('\n'.join(lines)+'\n').encode()}
 for name,b in outputs.items():p=out/name;g.require(not p.exists()or p.is_file()and not p.is_symlink()and p.read_bytes()==b,'World damage cached fixture changed')
 out.mkdir(parents=True,exist_ok=True);marker.write_bytes(b'world-damage-fixture-v1')
 for name,b in outputs.items():(out/name).write_bytes(b)
 print('Checked world damage resource and361 native observations;not new native execution');return outputs
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);build(a.parse_args().out)
