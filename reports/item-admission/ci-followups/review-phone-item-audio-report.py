from pathlib import Path
import copy,hashlib,json,sys
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root))
from tools import link_phone_content as l
out=root/'build/full-port/phone-item-audio-report-1';out.mkdir(exist_ok=False)
path=root/l.REPORT;prior=path.read_bytes();before=json.loads(prior)
room=json.loads((root/'content/native-opening.json').read_bytes());house=json.loads((root/'content/native-house.json').read_bytes());audio=l.build_audio(root,room)
assert audio==json.loads((root/'content/native-audio.json').read_bytes()) and len(audio['assets'])==61
after=l.report(root,room,house,audio);expected=copy.deepcopy(before)
added=audio['assets'][60];source=added['source_path'].removeprefix('res://')
assert added['stable_id']==1801 and source not in expected['sources'] and source+'.import'not in expected['sources']
expected['sources'][source]=added['source_sha256'];expected['sources'][source+'.import']=added['import_sha256']
expected['outputs']['content/native-audio.json']=hashlib.sha256((root/'content/native-audio.json').read_bytes()).hexdigest()
assert after==expected
(out/'previous-report.json').write_bytes(prior)
l.write(path,after)
record=dict(schema=1,producer_sha256=hashlib.sha256(Path(l.__file__).read_bytes()).hexdigest(),
 old_sha256=hashlib.sha256(prior).hexdigest(),new_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
 changed_fields=['sources.'+source,'sources.'+source+'.import','outputs.content/native-audio.json'],
 linked_audio_equals_production=True,phone_programs_unchanged=True,
 scope='Actual original report producer. Only newly admitted audio source/import and full bank provenance added; no phone graph, command, binding, room or game resource change.')
(out/'source-review.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8');print(json.dumps(record))
