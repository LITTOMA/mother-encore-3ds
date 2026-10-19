from pathlib import Path
r=Path(__file__).resolve().parents[2];p=r/'tools/mick_treats.py';raw=p.read_bytes();s=raw.decode('utf8');first=s.index('    children = {n.path: [] for n in order}');last=s.index('    events = []',first);old=s[first:last];assert'    state = rng.state'in old and old.count('    ready(\'.\')')==1
nl='\r\n'if'\r\n'in s else'\n';new=nl.join(['    from tools.field_ready_rng import replay as replay_ready','    _, ready_result = replay_ready(ex, order, READY_SCRIPTS)',"    state = ready_result['state']",''])
p.write_bytes((s[:first]+new+s[last:]).encode('utf8'));print('Factored existing ready replay only; viewport event generation unchanged')
