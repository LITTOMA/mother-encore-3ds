#!/usr/bin/env python3
"""Compare normalized callback ownership; original real-time tween deltas excluded."""
import json
from pathlib import Path
r=Path(__file__).resolve().parents[1]/'reports/source-reference'
original=json.loads((r/'reference.json').read_text());native=json.loads((r/'native.json').read_text())
assert len(original)==len(native)==9
for a,b in zip(original,native):
    assert a['label']==b['label'] and a['areas']==b['areas'],(a,b)
    for v in a['voices']: v.pop('volume')
    for v in b['voices']: v.pop('volume')
    assert a['voices']==b['voices'],(a,b)
result={'passed':True,'cases':9,'equal':'Ordered source callback labels, registered areas, ordered track identity and playing state','excluded':'Original snapshots use real idle deltas, native callback probe advances no tween time. Continuous gain timing is not compared here; focused native tests separately cover reviewed quartic-in/out and linear midpoint equations.'}
(r/'comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
