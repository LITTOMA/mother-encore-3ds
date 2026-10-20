"""Query all 64 routes between the eight audited player animation states.

Both full original topology and the retained subset run in the real engine
with inert child animations. No player method, physics or audio is exercised.
Imported method fingerprints identify fixture inputs, not executed coverage.
"""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_swing_pause_reference as pause
from tools.run_scene_reference import invoke

def run(work,reports,engine):
 work=work.resolve();reports=reports.resolve();engine=engine.resolve()
 if work.exists()or reports.exists()or not work.is_relative_to(ROOT/'build')or not reports.is_relative_to(ROOT/'reports'):raise ValueError('Fresh owned player path directories required')
 work.mkdir(parents=True);reports.mkdir(parents=True);proof,params=pause.prepare(work)
 original='for destination in ["Idle","FaintedIdle"]:'
 if pause.PROBE.count(original)!=1:raise ValueError('Pause path fixture structure changed')
 script=pause.PROBE.replace(original,'for destination in config.states:');(work/'probe.gd').write_text(script,encoding='utf8')
 config=dict(cases=[],parameters=params,offset=0,states=proof['selected_states']);raw=json.dumps(config);(work/'input.json').write_text(raw,encoding='utf8');(reports/'input.json').write_text(raw,encoding='utf8')
 invoke(engine,work,['--editor','--quit'],reports/'import.log');invoke(engine,work,['--fixed-fps','60','--script',str(work/'probe.gd')],reports/'godot.log')
 actual=json.loads((work/'native.json').read_bytes());expected={a+'->'+b for a in config['states']for b in config['states']}
 if actual['engine']['string']!='3.6.2-stable (official)'or actual['rows']or len(actual['paths'])!=2 or any(set(p)!=expected for p in actual['paths']):raise ValueError('Incomplete original player path query')
 (reports/'reference.json').write_text(json.dumps(actual,indent=2)+'\n',encoding='utf8')
 differences={key:dict(full=actual['paths'][0][key],subset=actual['paths'][1][key])for key in expected if actual['paths'][0][key]!=actual['paths'][1][key]}
 closed=not differences and all(type(path)is list and len(path)<len(config['states'])and all(node in config['states']for node in path)for path in actual['paths'][0].values())
 sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
 proof.update(scope=__doc__,engine=actual['engine'],engine_sha256=sha(engine),routes_each=len(expected),closed=closed,differences=differences,imported_methods_not_executed=True,tools={p:sha(ROOT/p)for p in ['tools/run_player_state_paths_reference.py','tools/run_swing_pause_reference.py','tools/run_swing_entry_reference.py','tools/run_swing_action_reference.py']},probe_sha256=sha(work/'probe.gd'),outputs={p.name:sha(p)for p in reports.iterdir()if p.is_file()})
 (reports/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8')
 if not closed:raise ValueError('Audited player subset is not closed under original travel paths; inspect actual differences')
 print('Original player paths:',len(expected),'routes in both full and subset topology; all closed')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--work',type=Path,required=True);p.add_argument('--reports',type=Path,required=True);p.add_argument('--engine',type=Path,required=True);o=p.parse_args();run(o.work,o.reports,o.engine)
