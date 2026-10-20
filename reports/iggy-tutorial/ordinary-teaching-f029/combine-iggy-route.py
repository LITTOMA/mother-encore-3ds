from pathlib import Path
import json
r=Path(__file__).resolve().parents[2];a=json.loads((r/'build/full-port/iggy-walk-plan-2.json').read_bytes());b=json.loads((r/'build/full-port/iggy-walk-plan-3.json').read_bytes());join=[1280,4625]
i=a['waypoints'].index(join);j=b['waypoints'].index(join);points=a['waypoints'][:i]+b['waypoints'][j:]
assert points[0]==[8,-39]and points[-1]==[1300,4713]
for first,last in zip(points,points[1:]):assert(first[0]==last[0])!=(first[1]==last[1])
p=dict(scope='Ordinary-input validated plan2 prefix plus source-map plan3 corridor from1280,4625. Only the exact source-admitted programme6 destination Area may be entered; guest stops on that event. Static geometry plan, not guest execution proof.',expanded=b['expanded'],grid=4,waypoints=points)
(r/'build/full-port/iggy-walk-plan-4.json').write_text(json.dumps(p,indent=2)+'\n',encoding='utf8');print(len(points),'corners; final',points[-11:])
