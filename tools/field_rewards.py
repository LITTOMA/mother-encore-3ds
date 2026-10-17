"""Checked ordinary item definitions appended by admitted field encounters."""
import copy,csv,io,json,shlex,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import bridge_entry
from tools.extract_battle_entry import Extractor,require,PIN
POLICY=ROOT/'content/field-rewards.json';RECEIPT=ROOT/'content/asset-receipts/graphics/ui/field-rewards/source.json'
def load():
 p=bridge_entry.read(POLICY);require(set(p)=={'schema','commit','items'}and type(p['schema'])is int and p['schema']==1 and p['commit']==PIN,'Field reward policy schema/pin');ex=Extractor(ROOT)
 for row in p['items']:
  require(set(row)==set('definition source document texture output winflag storage'.split()),'Field reward fields');require(ex.yaml('Data/Items/'+row['source']+'.yaml')==row['document'],'Changed field item source')
  require(type(row['definition'])is int and row['definition']>0 and type(row['storage'])is int and row['storage']==0,'Field reward stable identity/policy')
  entry=bridge_entry.policy();enemy=ex.yaml('Data/Battlers/'+entry['battler']+'.yaml');programme=ex.yaml(entry['programme'])
  battles=[phrase['startbattle']for phrase in programme.values()if 'startbattle'in phrase]
  require(len(battles)==1 and battles[0]['winflag']==row['winflag']and any(item['item']==row['source']for item in enemy['items']),'Field reward is not bound to its actual encounter/win flag')
  d=row['document'];require(not d['keyitem']and not d['actions']and not d['slot']and not d['HPrecover']and not d['PPrecover']and not any(d['boost'].values())and not d['transform'],'Unreviewed field reward mechanism')
  ex.data(row['texture']);ex.data(row['texture']+'.import');ex.text('LICENSE')
 return p,ex
def receipt():
 p,ex=load();r=bridge_entry.read(RECEIPT);require(r['commit']==PIN and r['policy_sha256']==bridge_entry.sha(POLICY)and r['producer_sha256']==bridge_entry.sha(__file__)and r['sources']==ex.sources,'Stale field reward receipt')
 for path,digest in r['outputs'].items():require(bridge_entry.sha(ROOT/'romfs'/path)==digest,'Changed field reward texture')
 return r
def append_item(ex,ir):
 p,source=load();r=receipt();out=copy.deepcopy(ir);table={x['key']:x for x in csv.DictReader(io.StringIO(source.text('Translations/TranslatedText/items - sheet.csv')))}
 for row in p['items']:
  d=row['document'];require(row['definition']==len(out['definitions'])+1,'Field reward stable prefix changed');size=source.png_size(row['texture']);icon=len(out['resources'])
  out['resources'].append(dict(id=max(x['id']for x in out['resources'])+1,path=row['output'],kind=1,width=size[0],height=size[1],columns=1,rows=1,sha256=r['outputs'][row['output']]))
  out['definitions'].append(dict(id=row['definition'],source=row['source'],name=table[d['name']]['en'],description=table[d['description']]['en'].replace('\\n','\n'),icon=icon,equipment_slot=0xffffffff,heal_hp=0,heal_pp=0,max_hp_boost=0,max_pp_boost=0,flags=0,can_use=0))
 for path in (POLICY,RECEIPT):out['dependencies'][path.relative_to(ROOT).as_posix()]=bridge_entry.sha(path)
 out['scope']+='; ordinary bridge loot carrying and descriptions; source battle use remains disabled'
 return out
def stage_files(source_root=ROOT/'romfs'):
 r=receipt();return {Path(p):(source_root/p).read_bytes()for p in r['outputs']}
def main():
 import argparse
 a=argparse.ArgumentParser();a.add_argument('action',choices=('assets','verify'));a.add_argument('--tex3ds',type=Path);a.add_argument('--tex3ds-command');args=a.parse_args()
 if args.action=='verify':receipt();return
 p,ex=load();require(args.tex3ds and args.tex3ds.is_file(),'Real tex3ds required');outputs={}
 for row in p['items']:
  target=ROOT/'romfs'/row['output'];target.parent.mkdir(parents=True,exist_ok=True);source=ex.upstream/row['texture'];rel=lambda p:p.resolve().relative_to(ROOT).as_posix()
  command=shlex.split(args.tex3ds_command)+['-f','rgba8','-z','none','-o',rel(target),rel(source)]if args.tex3ds_command else[str(args.tex3ds),'-f','rgba8','-z','none','-o',str(target),str(source)]
  subprocess.run(command,cwd=ROOT,check=True);outputs[row['output']]=bridge_entry.sha(target)
 bridge_entry.write(RECEIPT,dict(schema=1,commit=PIN,policy_sha256=bridge_entry.sha(POLICY),producer_sha256=bridge_entry.sha(__file__),sources=ex.sources,outputs=outputs,tex3ds_sha256=bridge_entry.sha(args.tex3ds),permission='Original game-related derivative art under the reviewed upstream LICENSE; no font or third-party binary copied.'))
if __name__=='__main__':main()
