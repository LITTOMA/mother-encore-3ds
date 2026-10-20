"""Private contact metadata fixture; the audio row is a codec double, not PCM."""
import argparse,copy,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_props as p,field_swing as swing,audio_asset as audio

def build(out):
 out=Path(out).resolve();assert(ROOT/'build').resolve()in out.parents
 out.mkdir(parents=True,exist_ok=True)
 policy=dict(p.read(),schema=3,contact_recipe='content/field-swing.json',scope=__doc__)
 recipe=out/'policy.json';recipe.write_bytes((json.dumps(policy,indent=2)+'\n').encode())
 data,clips,proof=p.source(recipe)
 (out/'source.encprops').write_bytes(p.encode(data,clips,capability=3))
 (out/'source.encswing').write_bytes(swing.encode(swing.source(ROOT/policy['contact_recipe'])))
 bank=audio.parse_bank((ROOT/'romfs/sound/banks/opening.encaudio').read_bytes())
 (out/'unrelated.encaudio').write_bytes(audio.build_bank([bank['assets'][0]],bank['master_db'],bank['silence_db']))
 # AudioBank joins require exact source path and digest. Frame data here is
 # deliberately inherited from a codec double; this test never plays it.
 record=copy.deepcopy(bank['assets'][0]);record.update(source_path='res://'+data['policy']['sound'],source_sha256=data['policy']['contact']['sound_sha256'],loop=False,loop_start=0)
 (out/'matching.encaudio').write_bytes(audio.build_bank([record],bank['master_db'],bank['silence_db']))
 wrong=copy.deepcopy(record);wrong['source_path']='res://Audio/wrong-source-codec-fixture.mp3'
 (out/'wrong-path.encaudio').write_bytes(audio.build_bank([wrong],bank['master_db'],bank['silence_db']))
 wrong=copy.deepcopy(record);wrong['source_sha256']='00'+wrong['source_sha256'][2:]
 (out/'wrong-digest.encaudio').write_bytes(audio.build_bank([wrong],bank['master_db'],bank['silence_db']))
 record['loop']=True;record['loop_start']=0
 (out/'looping.encaudio').write_bytes(audio.build_bank([record],bank['master_db'],bank['silence_db']))
 (out/'source.json').write_bytes((json.dumps(dict(data=data,proof=proof),indent=2)+'\n').encode())
 print('Source contact fixture: %d rows, source action and metadata-only audio doubles'%len(data['props']))

if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);build(ap.parse_args().out)
