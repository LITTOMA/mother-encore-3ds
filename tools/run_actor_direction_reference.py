"""Original Actor direction writes while a source turn coroutine is suspended."""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import reference_actor_actions as ref
from tools.run_scene_reference import invoke

def run(work,reports,engine):
 work=work.resolve();reports=reports.resolve();engine=engine.resolve()
 if work.exists()or reports.exists()or not work.is_relative_to(ROOT/'build')or not reports.is_relative_to(ROOT/'reports'):raise ValueError('Fresh owned work/report directories required')
 source=ROOT/'tests/fixtures/actor-direction-cases.json';cases=json.loads(source.read_bytes());review=json.loads(ref.REVIEW.read_bytes())
 if set(cases)!={'schema','scope','cases'}or type(cases['schema'])is not int or cases['schema']!=1:raise ValueError('Direction reference domain rejected')
 ref.prepare(ROOT/'upstream/MOTHER-Encore',work,review)
 (work/'cases.json').write_text(json.dumps(cases['cases']),encoding='utf8');reports.mkdir(parents=True)
 invoke(engine,work,['--fixed-fps','60','--script',str(work/'probe.gd'),'--encore-out='+str(reports/'reference.json')],reports/'godot.log')
 doc=json.loads((reports/'reference.json').read_bytes())
 if doc['sources']!=review['sources']or doc['symbols']!=review['function_sha256']or doc['godot']['string']!='3.6.2-stable (official)'or[c['definition']for c in doc['cases']]!=cases['cases']:raise ValueError('Direction source/engine/reference coverage differs')
 rows=[]
 for i,c in enumerate(doc['cases']):
  if len(c['frames'])!=c['definition']['ticks']:raise ValueError('Incomplete direction observation')
  for tick,f in enumerate(c['frames']):rows.append('\t'.join(map(str,[i,tick,*f['direction'],int(f['rotating']),f['action_signals']])))
 (reports/'reference.tsv').write_text('\n'.join(rows)+'\n',encoding='utf8')
 sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
 proof=dict(scope=cases['scope'],engine_sha256=sha(engine),source_cases_sha256=sha(source),sources=doc['sources'],symbols=doc['symbols'],observations=len(rows),reports_sha256={p.name:sha(p)for p in reports.iterdir()if p.is_file()},tools_sha256={p.relative_to(ROOT).as_posix():sha(p)for p in[Path(__file__).resolve(),ROOT/'tools/reference_actor_actions.py',ROOT/'tools/godot_exporter/actor_actions_probe.gd',ROOT/'tools/godot_exporter/actor_actions_wrapper.gd']})
 (reports/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Original suspended Actor direction:',len(rows),'observations; isolated methods only')
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--work',type=Path,required=True);a.add_argument('--reports',type=Path,required=True);a.add_argument('--godot',type=Path,required=True);o=a.parse_args();run(o.work,o.reports,o.godot)
