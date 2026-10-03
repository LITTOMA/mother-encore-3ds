#!/usr/bin/env python3
"""Refresh the reviewed synthetic A/B fixture against the current pack schema.

Preserves the existing seven changes and behavior expectations, validates their
baseline source values, compiles using an environment with no C++ toolchain,
and executes the same already-built runtime probe before/after content work.
Never changes shipping source IR, the runtime or runtime expectations.
"""
import argparse,copy,hashlib,json,os,re,subprocess,sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def require(value,message):
 if not value:raise ValueError(message)
def prepare(source,plan):
 a=copy.deepcopy(source);b=copy.deepcopy(source)
 require(len(plan['changes'])==7,'Unreviewed number of data-only changes')
 for change in plan['changes']:
  section=change['section'];field=change['field']
  if field=='count':
   require(section=='Flag'and change['name']=='registered_flag_count','Unreviewed count change')
   require(len(a['sections'][section])==change['A']and change['B']==change['A']+1,'Reviewed baseline flag count changed')
   flag=plan['added_flag'];require(flag['name']=='data_only_probe_flag'and flag['index']==len(b['sections']['Flag'])and flag['default']is False,'Unreviewed synthetic flag')
   require(flag['stable_id']not in {x['stable_id']for x in b['sections']['Flag']}and flag['name']not in b['strings'],'Synthetic flag identity collision')
   b['strings'].append(flag['name']);b['sections']['Flag'].append(dict(stable_id=flag['stable_id'],name_string=len(b['strings'])-1,default_value=0,flags=0))
  else:
   index=change['index'];match=re.fullmatch(r'([a-z0-9_]+)(?:\[([0-9]+)\])?',field);require(match,'Unreviewed field path')
   name,component=match.groups();av=a['sections'][section][index][name]
   if component is None:
    require(av==change['A'],'Reviewed baseline changed: '+change['name']);b['sections'][section][index][name]=change['B']
   else:
    component=int(component);require(av[component]==change['A'],'Reviewed baseline changed: '+change['name']);b['sections'][section][index][name][component]=change['B']
 require(a['sections']['Program']==b['sections']['Program'],'Synthetic proof changed program graph')
 require(a['rules']==b['rules']and a['capabilities']==b['capabilities'],'Synthetic proof changed rules')
 return a,b

def main():
 p=argparse.ArgumentParser();p.add_argument('--dir',type=Path,default=ROOT/'reports/data-only-proof-victory');p.add_argument('--probe',type=Path,required=True);args=p.parse_args()
 directory=args.dir.resolve();probe=args.probe.resolve();source_path=ROOT/'content/native-opening.json'
 plan=json.loads((directory/'expected.json').read_text());source=json.loads(source_path.read_text());a,b=prepare(source,plan)
 before=digest(probe);old={'room_rules_revision':plan['room_rules_revision'],'room_capability_revision':plan['room_capability_revision'],'packages':copy.deepcopy(plan['packages'])}
 # Python is absolute. An empty PATH and disabled compiler variables establish
 # that this step cannot accidentally regenerate a C++ content header/program.
 env=dict(os.environ,PATH=str(directory/'nonexistent-toolchain'),CC='/bin/false',CXX='/bin/false',DEVKITPRO='',DEVKITARM='')
 for name,ir in [('A',a),('B',b)]:
  src=directory/(name+'.json');out=directory/(name+'.encroom');manifest=directory/(name+'-manifest.json')
  src.write_text(json.dumps(ir,indent=2,ensure_ascii=False)+'\n')
  command=[sys.executable,str(ROOT/'tools/native_content.py'),'compile','--input',str(src),'--out',str(out),'--manifest',str(manifest)]
  result=subprocess.run(command,capture_output=True,text=True,env=env,cwd=ROOT)
  (directory/(name+'-compile.log')).write_text(result.stdout+result.stderr);require(result.returncode==0,'Synthetic data compile failed: '+result.stdout+result.stderr)
  compiled=json.loads(manifest.read_text());require(compiled['cpp_compiler_invoked']is False,'Compiler invocation not allowed')
  plan['packages'][name]={'path':out.relative_to(ROOT).as_posix(),'bytes':out.stat().st_size,'sha256':digest(out)}
 plan.update(compiler_sha256=digest(ROOT/'tools/native_content.py'),room_rules_revision=source['rules'],room_capability_revision=source['capabilities'],runtime_probe_status='Verified by one unchanged executable; see runtime-proof.json')
 (directory/'expected.json').write_text(json.dumps(plan,indent=2)+'\n')
 run=subprocess.run([sys.executable,str(ROOT/'tools/check_data_only.py'),'--probe',str(probe),'--dir',str(directory)],capture_output=True,text=True,env=env,cwd=ROOT)
 (directory/'probe.log').write_text(run.stdout+run.stderr);require(run.returncode==0,'Same-executable proof failed: '+run.stdout+run.stderr)
 after=digest(probe);require(before==after,'Probe executable changed during data-only generation')
 report={'schema':1,'source_ir_sha256':digest(source_path),'scope':'Original seven synthetic data changes and nine runtime observations preserved while refreshing fixture to current Room schema','legacy_package_metadata':old,'current_package_metadata':plan['packages'],'changes':plan['changes'],'behavior_probe_expectations':plan['behavior_probe_expectations'],'executable_sha256_before':before,'executable_sha256_after':after,'same_executable':True,'cpp_compiler_invoked':False,'compiler_environment':{k:env[k]for k in ['PATH','CC','CXX','DEVKITPRO','DEVKITARM']}}
 (directory/'regeneration-proof.json').write_text(json.dumps(report,indent=2)+'\n');print(run.stdout.strip())
 return 0
if __name__=='__main__':raise SystemExit(main())
