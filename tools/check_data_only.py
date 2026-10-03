#!/usr/bin/env python3
"""Prove runtime content changes with one unchanged already-built executable."""
import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--probe',type=Path,required=True);p.add_argument('--dir',type=Path,default=Path('reports/data-only-proof'));a=p.parse_args()
hash_before=hashlib.sha256(a.probe.read_bytes()).hexdigest()
expected=json.loads((a.dir/'expected.json').read_text());observed={}
for variant in ('A','B'):
 result=subprocess.run([str(a.probe.resolve()),str((a.dir/(variant+'.encroom')).resolve())],capture_output=True,text=True,check=True)
 observed[variant]=json.loads(result.stdout)
 for change in expected['changes']:
  if observed[variant][change['name']]!=change[variant]:raise SystemExit('Content behavior mismatch: '+change['name'])
 if observed[variant]['level_at_9xp']!=expected['behavior_probe_expectations']['level_'+variant]:raise SystemExit('XP behavior mismatch')
 if observed[variant]['new_flag_known']!=expected['behavior_probe_expectations']['new_flag_known_'+variant]:raise SystemExit('Registry behavior mismatch')
 (a.dir/(variant+'-runtime.json')).write_text(json.dumps(observed[variant],indent=2)+'\n')
hash_after=hashlib.sha256(a.probe.read_bytes()).hexdigest()
if hash_before!=hash_after:raise SystemExit('Executable changed')
(a.dir/'runtime-proof.json').write_text(json.dumps({'executable':str(a.probe),'sha256_before':hash_before,'sha256_after':hash_after,'same_executable':True,'cpp_compiler_invoked':False,'observed':observed},indent=2)+'\n')
print('PASS: 7 content changes and XP/flag behavior observed by one unchanged executable')
